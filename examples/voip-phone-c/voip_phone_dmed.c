/*
 * DMED VoIP Phone Server
 *
 * Makes a VoIP phone discoverable as a DMED endpoint on the local network.
 * Exposes call management, voicemail, DECT handset status, and DND via MCP.
 *
 * Designed for cross-compilation to embedded Linux targets (e.g. ARM/MIPS).
 * No external dependencies beyond POSIX sockets and pthreads.
 *
 * Build (native):
 *   make
 *
 * Build (cross-compile for ARM):
 *   make CROSS=arm-linux-gnueabihf-
 *
 * Run:
 *   ./voip_phone_dmed [port]
 */

#define DMED_IMPLEMENTATION
#include "dmed.h"
#include "dmed_ble.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define DEFAULT_PORT   8080
#define MAX_HTTP       8192
#define INSTANCE_ID    0x766F6970  /* "voip" */

/* ─── Simulated device state ─── */

typedef struct {
    char number[32];
    char caller_id[64];
    char type[12];       /* incoming, outgoing, missed */
    time_t timestamp;
} call_record_t;

typedef struct {
    char from[64];
    int duration_sec;
    int unread;
    time_t timestamp;
} voicemail_t;

static struct {
    int dnd_enabled;
    char dnd_schedule[32];
    int sip_registered;
    char active_call_number[32];
    int in_call;
    char last_dialed[32];
    call_record_t history[20];
    int history_count;
    voicemail_t voicemails[5];
    int vm_count;
    struct { char name[32]; int online; } dect[2];
} state;

static int g_port;

static void init_state(void) {
    memset(&state, 0, sizeof(state));
    state.sip_registered = 1;
    state.dnd_enabled = 0;

    /* Seed some call history */
    state.history_count = 3;
    strcpy(state.history[0].number, "+14155551234");
    strcpy(state.history[0].caller_id, "Alice");
    strcpy(state.history[0].type, "incoming");
    state.history[0].timestamp = time(NULL) - 3600;

    strcpy(state.history[1].number, "+14155555678");
    strcpy(state.history[1].caller_id, "Bob");
    strcpy(state.history[1].type, "missed");
    state.history[1].timestamp = time(NULL) - 7200;

    strcpy(state.history[2].number, "+18005551000");
    strcpy(state.history[2].caller_id, "");
    strcpy(state.history[2].type, "outgoing");
    state.history[2].timestamp = time(NULL) - 1800;
    strcpy(state.last_dialed, "+18005551000");

    /* Voicemails */
    state.vm_count = 1;
    strcpy(state.voicemails[0].from, "Bob (+14155555678)");
    state.voicemails[0].duration_sec = 23;
    state.voicemails[0].unread = 1;
    state.voicemails[0].timestamp = time(NULL) - 7000;

    /* DECT handsets */
    strcpy(state.dect[0].name, "Kitchen Handset");
    state.dect[0].online = 1;
    strcpy(state.dect[1].name, "Bedroom Handset");
    state.dect[1].online = 0;
}

/* ─── Helpers ─── */

static void get_local_ip(char *buf, size_t len) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
    connect(s, (struct sockaddr *)&addr, sizeof(addr));
    struct sockaddr_in local;
    socklen_t slen = sizeof(local);
    getsockname(s, (struct sockaddr *)&local, &slen);
    inet_ntop(AF_INET, &local.sin_addr, buf, len);
    close(s);
}

static void http_respond(int fd, int code, const char *ctype, const char *body, size_t body_len) {
    char hdr[256];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d OK\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        code, ctype, body_len);
    write(fd, hdr, hlen);
    write(fd, body, body_len);
}

/* Simple JSON field extractor: find "key":"value" and return value */
static int json_get_str(const char *json, const char *key, char *out, size_t out_len) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    const char *end = strchr(p, '"');
    if (!end) return -1;
    size_t len = (size_t)(end - p);
    if (len >= out_len) len = out_len - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return (int)len;
}

static int json_get_bool(const char *json, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ') p++;
    if (strncmp(p, "true", 4) == 0) return 1;
    if (strncmp(p, "false", 5) == 0) return 0;
    return -1;
}

