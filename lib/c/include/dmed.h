/*
 * DMED — Dynamic MCP Endpoint Discovery Protocol
 * C Library Header (dmed.h) — v0.2.0
 *
 * Single-header library. Zero heap allocation for beacon encode/decode.
 * Designed for embedded (ESP32, STM32, Linux, macOS).
 *
 * v0.2: Added action request/response structures for lightweight interaction.
 *
 * Usage:
 *   #define DMED_IMPLEMENTATION
 *   #include "dmed.h"
 */

#ifndef DMED_H
#define DMED_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─── Version ─── */
#define DMED_VERSION_MAJOR 0
#define DMED_VERSION_MINOR 2
#define DMED_VERSION_PATCH 0
#define DMED_PROTOCOL_VERSION 1

/* ─── Beacon ─── */
#define DMED_BEACON_MIN_SIZE 6
#define DMED_BEACON_MAX_SIZE 9

/* Flags */
#define DMED_FLAG_AUTH   (1 << 0)
#define DMED_FLAG_TLS    (1 << 1)
#define DMED_FLAG_MULTI  (1 << 2)
#define DMED_FLAG_CLOUD  (1 << 3)

/* Service types */
#define DMED_ST_UNKNOWN        0x00
#define DMED_ST_IOT_DEVICE     0x01
#define DMED_ST_MEDIA          0x02
#define DMED_ST_APPLIANCE      0x03
#define DMED_ST_VEHICLE        0x04
#define DMED_ST_RETAIL_KIOSK   0x05
#define DMED_ST_INFRASTRUCTURE 0x06
#define DMED_ST_COMPUTING      0x07
#define DMED_ST_AI_SERVICE     0x08
#define DMED_ST_DATA_SOURCE    0x09
#define DMED_ST_TOOL_UTILITY   0x0A
#define DMED_ST_COMMUNICATION  0x0B
#define DMED_ST_HEALTH_MEDICAL 0x0C
#define DMED_ST_INDUSTRIAL     0x0D
#define DMED_ST_ENVIRONMENTAL  0x0E
#define DMED_ST_SECURITY       0x0F
#define DMED_ST_INFORMATION    0x10
#define DMED_ST_ENERGY         0x11
#define DMED_ST_CUSTOM         0xFF

/* Action status codes */
#define DMED_ACTION_OK         0
#define DMED_ACTION_ERR_UNKNOWN   -1
#define DMED_ACTION_ERR_PARAMS    -2
#define DMED_ACTION_ERR_INTERNAL  -3

/* ─── Structures ─── */

typedef struct {
    uint8_t  version;       /* 1-15 */
    uint8_t  flags;         /* DMED_FLAG_* bitmask */
    uint8_t  service_type;  /* DMED_ST_* */
    uint32_t instance_id;
    int8_t   tx_power;      /* dBm, valid if has_tx_power */
    uint16_t name_hash;     /* CRC-16, valid if has_name_hash */
    uint8_t  has_tx_power;
    uint8_t  has_name_hash;
} dmed_beacon_t;

/* v0.2: Action request/response for lightweight interaction */
typedef struct {
    const char *action;     /* Tool/action name to invoke */
    const char *params_json;/* JSON string of parameters (or NULL) */
} dmed_action_request_t;

typedef struct {
    int status;             /* DMED_ACTION_OK or DMED_ACTION_ERR_* */
    const char *action;     /* Echoed action name */
    char result[1024];      /* Result text */
    char error[256];        /* Error message if status != OK */
} dmed_action_response_t;

typedef enum {
    DMED_OK = 0,
    DMED_ERR_BUFFER_TOO_SMALL = -1,
    DMED_ERR_INVALID_DATA = -2,
    DMED_ERR_INVALID_VERSION = -3,
} dmed_error_t;

/* ─── Beacon API ─── */

