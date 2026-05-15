# DMED Protocol Specification

## Dynamic MCP Discovery Protocol — Draft v0.1

**Date:** 2026-05-15
**Status:** Draft
**Authors:** Nilesh

---

## 1. Introduction

### 1.1 Purpose

The Dynamic MCP Discovery Protocol (DMED) defines a standard mechanism for discovering MCP-compatible endpoints across multiple network transports. DMED enables a client (typically a mobile app or AI assistant) to automatically find and connect to nearby or reachable services, devices, tools, and information endpoints without prior configuration.

### 1.2 Scope

DMED covers discovery of:
- **Services** — AI tools, APIs, cloud endpoints exposing MCP interfaces
- **Devices** — IoT devices, smart home appliances, embedded systems
- **Information endpoints** — Data sources, knowledge bases, sensor feeds
- **Features/Tools** — Individual capabilities offered by a host (e.g., "control lights", "query inventory")

DMED operates over:
- **Bluetooth Low Energy (BLE)**
- **WiFi / Local Area Network (mDNS/DNS-SD)**
- **Ethernet / Wired LAN (mDNS/DNS-SD)**
- **Internet (DNS TXT, HTTPS well-known)** — for remote/cloud service discovery

### 1.3 Design Principles

1. **Transport-agnostic core** — One logical protocol, multiple physical bindings
2. **Zero-config** — No manual pairing, no URL entry; scan and discover
3. **Lightweight** — Beacon fits in BLE advertisement (31 bytes) or mDNS TXT record
4. **Progressive disclosure** — Beacon → Capability Card → Full MCP session
5. **Secure by default** — Authentication and encryption at session layer
6. **MCP-native** — Discovered endpoints speak standard MCP; DMED is only the discovery layer

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      DMED CLIENT (Mobile App)                  │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │BLE Scanner│  │mDNS/WiFi │  │ Ethernet │  │  Internet│   │
│  │           │  │ Scanner  │  │ Scanner  │  │  Scanner │   │
│  └─────┬────┘  └─────┬────┘  └─────┬────┘  └─────┬────┘   │
│        │              │              │              │         │
│        └──────────────┴──────┬───────┴──────────────┘         │
│                              │                                │
│                    ┌─────────▼─────────┐                     │
│                    │  DMED Discovery     │                     │
│                    │  Manager           │                     │
│                    │                    │                     │
│                    │  - Deduplicate     │                     │
│                    │  - Rank/Filter     │                     │
│                    │  - Cache           │                     │
│                    └─────────┬─────────┘                     │
│                              │                                │
│                    ┌─────────▼─────────┐                     │
│                    │  MCP Session       │                     │
│                    │  Manager           │                     │
│                    └───────────────────┘                     │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                      DMED SERVER (Endpoint)                    │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │BLE Beacon│  │mDNS/WiFi │  │ Ethernet │  │  DNS TXT │   │
│  │Advertiser│  │Advertiser│  │Advertiser│  │ Record   │   │
│  └─────┬────┘  └─────┬────┘  └─────┬────┘  └─────┬────┘   │
│        │              │              │              │         │
│        └──────────────┴──────┬───────┴──────────────┘         │
│                              │                                │
│                    ┌─────────▼─────────┐                     │
│                    │  DMED Advertiser    │                     │
│                    │  Daemon            │                     │
│                    └─────────┬─────────┘                     │
│                              │                                │
│                    ┌─────────▼─────────┐                     │
│                    │  MCP Server        │                     │
│                    │  (tools/resources) │                     │
│                    └───────────────────┘                     │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 Discovery Flow

```
Phase 1: BEACON (passive broadcast)
  Server → broadcasts DMED Beacon on one or more transports
  Client → scans, collects beacons

Phase 2: CAPABILITY CARD (active query)
  Client → connects to server, requests Capability Card
  Server → returns full JSON capability description

Phase 3: MCP SESSION (interaction)
  Client → establishes MCP session using transport info from Capability Card
  Client ↔ Server: standard MCP protocol (tools, resources, prompts)
```

---

## 3. DMED Beacon Format

The beacon is the minimal advertisement broadcast by a DMED server. It must be compact enough to fit in constrained transports (BLE: 31 bytes adv data).

### 3.1 Beacon Fields

| Field | Size | Required | Description |
|-------|------|----------|-------------|
| `version` | 4 bits | Yes | DMED protocol version (0-15) |
| `flags` | 4 bits | Yes | Capability flags (see 3.2) |
| `service_type` | 8 bits | Yes | Category of service (see 3.3) |
| `instance_id` | 32 bits | Yes | Unique instance identifier (random or hash) |
| `tx_power` | 8 bits | No | Transmit power (for proximity estimation) |
| `name_hash` | 16 bits | No | Truncated hash of human-readable name |