static int json_get_int(const char *json, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    while (*p == ' ') p++;
    return atoi(p);
}

/* ─── Tool Handlers ─── */

static int handle_make_call(const char *params, char *out, size_t out_len) {
    char number[32] = "", line[8] = "fxs";
    json_get_str(params, "number", number, sizeof(number));
    json_get_str(params, "line", line, sizeof(line));
    if (number[0] == '\0')
        return snprintf(out, out_len, "Error: 'number' is required");
    if (state.in_call)
        return snprintf(out, out_len, "Error: already in a call with %s", state.active_call_number);
    state.in_call = 1;
    strncpy(state.active_call_number, number, sizeof(state.active_call_number) - 1);
    strncpy(state.last_dialed, number, sizeof(state.last_dialed) - 1);
    return snprintf(out, out_len, "Dialing %s on %s line...", number, line);
}

static int handle_hangup(char *out, size_t out_len) {
    if (!state.in_call)
        return snprintf(out, out_len, "No active call to hang up");
    snprintf(out, out_len, "Call with %s ended", state.active_call_number);
    state.in_call = 0;
    state.active_call_number[0] = '\0';
    return (int)strlen(out);
}

static int handle_redial(char *out, size_t out_len) {
    if (state.last_dialed[0] == '\0')
        return snprintf(out, out_len, "No previous number to redial");
    state.in_call = 1;
    strncpy(state.active_call_number, state.last_dialed, sizeof(state.active_call_number) - 1);
    return snprintf(out, out_len, "Redialing %s...", state.last_dialed);
}

static int handle_get_active_calls(char *out, size_t out_len) {
    if (!state.in_call)
        return snprintf(out, out_len, "{\"active_calls\":[]}");
    return snprintf(out, out_len, "{\"active_calls\":[\"%s\"]}", state.active_call_number);
}

static int handle_get_missed_calls(const char *params, char *out, size_t out_len) {
    int limit = 10;
    if (params) { int l = json_get_int(params, "limit"); if (l > 0) limit = l; }
    int pos = snprintf(out, out_len, "{\"missed_calls\":[");
    int count = 0;
    for (int i = 0; i < state.history_count && count < limit; i++) {
        if (strcmp(state.history[i].type, "missed") != 0) continue;
        if (count > 0) pos += snprintf(out + pos, out_len - pos, ",");
        pos += snprintf(out + pos, out_len - pos,
            "{\"number\":\"%s\",\"caller_id\":\"%s\",\"time\":%ld}",
            state.history[i].number, state.history[i].caller_id,
            (long)state.history[i].timestamp);
        count++;
    }
    pos += snprintf(out + pos, out_len - pos, "],\"count\":%d}", count);
    return pos;
}

static int handle_get_call_history(const char *params, char *out, size_t out_len) {
    int limit = 10;
    char type_filter[12] = "all";
    if (params) {
        int l = json_get_int(params, "limit"); if (l > 0) limit = l;
        json_get_str(params, "type", type_filter, sizeof(type_filter));
    }
    int pos = snprintf(out, out_len, "{\"calls\":[");
    int count = 0;
    for (int i = 0; i < state.history_count && count < limit; i++) {
        if (strcmp(type_filter, "all") != 0 && strcmp(state.history[i].type, type_filter) != 0)
            continue;
        if (count > 0) pos += snprintf(out + pos, out_len - pos, ",");
        pos += snprintf(out + pos, out_len - pos,
            "{\"number\":\"%s\",\"caller_id\":\"%s\",\"type\":\"%s\",\"time\":%ld}",
            state.history[i].number, state.history[i].caller_id,
            state.history[i].type, (long)state.history[i].timestamp);
        count++;
    }
    pos += snprintf(out + pos, out_len - pos, "]}");
    return pos;
}

