/*
 * dmed_service.c — DMED BLE GATT service using notify pattern
 *
 * Uses the same write-request/notify-response pattern as the existing bleclient:
 * - Client writes JSON request to characteristic
 * - Server sets characteristic value (triggers notification with response)
 *
 * Single characteristic for DMED: write to request, notify for response.
 * Requests: {"cmd":"card"} or {"cmd":"action","action":"make_call","params":{}}
 */

#include "btclient.h"
#include "dmed_service.h"

#define MAX_BUF 2048

static char *g_card_json = NULL;
static uint32_t g_card_len = 0;

/* Reference to our DMED characteristic interface (set from dbus-support.c) */
extern OrgBluezGattCharacteristic1 *dmed_chr_iface;

/* ─── Action Dispatcher ─── */

static int dispatch_action(const char *action, const char *params, char *out, size_t out_len) {
    if (!action || !action[0])
        return snprintf(out, out_len, "Error: no action specified");
    if (strcmp(action, "make_call") == 0)
        return snprintf(out, out_len, "Dialing...");
    if (strcmp(action, "hangup") == 0)
        return snprintf(out, out_len, "Call ended");
    if (strcmp(action, "get_missed_calls") == 0)
        return snprintf(out, out_len, "{\"missed_calls\":[],\"count\":0}");
    if (strcmp(action, "get_call_history") == 0)
        return snprintf(out, out_len, "{\"calls\":[]}");
    if (strcmp(action, "get_voicemail") == 0)
        return snprintf(out, out_len, "{\"voicemails\":[],\"count\":0}");
    if (strcmp(action, "redial") == 0)
        return snprintf(out, out_len, "No previous number");
    if (strcmp(action, "get_active_calls") == 0)
        return snprintf(out, out_len, "{\"active_calls\":[]}");
    if (strcmp(action, "get_registration_status") == 0)
        return snprintf(out, out_len, "{\"sip_registered\":true}");
    if (strcmp(action, "get_dect_handsets") == 0)
        return snprintf(out, out_len, "{\"handsets\":[]}");
    if (strcmp(action, "do_not_disturb") == 0)
        return snprintf(out, out_len, "DND toggled");
    return snprintf(out, out_len, "Unknown action: %s", action);
}

/* ─── Init ─── */

void dmed_init(void) {
    g_card_json = g_strdup("{\"name\":\"Phone\"}");
    g_card_len = strlen(g_card_json);
    btclient_info("DMED card initialized (%u bytes)\n", g_card_len);
}

/* ─── Send response via notification ─── */

static void dmed_send_notify(const char *response) {
    if (!dmed_chr_iface) return;
    GVariant *variant = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
        (gconstpointer)response, strlen(response), sizeof(guint8));
    org_bluez_gatt_characteristic1_set_value(dmed_chr_iface, variant);
    btclient_info("DMED: notify sent (%zu bytes)\n", strlen(response));
}

/* ─── GATT Property Getters ─── */

GVariant *dmed_get_svc_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_CARD_SVC_UUID);
    g_variant_dict_insert(&dict, "Primary", "b", TRUE);
    return g_variant_dict_end(&dict);
}

GVariant *dmed_get_chr_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_CARD_DATA_UUID);
    g_variant_dict_insert_value(&dict, "Service", g_variant_new_object_path(DMED_SVC_CARD_PATH));
    const gchar *flags[] = {"notify", "write", NULL};
    g_variant_dict_insert_value(&dict, "Flags", g_variant_new_strv(flags, -1));
    return g_variant_dict_end(&dict);
}

/* ─── GATT Callbacks ─── */

gboolean dmed_on_write(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *arg_data, GVariant *opts)
{
    gsize len = 0;
    gconstpointer data = g_variant_get_fixed_array(arg_data, &len, 1);

    char buf[MAX_BUF];
    gsize clen = len < sizeof(buf)-1 ? len : sizeof(buf)-1;
    memcpy(buf, data, clen);
    buf[clen] = '\0';

    btclient_info("DMED: write received (%zu bytes): %.50s\n", len, buf);

    /* Parse command */
    char cmd[32] = "";
    const char *p = strstr(buf, "\"cmd\":\"");
    if (p) {
        p += 7;
        const char *e = strchr(p, '"');
        if (e) { int l = e-p < 31 ? (int)(e-p) : 31; memcpy(cmd, p, l); cmd[l]=0; }
    }

    if (strcmp(cmd, "card") == 0) {
        /* Send card as notification */
        dmed_send_notify(g_card_json);
    } else if (strcmp(cmd, "action") == 0) {
        /* Parse action name */
        char action[64] = "";
        p = strstr(buf, "\"action\":\"");
        if (p) {
            p += 10;
            const char *e = strchr(p, '"');
            if (e) { int l = e-p < 63 ? (int)(e-p) : 63; memcpy(action, p, l); action[l]=0; }
        }
        const char *params = strstr(buf, "\"params\":");
        if (params) params += 9;

        char result[1024];
        int rlen = dispatch_action(action, params, result, sizeof(result));

        char response[MAX_BUF];
        snprintf(response, sizeof(response),
            "{\"status\":\"ok\",\"action\":\"%s\",\"result\":{\"text\":\"%.*s\"}}",
            action, rlen, result);
        dmed_send_notify(response);
    } else {
        dmed_send_notify("{\"error\":\"unknown cmd\"}");
    }

    org_bluez_gatt_characteristic1_complete_write_value(obj, inv);
    return TRUE;
}

gboolean dmed_on_start_notify(OrgBluezGattCharacteristic1 *obj, GDBusMethodInvocation *inv) {
    btclient_info("DMED: start notify\n");
    org_bluez_gatt_characteristic1_complete_start_notify(obj, inv);
    return TRUE;
}

gboolean dmed_on_stop_notify(OrgBluezGattCharacteristic1 *obj, GDBusMethodInvocation *inv) {
    btclient_info("DMED: stop notify\n");
    org_bluez_gatt_characteristic1_complete_stop_notify(obj, inv);
    return TRUE;
}
