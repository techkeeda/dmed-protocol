# API Reference — C Library (`dmed.h`)

Single-header library. Add to your project:
```c
#define DMED_IMPLEMENTATION  // in exactly ONE .c file
#include "dmed.h"
```

---

## Constants

| Name | Value | Description |
|------|-------|-------------|
| `DMED_VERSION_MAJOR` | 0 | Library major version |
| `DMED_VERSION_MINOR` | 1 | Library minor version |
| `DMED_PROTOCOL_VERSION` | 1 | Wire protocol version |
| `DMED_BEACON_MIN_SIZE` | 6 | Minimum beacon bytes (no optionals) |
| `DMED_BEACON_MAX_SIZE` | 9 | Maximum beacon bytes (all fields) |

## Flags

| Name | Value | Meaning |
|------|-------|---------|
| `DMED_FLAG_AUTH` | 0x01 | Server requires authentication |
| `DMED_FLAG_TLS` | 0x02 | MCP session is encrypted |
| `DMED_FLAG_MULTI` | 0x04 | Multiple tools available |
| `DMED_FLAG_CLOUD` | 0x08 | Backed by internet service |

## Service Types

| Name | Value | Category |
|------|-------|----------|
| `DMED_ST_UNKNOWN` | 0x00 | Unknown |
| `DMED_ST_IOT_DEVICE` | 0x01 | IoT Device |
| `DMED_ST_MEDIA` | 0x02 | Media |
| `DMED_ST_APPLIANCE` | 0x03 | Appliance |
| `DMED_ST_VEHICLE` | 0x04 | Vehicle |
| `DMED_ST_RETAIL_KIOSK` | 0x05 | Retail/Kiosk |
| `DMED_ST_INFRASTRUCTURE` | 0x06 | Infrastructure |
| `DMED_ST_COMPUTING` | 0x07 | Computing |
| `DMED_ST_AI_SERVICE` | 0x08 | AI Service |
| `DMED_ST_DATA_SOURCE` | 0x09 | Data Source |
| `DMED_ST_TOOL_UTILITY` | 0x0A | Tool/Utility |
| `DMED_ST_COMMUNICATION` | 0x0B | Communication |
| `DMED_ST_HEALTH_MEDICAL` | 0x0C | Health/Medical |
| `DMED_ST_INDUSTRIAL` | 0x0D | Industrial |
| `DMED_ST_ENVIRONMENTAL` | 0x0E | Environmental |
| `DMED_ST_SECURITY` | 0x0F | Security |
| `DMED_ST_INFORMATION` | 0x10 | Information |
| `DMED_ST_ENERGY` | 0x11 | Energy |
| `DMED_ST_CUSTOM` | 0xFF | Custom |

---

## Structures

### `dmed_beacon_t`

```c
typedef struct {
    uint8_t  version;       // Protocol version (1-15)
    uint8_t  flags;         // DMED_FLAG_* bitmask
    uint8_t  service_type;  // DMED_ST_* value
    uint32_t instance_id;   // Unique server identifier
    int8_t   tx_power;      // Transmit power in dBm
    uint16_t name_hash;     // CRC-16 of server name
    uint8_t  has_tx_power;  // 1 if tx_power is valid
    uint8_t  has_name_hash; // 1 if name_hash is valid
} dmed_beacon_t;
```

---

## Functions

### `dmed_beacon_encode`

```c
int dmed_beacon_encode(const dmed_beacon_t *beacon, uint8_t *buf, size_t buf_len);
```

Encode a beacon struct into binary wire format.

**Parameters:**
| Param | Description |
|-------|-------------|
| `beacon` | Pointer to populated beacon struct |
| `buf` | Output buffer (must be ≥ 6 bytes, recommend 9) |
| `buf_len` | Size of output buffer |

**Returns:** Number of bytes written (6-9), or negative `dmed_error_t` on failure.