int dmed_beacon_encode(const dmed_beacon_t *beacon, uint8_t *buf, size_t buf_len);
int dmed_beacon_decode(const uint8_t *buf, size_t buf_len, dmed_beacon_t *beacon);
uint16_t dmed_crc16(const uint8_t *data, size_t len);
uint32_t dmed_instance_id_from_string(const char *str);
int dmed_mdns_txt_encode(const dmed_beacon_t *beacon, const char *name,
                        const char *mcp_path, const char *card_path,
                        char *buf, size_t buf_len);
const char *dmed_service_type_str(uint8_t st);
uint8_t dmed_service_type_from_str(const char *str);

/* ─── v0.2: Action API ─── */

/**
 * Format an action request as JSON into buf.
 * Returns bytes written or negative error.
 */
int dmed_action_format_request(const dmed_action_request_t *req, char *buf, size_t buf_len);

/**
 * Parse an action response from JSON.
 * Returns DMED_OK on success.
 */
int dmed_action_parse_response(const char *json, size_t json_len, dmed_action_response_t *resp);

#ifdef __cplusplus
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════ */
/* IMPLEMENTATION                                                             */
/* ═══════════════════════════════════════════════════════════════════════════ */

#ifdef DMED_IMPLEMENTATION

int dmed_beacon_encode(const dmed_beacon_t *beacon, uint8_t *buf, size_t buf_len) {
    if (!beacon || !buf) return DMED_ERR_INVALID_DATA;
    size_t needed = DMED_BEACON_MIN_SIZE;
    if (beacon->has_tx_power) needed++;
    if (beacon->has_name_hash) needed += 2;
    if (buf_len < needed) return DMED_ERR_BUFFER_TOO_SMALL;

    buf[0] = ((beacon->version & 0x0F) << 4) | (beacon->flags & 0x0F);
    buf[1] = beacon->service_type;
    buf[2] = (beacon->instance_id >> 24) & 0xFF;
    buf[3] = (beacon->instance_id >> 16) & 0xFF;
    buf[4] = (beacon->instance_id >> 8) & 0xFF;
    buf[5] = beacon->instance_id & 0xFF;

    size_t pos = 6;
    if (beacon->has_tx_power) {
        buf[pos++] = (uint8_t)beacon->tx_power;
    }
    if (beacon->has_name_hash) {
        buf[pos++] = (beacon->name_hash >> 8) & 0xFF;
        buf[pos++] = beacon->name_hash & 0xFF;
    }
    return (int)pos;
}

int dmed_beacon_decode(const uint8_t *buf, size_t buf_len, dmed_beacon_t *beacon) {
    if (!buf || !beacon) return DMED_ERR_INVALID_DATA;
    if (buf_len < DMED_BEACON_MIN_SIZE) return DMED_ERR_BUFFER_TOO_SMALL;

    memset(beacon, 0, sizeof(*beacon));
    beacon->version = (buf[0] >> 4) & 0x0F;
    beacon->flags = buf[0] & 0x0F;
    beacon->service_type = buf[1];
    beacon->instance_id = ((uint32_t)buf[2] << 24) | ((uint32_t)buf[3] << 16) |
                          ((uint32_t)buf[4] << 8) | buf[5];

    if (beacon->version == 0 || beacon->version > 15)
        return DMED_ERR_INVALID_VERSION;

    if (buf_len >= 7) {
        beacon->tx_power = (int8_t)buf[6];
        beacon->has_tx_power = 1;
    }
    if (buf_len >= 9) {
        beacon->name_hash = ((uint16_t)buf[7] << 8) | buf[8];
        beacon->has_name_hash = 1;
    }
    return DMED_OK;
}

uint16_t dmed_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