static int handle_get_voicemail(const char *params, char *out, size_t out_len) {
    int unread_only = 0;
    if (params) { int v = json_get_bool(params, "unread_only"); if (v == 1) unread_only = 1; }
    int pos = snprintf(out, out_len, "{\"voicemails\":[");
    int count = 0;
    for (int i = 0; i < state.vm_count; i++) {
        if (unread_only && !state.voicemails[i].unread) continue;
        if (count > 0) pos += snprintf(out + pos, out_len - pos, ",");
        pos += snprintf(out + pos, out_len - pos,
            "{\"from\":\"%s\",\"duration\":%d,\"unread\":%s}",
            state.voicemails[i].from, state.voicemails[i].duration_sec,
            state.voicemails[i].unread ? "true" : "false");
        count++;
    }
    pos += snprintf(out + pos, out_len - pos, "],\"count\":%d}", count);
    return pos;
}

static int handle_get_registration_status(char *out, size_t out_len) {
    return snprintf(out, out_len,
        "{\"sip_registered\":%s,\"provider\":\"ooma\",\"line\":\"fxs\"}",
        state.sip_registered ? "true" : "false");
}

static int handle_get_dect_handsets(char *out, size_t out_len) {
    int pos = snprintf(out, out_len, "{\"handsets\":[");
    for (int i = 0; i < 2; i++) {
        if (i > 0) pos += snprintf(out + pos, out_len - pos, ",");
        pos += snprintf(out + pos, out_len - pos,
            "{\"name\":\"%s\",\"online\":%s}",
            state.dect[i].name, state.dect[i].online ? "true" : "false");
    }
    pos += snprintf(out + pos, out_len - pos, "]}");
    return pos;
}

static int handle_do_not_disturb(const char *params, char *out, size_t out_len) {
    int enabled = json_get_bool(params, "enabled");
    if (enabled < 0)
        return snprintf(out, out_len, "Error: 'enabled' (boolean) is required");
    state.dnd_enabled = enabled;
    json_get_str(params, "schedule", state.dnd_schedule, sizeof(state.dnd_schedule));
    if (state.dnd_schedule[0])
        return snprintf(out, out_len, "DND %s (schedule: %s)",
            enabled ? "enabled" : "disabled", state.dnd_schedule);
    return snprintf(out, out_len, "DND %s", enabled ? "enabled" : "disabled");
}

/* ─── DMED Card JSON ─── */

static const char *CARD_FMT =
    "{\"dmed_version\":\"0.2.0\",\"instance_id\":\"voip0001\","
    "\"name\":\"Home Phone\","
    "\"description\":\"VoIP phone system with call history, dialing, and DECT handset management\","
    "\"service_type\":\"communication\","
    "\"transports\":[{\"type\":\"http\",\"url\":\"http://%s:%d/mcp\",\"priority\":1}],"
    "\"auth\":{\"type\":\"none\"},"
    "\"capabilities\":{\"tools\":["
    "{\"name\":\"make_call\",\"description\":\"Dial a phone number\"},"
    "{\"name\":\"get_missed_calls\",\"description\":\"Get missed calls\"},"
    "{\"name\":\"get_call_history\",\"description\":\"Get recent call history\"},"
    "{\"name\":\"get_voicemail\",\"description\":\"Get voicemail messages\"},"
    "{\"name\":\"redial\",\"description\":\"Redial last outgoing number\"},"
    "{\"name\":\"get_active_calls\",\"description\":\"Get active calls\"},"
    "{\"name\":\"hangup\",\"description\":\"End the current call\"},"
    "{\"name\":\"get_registration_status\",\"description\":\"Get SIP registration status\"},"
    "{\"name\":\"get_dect_handsets\",\"description\":\"List DECT handsets\"},"
    "{\"name\":\"do_not_disturb\",\"description\":\"Enable/disable DND\"}"
    "],\"resources\":[],\"prompts\":[]},"
    "\"metadata\":{\"location\":\"hall\",\"zone\":\"Flat-B304\",\"model\":\"Ooma SunDial\"},"
    "\"tags\":[\"voip\",\"phone\",\"dect\",\"fxs\",\"smart-home\"]}";

/* ─── Actions endpoint (v0.2) ─── */