**Example:**
```c
dmed_beacon_t b = {.version=1, .flags=DMED_FLAG_MULTI, .service_type=DMED_ST_IOT_DEVICE,
                  .instance_id=0xA1B2C3D4};
uint8_t buf[9];
int len = dmed_beacon_encode(&b, buf, sizeof(buf));  // len = 6
```

---

### `dmed_beacon_decode`

```c
int dmed_beacon_decode(const uint8_t *buf, size_t buf_len, dmed_beacon_t *beacon);
```

Decode binary beacon data into a struct.

**Parameters:**
| Param | Description |
|-------|-------------|
| `buf` | Input buffer containing beacon bytes |
| `buf_len` | Number of bytes in buffer |
| `beacon` | Output struct (zeroed then populated) |

**Returns:** `DMED_OK` (0) on success, or negative `dmed_error_t`.

**Example:**
```c
dmed_beacon_t decoded;
int rc = dmed_beacon_decode(buf, len, &decoded);
if (rc == DMED_OK) {
    printf("Found: %s (id=%08X)\n",
           dmed_service_type_str(decoded.service_type), decoded.instance_id);
}
```

---

### `dmed_crc16`

```c
uint16_t dmed_crc16(const uint8_t *data, size_t len);
```

Compute CRC-16/CCITT hash. Used for `name_hash` field.

**Parameters:**
| Param | Description |
|-------|-------------|
| `data` | Input bytes |
| `len` | Number of bytes |

**Returns:** 16-bit CRC value.

**Example:**
```c
uint16_t hash = dmed_crc16((uint8_t*)"Kitchen Light", 13);
beacon.name_hash = hash;
beacon.has_name_hash = 1;
```

---

### `dmed_instance_id_from_string`

```c
uint32_t dmed_instance_id_from_string(const char *str);
```

Generate a stable 32-bit instance ID from any string (uses CRC-32).

**Parameters:**
| Param | Description |
|-------|-------------|
| `str` | Null-terminated string (serial number, MAC, UUID, etc.) |

**Returns:** 32-bit instance ID.

**Example:**
```c
uint32_t id = dmed_instance_id_from_string("device-serial-ABC123");
beacon.instance_id = id;
```

---

### `dmed_mdns_txt_encode`

```c
int dmed_mdns_txt_encode(const dmed_beacon_t *beacon, const char *name,
                        const char *mcp_path, const char *card_path,
                        char *buf, size_t buf_len);
```

Format beacon fields into null-separated key=value pairs for mDNS TXT records.

**Parameters:**
| Param | Description |
|-------|-------------|
| `beacon` | Beacon struct with fields to encode |
| `name` | Human-readable server name (or NULL) |
| `mcp_path` | MCP endpoint path, e.g. "/mcp" |
| `card_path` | Card endpoint path, e.g. "/dmed/card" |
| `buf` | Output buffer |
| `buf_len` | Buffer size |

**Returns:** Total bytes written (including null separators), or negative error.

---

### `dmed_service_type_str`

```c
const char *dmed_service_type_str(uint8_t st);
```

Convert service type code to string. Returns `"unknown"` for unrecognized codes.

---

### `dmed_service_type_from_str`

```c
uint8_t dmed_service_type_from_str(const char *str);
```

Convert string to service type code. Returns `DMED_ST_UNKNOWN` for unrecognized strings.

---

## Error Codes

| Name | Value | Meaning |
|------|-------|---------|
| `DMED_OK` | 0 | Success |
| `DMED_ERR_BUFFER_TOO_SMALL` | -1 | Output buffer too small |
| `DMED_ERR_INVALID_DATA` | -2 | NULL pointer or malformed input |
| `DMED_ERR_INVALID_VERSION` | -3 | Beacon version is 0 or unsupported |

---

## Memory & Performance

- **Zero heap allocation** — all functions use caller-provided buffers
- **No global state** — fully reentrant, thread-safe
- **Code size** — ~2KB compiled (ARM Cortex-M: ~1.5KB)
- **Stack usage** — max ~32 bytes per function call
