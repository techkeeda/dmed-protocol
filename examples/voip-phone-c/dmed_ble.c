/*
 * dmed_ble.c — BLE GATT server for DMED via BlueZ D-Bus API
 *
 * Registers GATT services with bluetoothd:
 *   - Card Service (D14D0001): card length + card data (read)
 *   - Action Service (D14D0004): action request (write), action response (read)
 * Also registers BLE advertisement with DMED beacon UUID.
 */

#include "dmed_ble.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <glib.h>
#include <gio/gio.h>
#include "dbus/objectmanager-generated.h"
#include "dbus/service-generated.h"
#include "dbus/advertise-generated.h"
#include "dbus/characteristic-generated.h"

#define BLUEZ_BUS   "org.bluez"
#define DMED_BUS    "org.dmed.server"
#define APP_PATH    "/org/dmed/server"
#define ADV_PATH    "/org/dmed/server/adv"
#define SVC_CARD_PATH   "/org/dmed/server/svc_card"
#define CHR_LEN_PATH    "/org/dmed/server/svc_card/chr_len"
#define CHR_DATA_PATH   "/org/dmed/server/svc_card/chr_data"
#define SVC_ACT_PATH    "/org/dmed/server/svc_act"
#define CHR_REQ_PATH    "/org/dmed/server/svc_act/chr_req"
#define CHR_RSP_PATH    "/org/dmed/server/svc_act/chr_rsp"

#define DMED_CARD_SVC_UUID  "d14d0001-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_LEN_UUID  "d14d0003-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_DATA_UUID "d14d0002-1000-4000-8000-00805f9b34fb"
#define DMED_ACT_SVC_UUID   "d14d0004-1000-4000-8000-00805f9b34fb"
#define DMED_ACT_REQ_UUID   "d14d0005-1000-4000-8000-00805f9b34fb"
#define DMED_ACT_RSP_UUID   "d14d0006-1000-4000-8000-00805f9b34fb"
#define DMED_BEACON_UUID    "d14d0000-1000-4000-8000-00805f9b34fb"

#define MAX_ACTION_BUF 2048

static const char *g_card_json;
static uint32_t g_card_len;
static char g_action_rsp[MAX_ACTION_BUF];
static int g_action_rsp_len;
static dmed_ble_action_handler_t g_handler;
static GMainLoop *g_loop;
static pthread_t g_thread;
static guint g_bus_id;
static char g_adapter_path[64] = "/org/bluez/hci0";


/* ─── Characteristic read/write callbacks ─── */

static gboolean on_card_len_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts)
{
    uint8_t buf[4];
    buf[0] = (g_card_len >> 24) & 0xFF;
    buf[1] = (g_card_len >> 16) & 0xFF;
    buf[2] = (g_card_len >> 8) & 0xFF;
    buf[3] = g_card_len & 0xFF;
    GVariant *val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buf, 4, 1);
    org_bluez_gatt_characteristic1_complete_read_value(obj, inv, val);
    return TRUE;
}

static gboolean on_card_data_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts)
{
    /* Read offset from options if present */
    guint16 offset = 0;
    if (opts) g_variant_lookup(opts, "offset", "q", &offset);
    guint32 remaining = (offset < g_card_len) ? g_card_len - offset : 0;
    GVariant *val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
        g_card_json + offset, remaining, 1);
    org_bluez_gatt_characteristic1_complete_read_value(obj, inv, val);
    return TRUE;
}