static const char *ACTIONS_JSON =
    "{\"actions\":["
    "{\"name\":\"make_call\",\"description\":\"Dial a phone number\",\"params\":{\"type\":\"object\",\"properties\":{\"number\":{\"type\":\"string\"},\"line\":{\"type\":\"string\",\"enum\":[\"fxs\",\"dect1\",\"dect2\"]}},\"required\":[\"number\"]}},"
    "{\"name\":\"get_missed_calls\",\"description\":\"Get missed calls\",\"params\":{\"type\":\"object\",\"properties\":{\"limit\":{\"type\":\"integer\"}}}},"
    "{\"name\":\"get_call_history\",\"description\":\"Get recent call history\",\"params\":{\"type\":\"object\",\"properties\":{\"type\":{\"type\":\"string\",\"enum\":[\"all\",\"incoming\",\"outgoing\",\"missed\"]},\"limit\":{\"type\":\"integer\"}}}},"
    "{\"name\":\"get_voicemail\",\"description\":\"Get voicemail messages\",\"params\":{\"type\":\"object\",\"properties\":{\"unread_only\":{\"type\":\"boolean\"}}}},"
    "{\"name\":\"redial\",\"description\":\"Redial last outgoing number\",\"params\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"get_active_calls\",\"description\":\"Get active calls\",\"params\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"hangup\",\"description\":\"End the current call\",\"params\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"get_registration_status\",\"description\":\"Get SIP registration status\",\"params\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"get_dect_handsets\",\"description\":\"List DECT handsets\",\"params\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"do_not_disturb\",\"description\":\"Enable/disable DND\",\"params\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"schedule\":{\"type\":\"string\"}},\"required\":[\"enabled\"]}}"
    "]}";

/* ─── MCP JSON-RPC Tools List ─── */

static const char *TOOLS_LIST =
    "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"tools\":["
    "{\"name\":\"make_call\",\"description\":\"Dial a phone number\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"number\":{\"type\":\"string\"},\"line\":{\"type\":\"string\",\"enum\":[\"fxs\",\"dect1\",\"dect2\"]}},\"required\":[\"number\"]}},"
    "{\"name\":\"get_missed_calls\",\"description\":\"Get missed calls\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"limit\":{\"type\":\"integer\"}}}},"
    "{\"name\":\"get_call_history\",\"description\":\"Get recent call history\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"type\":{\"type\":\"string\",\"enum\":[\"all\",\"incoming\",\"outgoing\",\"missed\"]},\"limit\":{\"type\":\"integer\"}}}},"
    "{\"name\":\"get_voicemail\",\"description\":\"Get voicemail messages\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"unread_only\":{\"type\":\"boolean\"}}}},"
    "{\"name\":\"redial\",\"description\":\"Redial last outgoing number\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"get_active_calls\",\"description\":\"Get active calls\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"hangup\",\"description\":\"End the current call\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"get_registration_status\",\"description\":\"Get SIP registration status\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"get_dect_handsets\",\"description\":\"List DECT handsets\",\"inputSchema\":{\"type\":\"object\",\"properties\":{}}},"
    "{\"name\":\"do_not_disturb\",\"description\":\"Enable/disable DND\",\"inputSchema\":{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"schedule\":{\"type\":\"string\"}},\"required\":[\"enabled\"]}}"
    "]}}";

/* ─── Dispatch tool call ─── */

static int dispatch_tool(const char *name, const char *params, char *out, size_t out_len) {
    if (strcmp(name, "make_call") == 0) return handle_make_call(params, out, out_len);
    if (strcmp(name, "hangup") == 0) return handle_hangup(out, out_len);
    if (strcmp(name, "redial") == 0) return handle_redial(out, out_len);
    if (strcmp(name, "get_active_calls") == 0) return handle_get_active_calls(out, out_len);
    if (strcmp(name, "get_missed_calls") == 0) return handle_get_missed_calls(params, out, out_len);
    if (strcmp(name, "get_call_history") == 0) return handle_get_call_history(params, out, out_len);
    if (strcmp(name, "get_voicemail") == 0) return handle_get_voicemail(params, out, out_len);
    if (strcmp(name, "get_registration_status") == 0) return handle_get_registration_status(out, out_len);
    if (strcmp(name, "get_dect_handsets") == 0) return handle_get_dect_handsets(out, out_len);
    if (strcmp(name, "do_not_disturb") == 0) return handle_do_not_disturb(params, out, out_len);
    return snprintf(out, out_len, "Unknown tool: %s", name);
}

/* ─── HTTP Request Handler ─── */