**Total minimum: 7 bytes** (fits easily in BLE, mDNS TXT, or any transport)

### 3.2 Flags (4 bits)

| Bit | Meaning |
|-----|---------|
| 0 | `auth_required` — Endpoint requires authentication |
| 1 | `encrypted` — MCP session will be encrypted |
| 2 | `multi_tool` — Endpoint exposes multiple tools |
| 3 | `internet_backed` — Endpoint proxies to cloud service |

### 3.3 Service Type (8 bits)

| Value | Category | Examples |
|-------|----------|----------|
| 0x01 | IoT Device | Smart light, thermostat, sensor |
| 0x02 | Media | TV, speaker, display |
| 0x03 | Appliance | Fridge, washer, oven |
| 0x04 | Vehicle | Car, scooter, EV charger |
| 0x05 | Retail/Kiosk | POS, vending machine, info kiosk |
| 0x06 | Infrastructure | Router, gateway, hub |
| 0x07 | Computing | Server, NAS, desktop |
| 0x08 | AI Service | LLM endpoint, inference server |
| 0x09 | Data Source | Database, API, sensor feed |
| 0x0A | Tool/Utility | Calculator, converter, formatter |
| 0x0B | Communication | Phone system, intercom, radio |
| 0x0C | Health/Medical | Monitor, scale, tracker |
| 0x0D | Industrial | PLC, SCADA, robot |
| 0x0E | Environmental | Weather station, air quality |
| 0x0F | Security | Camera, lock, alarm |
| 0x10 | Information | Directory, knowledge base, signage |
| 0x11-0xFE | Reserved | Future categories |
| 0xFF | Custom | Vendor-defined |

---

## 4. Transport Bindings

### 4.1 BLE Transport

**Advertisement:**
- Service UUID: `0xDMED0` (16-bit, to be registered) or 128-bit UUID `XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX`
- Advertisement data: DMED Beacon (7+ bytes) in Manufacturer Specific Data or Service Data field
- Scan Response: UTF-8 human-readable name (up to 20 chars)

**Capability Card retrieval:**
- GATT Service UUID: `0xDMED1`
- Characteristic: `0xDMED2` (Read) — Returns Capability Card as JSON (may require multiple reads via offset)

**MCP Session:**
- Option A: GATT characteristic for JSON-RPC exchange (low bandwidth)
- Option B: Capability Card provides WiFi/HTTP endpoint for full MCP session (BLE used only for discovery)

### 4.2 WiFi / Ethernet Transport (mDNS/DNS-SD)

**Service Advertisement:**
```
Service Type: _mcp-dmed._tcp
```

**DNS-SD TXT Record fields:**
```
v=1                          # DMED version
id=<instance_id>             # 32-bit hex
st=<service_type>            # hex byte
flags=<flags>                # hex nibble
name=<human readable name>   # UTF-8
path=/mcp                    # MCP endpoint path
card=/dmed/card               # Capability Card URL path
```

**Capability Card retrieval:**
- HTTP GET `http://<host>:<port>/dmed/card`
- Returns: JSON Capability Card

**MCP Session:**
- Standard MCP Streamable HTTP at `http://<host>:<port>/mcp`

### 4.3 Internet Transport (DNS + HTTPS)

**DNS TXT Record:**
```
_dmed.<domain> IN TXT "v=1; url=https://example.com/mcp; st=08; flags=3; name=My AI Service"
```

**Capability Card:**
- HTTPS GET `https://<domain>/.well-known/dmed/card`

**MCP Session:**
- Standard MCP over HTTPS at the advertised URL

---

## 5. Capability Card

The Capability Card is a JSON document returned after initial beacon discovery. It provides full details needed to establish an MCP session.

### 5.1 Schema

```json
{
  "dmed_version": "0.1",
  "instance_id": "a1b2c3d4",
  "name": "Living Room Smart Light",
  "description": "Philips Hue color bulb with brightness and color control",
  "service_type": "iot_device",
  "vendor": {
    "name": "Philips",
    "url": "https://www.philips.com"
  },
  "transports": [
    {
      "type": "ble",
      "address": "AA:BB:CC:DD:EE:FF",
      "mcp_via": "http",
      "mcp_endpoint": null
    },
    {
      "type": "http",
      "url": "http://192.168.1.42:8080/mcp",
      "auth": "none"
    }
  ],
  "auth": {
    "type": "none | api_key | oauth2 | pin",
    "oauth2_url": null,
    "pin_display": "device_screen"
  },
  "capabilities": {
    "tools": [
      {
        "name": "set_brightness",
        "description": "Set light brightness 0-100%"
      },
      {
        "name": "set_color",
        "description": "Set light color by name or hex"
      },
      {
        "name": "get_status",
        "description": "Get current light state"
      }
    ],
    "resources": [],
    "prompts": []
  },
  "metadata": {
    "firmware": "3.2.1",
    "uptime_seconds": 86400,
    "location": "Living Room",
    "tags": ["lighting", "smart-home", "color"]
  }
}
```