static gboolean on_action_req_write(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *arg_data, GVariant *opts)
{
    gsize len = 0;
    gconstpointer data = g_variant_get_fixed_array(arg_data, &len, 1);

    char buf[MAX_ACTION_BUF];
    gsize clen = len < sizeof(buf)-1 ? len : sizeof(buf)-1;
    memcpy(buf, data, clen);
    buf[clen] = '\0';

    /* Parse action name */
    char action[64] = "";
    const char *p = strstr(buf, "\"action\":\"");
    if (p) {
        p += 10;
        const char *e = strchr(p, '"');
        if (e) { int l = e-p < 63 ? (int)(e-p) : 63; memcpy(action, p, l); action[l]=0; }
    }
    const char *params = strstr(buf, "\"params\":");
    if (params) params += 9;

    char result[1024];
    int rlen = 0;
    if (g_handler)
        rlen = g_handler(action, params, result, sizeof(result));

    g_action_rsp_len = snprintf(g_action_rsp, sizeof(g_action_rsp),
        "{\"status\":\"ok\",\"action\":\"%s\",\"result\":{\"text\":\"%.*s\"}}",
        action, rlen, result);

    org_bluez_gatt_characteristic1_complete_write_value(obj, inv);
    printf("[BLE] Action: %s -> %d bytes response\n", action, g_action_rsp_len);
    return TRUE;
}

static gboolean on_action_rsp_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts)
{
    guint16 offset = 0;
    if (opts) g_variant_lookup(opts, "offset", "q", &offset);
    int remaining = (offset < g_action_rsp_len) ? g_action_rsp_len - offset : 0;
    GVariant *val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
        g_action_rsp + offset, remaining, 1);
    org_bluez_gatt_characteristic1_complete_read_value(obj, inv, val);
    return TRUE;
}

/* ─── GetManagedObjects — returns full GATT hierarchy to bluetoothd ─── */

static GVariant *make_svc_props(const char *uuid) {
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&b, "{sv}", "UUID", g_variant_new_string(uuid));
    g_variant_builder_add(&b, "{sv}", "Primary", g_variant_new_boolean(TRUE));
    return g_variant_builder_end(&b);
}

static GVariant *make_chr_props(const char *uuid, const char *svc_path, const char **flags) {
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&b, "{sv}", "UUID", g_variant_new_string(uuid));
    g_variant_builder_add(&b, "{sv}", "Service", g_variant_new_object_path(svc_path));
    g_variant_builder_add(&b, "{sv}", "Flags", g_variant_new_strv(flags, -1));
    return g_variant_builder_end(&b);
}

static gboolean on_get_managed_objects(OrgFreedesktopDBusObjectManager *obj,
    GDBusMethodInvocation *inv)
{
    GVariantBuilder root;
    g_variant_builder_init(&root, G_VARIANT_TYPE("a{oa{sa{sv}}}"));

    /* Card Service */
    GVariantBuilder ifaces;
    g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&ifaces, "{s@a{sv}}", "org.bluez.GattService1",
        make_svc_props(DMED_CARD_SVC_UUID));
    g_variant_builder_add(&root, "{o@a{sa{sv}}}", SVC_CARD_PATH, g_variant_builder_end(&ifaces));

    /* Card Length Characteristic */
    const char *read_flags[] = {"read", NULL};
    g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&ifaces, "{s@a{sv}}", "org.bluez.GattCharacteristic1",
        make_chr_props(DMED_CARD_LEN_UUID, SVC_CARD_PATH, read_flags));
    g_variant_builder_add(&root, "{o@a{sa{sv}}}", CHR_LEN_PATH, g_variant_builder_end(&ifaces));

    /* Card Data Characteristic */
    g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&ifaces, "{s@a{sv}}", "org.bluez.GattCharacteristic1",
        make_chr_props(DMED_CARD_DATA_UUID, SVC_CARD_PATH, read_flags));
    g_variant_builder_add(&root, "{o@a{sa{sv}}}", CHR_DATA_PATH, g_variant_builder_end(&ifaces));

    /* Action Service */
    g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&ifaces, "{s@a{sv}}", "org.bluez.GattService1",
        make_svc_props(DMED_ACT_SVC_UUID));
    g_variant_builder_add(&root, "{o@a{sa{sv}}}", SVC_ACT_PATH, g_variant_builder_end(&ifaces));

    /* Action Request Characteristic (write) */
    const char *write_flags[] = {"write", NULL};
    g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&ifaces, "{s@a{sv}}", "org.bluez.GattCharacteristic1",
        make_chr_props(DMED_ACT_REQ_UUID, SVC_ACT_PATH, write_flags));
    g_variant_builder_add(&root, "{o@a{sa{sv}}}", CHR_REQ_PATH, g_variant_builder_end(&ifaces));

    /* Action Response Characteristic (read) */
    g_variant_builder_init(&ifaces, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_add(&ifaces, "{s@a{sv}}", "org.bluez.GattCharacteristic1",
        make_chr_props(DMED_ACT_RSP_UUID, SVC_ACT_PATH, read_flags));
    g_variant_builder_add(&root, "{o@a{sa{sv}}}", CHR_RSP_PATH, g_variant_builder_end(&ifaces));

    org_freedesktop_dbus_object_manager_complete_get_managed_objects(obj, inv,
        g_variant_builder_end(&root));
    printf("[BLE] GATT services registered with bluetoothd\n");
    return TRUE;
}

