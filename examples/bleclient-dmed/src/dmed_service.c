/*
 * dmed_service.c — DMED BLE GATT service implementation
 *
 * Provides capability card (read) and action dispatch (write/read) over BLE.
 */

#include "btclient.h"
#include "dmed_service.h"

#define MAX_ACTION_BUF 2048

static char *g_card_json = NULL;
static uint32_t g_card_len = 0;
static char g_action_rsp[MAX_ACTION_BUF];
static int g_action_rsp_len = 0;

/* ─── DMED Action Dispatcher (simulated VoIP phone) ─── */

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

/* ─── Init: build card JSON ─── */

void dmed_init(void) {
    char hostname[64] = "ooma-device";
    gethostname(hostname, sizeof(hostname));

    g_card_json = g_strdup_printf(
        "{\"dmed_version\":\"0.2.0\",\"instance_id\":\"%s\","
        "\"name\":\"Home Phone\","
        "\"description\":\"VoIP phone with call management over BLE\","
        "\"service_type\":\"communication\","
        "\"auth\":{\"type\":\"none\"},"
        "\"capabilities\":{\"tools\":["
        "{\"name\":\"make_call\",\"description\":\"Dial a phone number\"},"
        "{\"name\":\"get_missed_calls\",\"description\":\"Get missed calls\"},"
        "{\"name\":\"get_call_history\",\"description\":\"Get call history\"},"
        "{\"name\":\"get_voicemail\",\"description\":\"Get voicemail\"},"
        "{\"name\":\"redial\",\"description\":\"Redial last number\"},"
        "{\"name\":\"get_active_calls\",\"description\":\"Get active calls\"},"
        "{\"name\":\"hangup\",\"description\":\"End call\"},"
        "{\"name\":\"get_registration_status\",\"description\":\"SIP status\"},"
        "{\"name\":\"get_dect_handsets\",\"description\":\"List DECT handsets\"},"
        "{\"name\":\"do_not_disturb\",\"description\":\"Toggle DND\"}"
        "]}}",
        hostname);
    g_card_len = strlen(g_card_json);
    btclient_info("DMED card initialized (%u bytes)\n", g_card_len);
}

/* ─── Property getters for GetManagedObjects ─── */

GVariant *dmed_get_card_svc_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_CARD_SVC_UUID);
    g_variant_dict_insert(&dict, "Primary", "b", TRUE);
    return g_variant_dict_end(&dict);
}

GVariant *dmed_get_card_len_chr_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_CARD_LEN_UUID);
    g_variant_dict_insert_value(&dict, "Service", g_variant_new_object_path(DMED_SVC_CARD_PATH));
    const gchar *flags[] = {"read", NULL};
    g_variant_dict_insert_value(&dict, "Flags", g_variant_new_strv(flags, -1));
    return g_variant_dict_end(&dict);
}

GVariant *dmed_get_card_data_chr_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_CARD_DATA_UUID);
    g_variant_dict_insert_value(&dict, "Service", g_variant_new_object_path(DMED_SVC_CARD_PATH));
    const gchar *flags[] = {"read", NULL};
    g_variant_dict_insert_value(&dict, "Flags", g_variant_new_strv(flags, -1));
    return g_variant_dict_end(&dict);
}

GVariant *dmed_get_act_svc_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_ACT_SVC_UUID);
    g_variant_dict_insert(&dict, "Primary", "b", TRUE);
    return g_variant_dict_end(&dict);
}

GVariant *dmed_get_act_req_chr_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_ACT_REQ_UUID);
    g_variant_dict_insert_value(&dict, "Service", g_variant_new_object_path(DMED_SVC_ACT_PATH));
    const gchar *flags[] = {"write", NULL};
    g_variant_dict_insert_value(&dict, "Flags", g_variant_new_strv(flags, -1));
    return g_variant_dict_end(&dict);
}

GVariant *dmed_get_act_rsp_chr_properties(void) {
    GVariantDict dict;
    g_variant_dict_init(&dict, NULL);
    g_variant_dict_insert(&dict, "UUID", "s", DMED_ACT_RSP_UUID);
    g_variant_dict_insert_value(&dict, "Service", g_variant_new_object_path(DMED_SVC_ACT_PATH));
    const gchar *flags[] = {"read", NULL};
    g_variant_dict_insert_value(&dict, "Flags", g_variant_new_strv(flags, -1));
    return g_variant_dict_end(&dict);
}

/* ─── GATT Callbacks ─── */

gboolean dmed_on_card_len_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts)
{
    uint8_t buf[4];
    buf[0] = (g_card_len >> 24) & 0xFF;
    buf[1] = (g_card_len >> 16) & 0xFF;
    buf[2] = (g_card_len >> 8) & 0xFF;
    buf[3] = g_card_len & 0xFF;
    GVariant *val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, buf, 4, 1);
    org_bluez_gatt_characteristic1_complete_read_value(obj, inv, val);
    btclient_info("DMED: card length read (%u)\n", g_card_len);
    return TRUE;
}

gboolean dmed_on_card_data_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts)
{
    guint16 offset = 0;
    if (opts) g_variant_lookup(opts, "offset", "q", &offset);
    uint32_t remaining = (offset < g_card_len) ? g_card_len - offset : 0;
    GVariant *val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
        g_card_json + offset, remaining, 1);
    org_bluez_gatt_characteristic1_complete_read_value(obj, inv, val);
    btclient_info("DMED: card data read (offset=%u, len=%u)\n", offset, remaining);
    return TRUE;
}

gboolean dmed_on_act_req_write(OrgBluezGattCharacteristic1 *obj,
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
    int rlen = dispatch_action(action, params, result, sizeof(result));

    g_action_rsp_len = snprintf(g_action_rsp, sizeof(g_action_rsp),
        "{\"status\":\"ok\",\"action\":\"%s\",\"result\":{\"text\":\"%.*s\"}}",
        action, rlen, result);

    org_bluez_gatt_characteristic1_complete_write_value(obj, inv);
    btclient_info("DMED: action '%s' -> %d bytes\n", action, g_action_rsp_len);
    return TRUE;
}

gboolean dmed_on_act_rsp_read(OrgBluezGattCharacteristic1 *obj,
    GDBusMethodInvocation *inv, GVariant *opts)
{
    guint16 offset = 0;
    if (opts) g_variant_lookup(opts, "offset", "q", &offset);
    int remaining = (offset < g_action_rsp_len) ? g_action_rsp_len - offset : 0;
    GVariant *val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
        g_action_rsp + offset, remaining > 0 ? remaining : 0, 1);
    org_bluez_gatt_characteristic1_complete_read_value(obj, inv, val);
    return TRUE;
}