### 5.2 Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `dmed_version` | string | Protocol version |
| `instance_id` | string | Matches beacon instance_id |
| `name` | string | Human-readable name |
| `service_type` | string | Category (matches beacon) |
| `transports` | array | Available connection methods |
| `capabilities` | object | Tools, resources, prompts summary |

### 5.3 Optional Fields

| Field | Type | Description |
|-------|------|-------------|
| `description` | string | Longer description |
| `vendor` | object | Manufacturer info |
| `auth` | object | Authentication requirements |
| `metadata` | object | Arbitrary key-value metadata |
| `icon_url` | string | URL to icon/image |
| `expires` | integer | Unix timestamp when card should be re-fetched |

---

## 6. Discovery Manager Behavior

### 6.1 Scanning

The client scans all available transports simultaneously:
- BLE: passive scan for DMED service UUID advertisements
- mDNS: browse for `_mcp-dmed._tcp` services
- Internet: query `_dmed.<domain>` DNS TXT records (for known domains)

### 6.2 Deduplication

The same endpoint may be discovered on multiple transports. The `instance_id` field is used to deduplicate. When the same `instance_id` appears on multiple transports, the client merges them into a single entry with multiple transport options.

### 6.3 Ranking & Filtering

Clients MAY rank discovered endpoints by:
- **Proximity** (BLE RSSI, same subnet)
- **Service type** (user preference filters)
- **Auth complexity** (prefer no-auth for casual interaction)
- **Transport quality** (prefer WiFi over BLE for bandwidth)

### 6.4 Caching

- Beacon data: cache for duration of advertisement interval (BLE) or DNS TTL (mDNS)
- Capability Cards: cache until `expires` field or 5 minutes default
- Disappeared endpoints: remove after 3x missed advertisement intervals

---

## 7. Security

### 7.1 Threat Model

| Threat | Mitigation |
|--------|-----------|
| Rogue beacon (impersonation) | Capability Card served over TLS; optional signed cards |
| Eavesdropping on MCP session | MCP over HTTPS/TLS; BLE encryption |
| Denial of service (beacon flood) | Client-side rate limiting; max discovered endpoints cap |
| Unauthorized tool invocation | Auth field in Capability Card; MCP-level auth |
| Tracking via instance_id | Rotate instance_id periodically; randomize BLE MAC |

### 7.2 Authentication Levels

| Level | Use Case | Mechanism |
|-------|----------|-----------|
| 0 — Open | Public info kiosks, signage | No auth |
| 1 — PIN | Home devices, personal IoT | PIN displayed on device |
| 2 — API Key | Developer tools, services | Pre-shared key |
| 3 — OAuth2 | Cloud services, enterprise | Standard OAuth2 flow |
| 4 — Mutual TLS | High-security industrial | Client + server certificates |

### 7.3 Privacy

- Servers SHOULD rotate `instance_id` if they don't want to be persistently tracked
- BLE advertisements SHOULD use random resolvable private addresses (RPA)
- Clients MUST NOT broadcast their identity during scanning (passive scan only)
- Capability Cards MUST NOT contain PII unless behind authentication

---

## 8. Protocol Versioning

- `version` field in beacon (4 bits: 0-15)
- `dmed_version` string in Capability Card (semver)
- Clients MUST ignore unknown fields (forward compatibility)
- Servers MUST support at least one version back (backward compatibility)

---

## 9. Examples

### 9.1 Smart Home: Discovering a Light Bulb

```
1. Phone BLE scan → sees DMED beacon:
   version=1, flags=0x0, service_type=0x01, instance_id=0xA1B2C3D4

2. Phone connects via BLE GATT, reads Capability Card:
   {name: "Kitchen Light", tools: [set_brightness, set_color, get_status],
    transports: [{type: "http", url: "http://192.168.1.42:8080/mcp"}]}

3. Phone establishes MCP session over WiFi HTTP:
   → tools/call: set_brightness({level: 75})
   ← result: {success: true, brightness: 75}
```

### 9.2 Retail: Coffee Shop Ordering Kiosk