uint32_t dmed_instance_id_from_string(const char *str) {
    uint32_t crc = 0xFFFFFFFF;
    while (*str) {
        crc ^= (uint8_t)*str++;
        for (int i = 0; i < 8; i++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

int dmed_mdns_txt_encode(const dmed_beacon_t *beacon, const char *name,
                        const char *mcp_path, const char *card_path,
                        char *buf, size_t buf_len) {
    int written = 0;
    int n;
    #define APPEND(fmt, ...) do { \
        n = snprintf(buf + written, buf_len - written, fmt, ##__VA_ARGS__); \
        if (n < 0 || (size_t)(written + n) >= buf_len) return DMED_ERR_BUFFER_TOO_SMALL; \
        written += n + 1; \
    } while(0)

    APPEND("v=%d", beacon->version);
    APPEND("id=%08x", beacon->instance_id);
    APPEND("st=%02x", beacon->service_type);
    APPEND("fl=%x", beacon->flags);
    if (name) { APPEND("nm=%s", name); }
    if (mcp_path) { APPEND("path=%s", mcp_path); }
    if (card_path) { APPEND("card=%s", card_path); }

    #undef APPEND
    return written;
}

int dmed_action_format_request(const dmed_action_request_t *req, char *buf, size_t buf_len) {
    if (!req || !req->action || !buf) return DMED_ERR_INVALID_DATA;
    int n;
    if (req->params_json) {
        n = snprintf(buf, buf_len, "{\"action\":\"%s\",\"params\":%s}", req->action, req->params_json);
    } else {
        n = snprintf(buf, buf_len, "{\"action\":\"%s\",\"params\":{}}", req->action);
    }
    if (n < 0 || (size_t)n >= buf_len) return DMED_ERR_BUFFER_TOO_SMALL;
    return n;
}

int dmed_action_parse_response(const char *json, size_t json_len, dmed_action_response_t *resp) {
    if (!json || !resp) return DMED_ERR_INVALID_DATA;
    memset(resp, 0, sizeof(*resp));
    /* Minimal JSON parsing — look for "status":"ok" or "status":"error" */
    if (strstr(json, "\"status\":\"ok\"") || strstr(json, "\"status\": \"ok\"")) {
        resp->status = DMED_ACTION_OK;
    } else {
        resp->status = DMED_ACTION_ERR_INTERNAL;
    }
    /* Extract result text if present */
    const char *txt = strstr(json, "\"text\":\"");
    if (txt) {
        txt += 8;
        const char *end = strchr(txt, '"');
        if (end) {
            size_t len = (size_t)(end - txt);
            if (len >= sizeof(resp->result)) len = sizeof(resp->result) - 1;
            memcpy(resp->result, txt, len);
        }
    }
    /* Extract error message if present */
    const char *msg = strstr(json, "\"message\":\"");
    if (msg) {
        msg += 11;
        const char *end = strchr(msg, '"');
        if (end) {
            size_t len = (size_t)(end - msg);
            if (len >= sizeof(resp->error)) len = sizeof(resp->error) - 1;
            memcpy(resp->error, msg, len);
        }
    }
    return DMED_OK;
}

static const struct { uint8_t code; const char *str; } dmed_st_table[] = {
    {0x00, "unknown"}, {0x01, "iot_device"}, {0x02, "media"},
    {0x03, "appliance"}, {0x04, "vehicle"}, {0x05, "retail_kiosk"},
    {0x06, "infrastructure"}, {0x07, "computing"}, {0x08, "ai_service"},
    {0x09, "data_source"}, {0x0A, "tool_utility"}, {0x0B, "communication"},
    {0x0C, "health_medical"}, {0x0D, "industrial"}, {0x0E, "environmental"},
    {0x0F, "security"}, {0x10, "information"}, {0x11, "energy"}, {0xFF, "custom"},
};

const char *dmed_service_type_str(uint8_t st) {
    for (size_t i = 0; i < sizeof(dmed_st_table)/sizeof(dmed_st_table[0]); i++)
        if (dmed_st_table[i].code == st) return dmed_st_table[i].str;
    return "unknown";
}

uint8_t dmed_service_type_from_str(const char *str) {
    for (size_t i = 0; i < sizeof(dmed_st_table)/sizeof(dmed_st_table[0]); i++)
        if (strcmp(dmed_st_table[i].str, str) == 0) return dmed_st_table[i].code;
    return DMED_ST_UNKNOWN;
}

#endif /* DMED_IMPLEMENTATION */
#endif /* DMED_H */