static void handle_request(int fd) {
    char req[MAX_HTTP];
    int n = read(fd, req, sizeof(req) - 1);
    if (n <= 0) { close(fd); return; }
    req[n] = '\0';

    char ip[64];
    get_local_ip(ip, sizeof(ip));
    char response[MAX_HTTP];

    /* GET /dmed/card */
    if (strstr(req, "GET /dmed/card")) {
        int len = snprintf(response, sizeof(response), CARD_FMT, ip, g_port);
        http_respond(fd, 200, "application/json", response, len);
    }
    /* GET /dmed/actions */
    else if (strstr(req, "GET /dmed/actions")) {
        http_respond(fd, 200, "application/json", ACTIONS_JSON, strlen(ACTIONS_JSON));
    }
    /* POST /dmed/action (v0.2 lightweight) */
    else if (strstr(req, "POST /dmed/action")) {
        char *body = strstr(req, "\r\n\r\n");
        if (!body) { close(fd); return; }
        body += 4;

        char action[64] = "";
        json_get_str(body, "action", action, sizeof(action));
        if (action[0] == '\0') {
            const char *err = "{\"status\":\"error\",\"message\":\"Missing 'action' field\"}";
            http_respond(fd, 400, "application/json", err, strlen(err));
        } else {
            char result[2048];
            /* Find params object — pass whole body, handlers will extract */
            const char *params = strstr(body, "\"params\":");
            dispatch_tool(action, params ? params + 9 : NULL, result, sizeof(result));
            int len = snprintf(response, sizeof(response),
                "{\"status\":\"ok\",\"action\":\"%s\",\"result\":{\"text\":\"%s\"}}", action, result);
            http_respond(fd, 200, "application/json", response, len);
        }
    }
    /* POST /mcp (full MCP JSON-RPC) */
    else if (strstr(req, "POST /mcp")) {
        char *body = strstr(req, "\r\n\r\n");
        if (!body) { close(fd); return; }
        body += 4;

        int rid = 1;
        /* Extract id */
        const char *idp = strstr(body, "\"id\":");
        if (idp) rid = atoi(idp + 5);

        if (strstr(body, "\"initialize\"")) {
            int len = snprintf(response, sizeof(response),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"protocolVersion\":\"2025-03-26\","
                "\"capabilities\":{\"tools\":{}},\"serverInfo\":{\"name\":\"Home Phone\",\"version\":\"1.0.0\"}}}", rid);
            http_respond(fd, 200, "application/json", response, len);
        }
        else if (strstr(body, "\"notifications/initialized\"")) {
            http_respond(fd, 204, "text/plain", "", 0);
        }
        else if (strstr(body, "\"tools/list\"")) {
            int len = snprintf(response, sizeof(response), TOOLS_LIST, rid);
            http_respond(fd, 200, "application/json", response, len);
        }
        else if (strstr(body, "\"tools/call\"")) {
            char tool_name[64] = "";
            /* Extract tool name from params.name */
            const char *pname = strstr(body, "\"name\":\"");
            /* Skip the method "name" — look for it inside "params" */
            const char *params_start = strstr(body, "\"params\"");
            if (params_start) {
                pname = strstr(params_start, "\"name\":\"");
                if (pname) {
                    pname += 8;
                    const char *end = strchr(pname, '"');
                    if (end) {
                        size_t l = (size_t)(end - pname);
                        if (l >= sizeof(tool_name)) l = sizeof(tool_name) - 1;
                        memcpy(tool_name, pname, l);
                        tool_name[l] = '\0';
                    }
                }
            }
            char result[2048];
            const char *args = strstr(params_start ? params_start : body, "\"arguments\":");
            dispatch_tool(tool_name, args ? args + 12 : NULL, result, sizeof(result));
            int len = snprintf(response, sizeof(response),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"%s\"}]}}",
                rid, result);
            http_respond(fd, 200, "application/json", response, len);
        }
        else {
            int len = snprintf(response, sizeof(response),
                "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}", rid);
            http_respond(fd, 200, "application/json", response, len);
        }
    }
    else {
        const char *msg = "Not found. Try GET /dmed/card";
        http_respond(fd, 404, "text/plain", msg, strlen(msg));
    }
    close(fd);
}