/* ─── Advertisement GetAll properties ─── */

static gboolean on_adv_get_all(OrgFreedesktopDBusProperties *obj,
    GDBusMethodInvocation *inv, const gchar *iface)
{
    if (g_strcmp0(iface, "org.bluez.LEAdvertisement1") != 0) {
        g_dbus_method_invocation_return_error(inv, G_DBUS_ERROR,
            G_DBUS_ERROR_UNKNOWN_INTERFACE, "Unknown interface");
        return TRUE;
    }

    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "Type", "s", "peripheral");

    const gchar *uuids[] = {DMED_BEACON_UUID, DMED_CARD_SVC_UUID, NULL};
    GVariant *uuid_array = g_variant_new_strv(uuids, -1);
    g_variant_dict_insert_value(&dict, "ServiceUUIDs", uuid_array);

    org_freedesktop_dbus_properties_complete_get_all(obj, inv,
        g_variant_dict_end(&dict));
    return TRUE;
}


/* ─── D-Bus bus acquired — export objects and register with BlueZ ─── */

static OrgFreedesktopDBusObjectManager *obj_mgr;
static OrgFreedesktopDBusProperties *adv_props;
static OrgBluezGattCharacteristic1 *chr_len_iface;
static OrgBluezGattCharacteristic1 *chr_data_iface;
static OrgBluezGattCharacteristic1 *chr_req_iface;
static OrgBluezGattCharacteristic1 *chr_rsp_iface;

static void on_register_app_done(GObject *src, GAsyncResult *res, gpointer data) {
    GError *err = NULL;
    GVariant *ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
    if (err) {
        fprintf(stderr, "[BLE] RegisterApplication: %s\n", err->message);
        g_error_free(err);
    } else {
        printf("[BLE] ✓ GATT application registered\n");
        if (ret) g_variant_unref(ret);
    }
}

static void on_register_adv_done(GObject *src, GAsyncResult *res, gpointer data) {
    GError *err = NULL;
    GVariant *ret = g_dbus_connection_call_finish(G_DBUS_CONNECTION(src), res, &err);
    if (err) {
        fprintf(stderr, "[BLE] RegisterAdvertisement: %s\n", err->message);
        g_error_free(err);
    } else {
        printf("[BLE] ✓ BLE advertisement registered\n");
        if (ret) g_variant_unref(ret);
    }
}

static gboolean register_with_bluez_idle(gpointer data) {
    GDBusConnection *conn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    if (!conn) { fprintf(stderr, "[BLE] D-Bus connection failed\n"); return FALSE; }

    /* Empty options dict a{sv} */
    GVariant *empty_opts = g_variant_new_parsed("@a{sv} {}");

    g_dbus_connection_call(conn, BLUEZ_BUS, g_adapter_path,
        "org.bluez.GattManager1", "RegisterApplication",
        g_variant_new("(o@a{sv})", APP_PATH, empty_opts),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
        on_register_app_done, NULL);

    empty_opts = g_variant_new_parsed("@a{sv} {}");
    g_dbus_connection_call(conn, BLUEZ_BUS, g_adapter_path,
        "org.bluez.LEAdvertisingManager1", "RegisterAdvertisement",
        g_variant_new("(o@a{sv})", ADV_PATH, empty_opts),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL,
        on_register_adv_done, NULL);

    g_object_unref(conn);
    return FALSE;
}