```
1. Phone WiFi mDNS browse → finds _mcp-dmed._tcp service:
   "CoffeeShop-Order-Kiosk" at 10.0.0.5:9000

2. Phone fetches http://10.0.0.5:9000/dmed/card:
   {name: "Joe's Coffee Ordering", service_type: "retail_kiosk",
    tools: [get_menu, place_order, get_order_status], auth: {type: "none"}}

3. User asks AI: "What's on the menu?"
   → MCP tools/call: get_menu()
   ← result: {items: [{name: "Latte", price: 4.50}, ...]}
```

### 9.3 Industrial: Factory Floor Sensor

```
1. Tablet Ethernet mDNS → finds _mcp-dmed._tcp:
   "CNC-Machine-7-Telemetry" at 172.16.0.107:8443

2. Fetches Capability Card (auth required: mutual TLS):
   {name: "CNC Machine 7", service_type: "industrial",
    tools: [get_temperature, get_vibration, get_tool_wear],
    auth: {type: "mtls", cert_url: "https://factory.internal/certs"}}

3. After mTLS handshake, engineer asks:
   "What's the vibration trend on spindle 2?"
   → MCP tools/call: get_vibration({spindle: 2, range: "1h"})
```

### 9.4 Internet: Cloud AI Service

```
1. App queries DNS: _dmed.api.example.com TXT record
   → "v=1; url=https://api.example.com/mcp; st=08; name=Example LLM API"

2. Fetches https://api.example.com/.well-known/dmed/card:
   {name: "Example LLM", service_type: "ai_service",
    tools: [generate_text, summarize, translate],
    auth: {type: "oauth2", oauth2_url: "https://auth.example.com"}}

3. After OAuth2 flow:
   → MCP tools/call: translate({text: "Hello", target: "es"})
   ← result: {translation: "Hola"}
```

---

## 10. Relationship to Existing Standards

| Standard | Relationship to DMED |
|----------|-------------------|
| MCP (Model Context Protocol) | DMED discovers endpoints; MCP is the session protocol |
| A2A (Agent-to-Agent) | Complementary; A2A for agent↔agent, DMED for client→service discovery |
| mDNS/DNS-SD (RFC 6762/6763) | DMED WiFi/Ethernet binding uses mDNS/DNS-SD as transport |
| BLE GATT | DMED BLE binding uses GATT for Capability Card retrieval |
| Matter | Inspiration for multi-transport approach; DMED is AI-focused, Matter is smart-home-focused |
| IETF draft-morrison-mcp-dns-discovery | Complementary; covers internet DNS discovery, DMED adds local/physical |
| UPnP/SSDP | Legacy alternative; DMED is lighter and MCP-native |
| W3C Web of Things | Capability Card inspired by Thing Descriptions |

---

## 11. IANA / Registry Considerations

The following would need registration/allocation:
- BLE Service UUID for DMED (`0xDMED0`, `0xDMED1`) — Bluetooth SIG
- DNS-SD service type `_mcp-dmed._tcp` — IANA
- DNS underscore label `_dmed` — IANA (per RFC 8552)
- Service Type registry (Section 3.3) — maintained by DMED community

---

## 12. Open Questions

1. **Should DMED support NFC tap-to-discover?** (NFC NDEF record containing Capability Card URL)
2. **Should there be a DMED relay/proxy?** (Gateway that aggregates local devices for remote access)
3. **How to handle firmware updates to Capability Cards?** (Versioning, cache invalidation)
4. **Should DMED define a "capability matching" query?** (Client asks "who can control lights?" vs. scanning all)
5. **Rate limiting / anti-spam for public beacons?** (Prevent beacon flooding in dense environments)
6. **Multi-language support in name/description?** (i18n in Capability Card)

---

## Appendix A: Wire Format — BLE Advertisement

```
Byte 0:       [version:4][flags:4]
Byte 1:       [service_type:8]
Bytes 2-5:    [instance_id:32] (big-endian)
Byte 6:       [tx_power:8] (signed int8, dBm)
Bytes 7-8:    [name_hash:16] (optional)
```

Total: 7-9 bytes in BLE AD Structure (type: Service Data or Manufacturer Specific)

## Appendix B: Wire Format — mDNS TXT Record

```
_mcp-dmed._tcp.local. 4500 IN TXT "v=1" "id=a1b2c3d4" "st=01" "fl=0" "name=Kitchen Light" "path=/mcp" "card=/dmed/card"
```

## Appendix C: Wire Format — DNS TXT Record (Internet)

```
_dmed.example.com. 3600 IN TXT "v=1; url=https://example.com/mcp; st=08; flags=3; name=My Service; card=https://example.com/.well-known/dmed/card"
```