/* ─── Beacon info ─── */

static void print_beacon_info(void) {
    dmed_beacon_t beacon = {
        .version = DMED_PROTOCOL_VERSION,
        .flags = 0,
        .service_type = DMED_ST_COMMUNICATION,
        .instance_id = INSTANCE_ID,
        .has_tx_power = 0,
        .has_name_hash = 0,
    };
    uint8_t buf[DMED_BEACON_MAX_SIZE];
    int len = dmed_beacon_encode(&beacon, buf, sizeof(buf));
    printf("[DMED] Beacon (%d bytes): ", len);
    for (int i = 0; i < len; i++) printf("%02X", buf[i]);
    printf("\n");
    printf("[DMED] Service: %s (0x%02X)\n", dmed_service_type_str(beacon.service_type), beacon.service_type);
}

/* ─── Main ─── */

int main(int argc, char **argv) {
    g_port = argc > 1 ? atoi(argv[1]) : DEFAULT_PORT;
    init_state();

    char ip[64];
    get_local_ip(ip, sizeof(ip));

    printf("\n");
    printf("  ┌──────────────────────────────────────────┐\n");
    printf("  │  DMED VoIP Phone Server                  │\n");
    printf("  │  Model: Ooma SunDial                     │\n");
    printf("  └──────────────────────────────────────────┘\n\n");

    print_beacon_info();
    printf("\n");
    printf("[DMED] ✓ Card:    http://%s:%d/dmed/card\n", ip, g_port);
    printf("[DMED] ✓ Actions: http://%s:%d/dmed/actions\n", ip, g_port);
    printf("[DMED] ✓ MCP:     http://%s:%d/mcp\n", ip, g_port);
    printf("[DMED] ✓ Broadcasting \"Home Phone\" on _dmed._tcp port %d\n\n", g_port);

    /* Start BLE advertisement + GATT server */
    char ble_card[4096];
    snprintf(ble_card, sizeof(ble_card),
        "{\"dmed_version\":\"0.2.0\",\"instance_id\":\"voip0001\","
        "\"name\":\"Home Phone\","
        "\"description\":\"VoIP phone system with call history, dialing, and DECT handset management\","
        "\"service_type\":\"communication\","
        "\"transport\":{\"http\":{\"host\":\"%s\",\"port\":%d}},"
        "\"auth\":{\"type\":\"none\"},"
        "\"capabilities\":{\"tools\":["
        "{\"name\":\"make_call\",\"description\":\"Dial a phone number\"},"
        "{\"name\":\"get_missed_calls\",\"description\":\"Get missed calls\"},"
        "{\"name\":\"get_call_history\",\"description\":\"Get recent call history\"},"
        "{\"name\":\"get_voicemail\",\"description\":\"Get voicemail messages\"},"
        "{\"name\":\"redial\",\"description\":\"Redial last outgoing number\"},"
        "{\"name\":\"get_active_calls\",\"description\":\"Get active calls\"},"
        "{\"name\":\"hangup\",\"description\":\"End the current call\"},"
        "{\"name\":\"get_registration_status\",\"description\":\"Get SIP registration status\"},"
        "{\"name\":\"get_dect_handsets\",\"description\":\"List DECT handsets\"},"
        "{\"name\":\"do_not_disturb\",\"description\":\"Enable/disable DND\"}"
        "],\"resources\":[],\"prompts\":[]},"
        "\"metadata\":{\"location\":\"hall\",\"zone\":\"Flat-B304\",\"model\":\"Ooma SunDial\"},"
        "\"tags\":[\"voip\",\"phone\",\"dect\",\"fxs\",\"smart-home\"]}", ip, g_port);

    if (dmed_ble_start(ble_card, dispatch_tool) == 0) {
        printf("[DMED] ✓ BLE advertising started\n\n");
    } else {
        printf("[DMED] ⚠ BLE advertising failed (continuing with HTTP only)\n\n");
    }

    /* Start HTTP server */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    listen(srv, 8);

    printf("[DMED] Listening on port %d...\n", g_port);

    while (1) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) continue;
        handle_request(client);
    }

    return 0;
}