static void on_bus_acquired(GDBusConnection *conn, const gchar *name, gpointer data) {
    GError *err = NULL;

    /* Object Manager at APP_PATH */
    obj_mgr = org_freedesktop_dbus_object_manager_skeleton_new();
    g_signal_connect(obj_mgr, "handle-get-managed-objects", G_CALLBACK(on_get_managed_objects), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(obj_mgr), conn, APP_PATH, &err);

    /* Advertisement properties */
    adv_props = org_freedesktop_dbus_properties_skeleton_new();
    g_signal_connect(adv_props, "handle-get-all", G_CALLBACK(on_adv_get_all), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(adv_props), conn, ADV_PATH, &err);

    /* Card Length characteristic */
    chr_len_iface = org_bluez_gatt_characteristic1_skeleton_new();
    g_signal_connect(chr_len_iface, "handle-read-value", G_CALLBACK(on_card_len_read), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(chr_len_iface), conn, CHR_LEN_PATH, &err);

    /* Card Data characteristic */
    chr_data_iface = org_bluez_gatt_characteristic1_skeleton_new();
    g_signal_connect(chr_data_iface, "handle-read-value", G_CALLBACK(on_card_data_read), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(chr_data_iface), conn, CHR_DATA_PATH, &err);

    /* Action Request characteristic (write) */
    chr_req_iface = org_bluez_gatt_characteristic1_skeleton_new();
    g_signal_connect(chr_req_iface, "handle-write-value", G_CALLBACK(on_action_req_write), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(chr_req_iface), conn, CHR_REQ_PATH, &err);

    /* Action Response characteristic (read) */
    chr_rsp_iface = org_bluez_gatt_characteristic1_skeleton_new();
    g_signal_connect(chr_rsp_iface, "handle-read-value", G_CALLBACK(on_action_rsp_read), NULL);
    g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(chr_rsp_iface), conn, CHR_RSP_PATH, &err);

    if (err) fprintf(stderr, "[BLE] export error: %s\n", err->message);
}

static void on_name_acquired(GDBusConnection *conn, const gchar *name, gpointer data) {
    printf("[BLE] D-Bus name acquired: %s\n", name);
    g_idle_add(register_with_bluez_idle, NULL);
}

static void on_name_lost(GDBusConnection *conn, const gchar *name, gpointer data) {
    fprintf(stderr, "[BLE] D-Bus name lost\n");
    if (g_loop) g_main_loop_quit(g_loop);
}

/* ─── Thread running GMainLoop ─── */

static void *ble_thread(void *arg) {
    (void)arg;
    g_loop = g_main_loop_new(NULL, FALSE);

    g_bus_id = g_bus_own_name(G_BUS_TYPE_SYSTEM, DMED_BUS,
        G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT | G_BUS_NAME_OWNER_FLAGS_REPLACE,
        on_bus_acquired, on_name_acquired, on_name_lost, NULL, NULL);

    g_main_loop_run(g_loop);
    g_bus_unown_name(g_bus_id);
    g_main_loop_unref(g_loop);
    g_loop = NULL;
    return NULL;
}

/* ─── Public API ─── */

int dmed_ble_start(const char *card_json, dmed_ble_action_handler_t handler) {
    if (!card_json) return -1;
    g_card_json = card_json;
    g_card_len = (uint32_t)strlen(card_json);
    g_handler = handler;
    g_action_rsp_len = 0;

    if (pthread_create(&g_thread, NULL, ble_thread, NULL) != 0) return -1;
    return 0;
}

void dmed_ble_stop(void) {
    if (g_loop) g_main_loop_quit(g_loop);
    pthread_join(g_thread, NULL);
}
