# DMED Protocol Specification — v0.2 (Draft)

## Dynamic MCP Endpoint Discovery Protocol

**Version:** 0.2  
**Status:** Draft  
**Author:** Nilesh Valmik Ladhe  

---

## What's New in v0.2

Version 0.2 adds the **Interaction Protocol** — a lightweight mechanism for clients to send actions/commands to discovered MCP endpoints over HTTP, completing the full lifecycle:

```
Discover → Connect → Interact → Disconnect
```

---

## 1. Purpose

DMED enables physical devices — appliances, phones, sensors, industrial equipment — to broadcast their capabilities over any hardware transport (BLE, WiFi, Ethernet, Thread/Matter, and others) and be discovered and controlled by AI agents with zero manual configuration.

DMED is structured in three tiers:

1. **Protocol** — The capability card format and action dispatch standard (this document)
2. **Discovery Framework** — A multi-transport SDK that surfaces DMED devices from BLE, mDNS, Ethernet, and more as a unified list (Appendix H)
3. **MCP Gateway** — A local bridge that exposes discovered DMED devices as MCP tools to remote AI agents over the internet (Appendix I)

## 2. Scope

DMED covers:
- **Discovery** — Devices broadcast their presence over any supported transport
- **Connect** — Clients fetch the device's Capability Card to learn its identity and actions
- **Interact** — Clients send lightweight actions or full MCP JSON-RPC sessions
- **Bridge** — Optionally, a local DMED-MCP Gateway makes local devices available to remote AI agents via MCP

## 3. Architecture

### 3.1 Three-Tier Architecture

```
┌──────────────────────────────────────────────────────────────┐
│  Tier 3: DMED-MCP Gateway                    WAN / Internet  │
│  Bridges local DMED devices to MCP                           │
│  See Appendix I                                              │
├──────────────────────────────────────────────────────────────┤
│  Tier 2: DMED Discovery Framework            Local / LAN     │
│  Multi-transport scanner SDK                                 │
│  See Appendix H                                              │
├──────────────────────────────────────────────────────────────┤
│  Tier 1: DMED Protocol                       The Standard    │
│  Capability card format + action dispatch                    │
│  This document                                               │
└──────────────────────────────────────────────────────────────┘
```

### 3.2 Protocol Stack (Tier 1 detail)

```
┌─────────────────────────────────────────────────────────────────┐
│                        DMED Protocol Stack                        │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: Interaction    │ POST /dmed/action (lightweight)       │
│                          │ POST /mcp (full JSON-RPC)             │
├──────────────────────────┼──────────────────────────────────────┤
│  Layer 3: Connect        │ GET /dmed/card (capabilities)         │
│                          │ GET /dmed/actions (available actions)  │
├──────────────────────────┼──────────────────────────────────────┤
│  Layer 2: Discovery      │ mDNS _dmed._tcp / BLE beacon          │
├──────────────────────────┼──────────────────────────────────────┤
│  Layer 1: Transport      │ WiFi / Ethernet / Bluetooth /         │
│                          │ Thread·Matter / Zigbee·Z-Wave /       │
│                          │ USB·Serial (via adapter daemon)        │
└──────────────────────────┴──────────────────────────────────────┘
```

### 3.3 Relationship to MCP

MCP's official roadmap covers internet-scale discovery via `.well-known/mcp.json` Server Cards — finding AI tools on the public web.

DMED fills the local and short-range layer that MCP does not cover:

| | MCP (official) | DMED |
|---|---|---|
| **Scope** | Internet / WAN | Local / BLE / LAN |
| **Discovery** | `.well-known` URL | BLE beacon / mDNS |
| **Device type** | Web services, APIs | Physical devices, embedded |
| **Transport** | HTTPS | Any (BLE, mDNS, Ethernet, …) |
| **Relationship** | Standard | Extends + bridges to MCP |

The DMED-MCP Gateway (Tier 3) is the bridge between local DMED devices and internet-connected AI agents. See Appendix I.

## 4. Protocol Lifecycle

### Phase 1: Discovery (Beacon)

MCP endpoints broadcast their presence:
- **mDNS:** Publish `_dmed._tcp` or `_dmed._tcp` service with TXT records
- **BLE:** Advertise beacon with service type and instance ID

### Phase 2: Connect (Card)

Client fetches the endpoint's Capability Card:

```
GET /dmed/card HTTP/1.1
Host: <endpoint-ip>:<port>
```

Response: Full endpoint manifest with tools, transports, auth requirements.

### Phase 3: Interact (Actions) — NEW in v0.2

#### 3a. List Available Actions

```
GET /dmed/actions HTTP/1.1
Host: <endpoint-ip>:<port>
```

Response:
```json
{
  "actions": [
    {
      "name": "brew_coffee",
      "description": "Brew a coffee with specified type and size",
      "params": {
        "type": "object",
        "properties": {
          "drink_type": {"type": "string", "enum": ["espresso", "latte"]},
          "size": {"type": "string", "enum": ["small", "medium", "large"]}
        },
        "required": ["drink_type", "size"]
      }
    }
  ]
}
```

#### 3b. Send Action (Lightweight)

```
POST /dmed/action HTTP/1.1
Host: <endpoint-ip>:<port>
Content-Type: application/json

{
  "action": "brew_coffee",
  "params": {
    "drink_type": "latte",
    "size": "large"
  }
}
```

Success Response:
```json
{
  "status": "ok",
  "action": "brew_coffee",
  "result": {
    "text": "☕ Your large latte is ready."
  }
}
```

Error Response:
```json
{
  "status": "error",
  "action": "brew_coffee",
  "message": "Water level too low. Please refill."
}
```

#### 3c. Full MCP Session (Optional)

For advanced use cases (streaming, multi-turn, resources), clients can establish a full MCP JSON-RPC session:

```
POST /mcp HTTP/1.1
Content-Type: application/json

{"jsonrpc": "2.0", "id": 1, "method": "tools/call", "params": {"name": "brew_coffee", "arguments": {"drink_type": "latte", "size": "large"}}}
```

### Phase 4: Disconnect

Client simply stops sending requests. No explicit disconnect handshake required for the lightweight action protocol. For full MCP sessions, standard MCP shutdown applies.

## 5. Interaction Protocol Design Principles

1. **Lightweight** — Single HTTP POST, single JSON response. No session state required.
2. **Stateless** — Each action is independent. No connection to maintain.
3. **Self-describing** — `/dmed/actions` tells the client exactly what's available and what params are needed.
4. **Compatible** — Works alongside full MCP. Clients can upgrade from lightweight actions to full MCP sessions.
5. **Transport-agnostic** — Works over any HTTP-capable transport (WiFi, Ethernet, tunneled BLE).

## 6. Endpoint HTTP API Summary

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/dmed/card` | Fetch endpoint capabilities (Card) |
| GET | `/dmed/actions` | List available actions with param schemas |
| POST | `/dmed/action` | Send a lightweight action/command |
| POST | `/mcp` | Full MCP JSON-RPC endpoint |

## 7. Error Codes

| HTTP Status | Meaning |
|-------------|---------|
| 200 | Success |
| 400 | Bad request (missing action field, invalid params) |
| 401 | Unauthorized (auth required) |
| 404 | Unknown action |
| 500 | Internal endpoint error |
| 503 | Endpoint unavailable (booting/shutting down) |

## 8. Authentication

When `auth.type` in the Card is not `"none"`, clients must include credentials:

| Auth Type | Header |
|-----------|--------|
| `token` | `Authorization: Bearer <token>` |
| `api_key` | `X-API-Key: <key>` |
| `oauth2` | `Authorization: Bearer <oauth-token>` |

## 9. Versioning

- Protocol version in beacon: `v=1` (wire format version)
- Spec version in Card/manifest: `dmed_version: "0.2.0"`
- Libraries follow semver independently

## 10. Conformance

A **DMED v0.2 compliant endpoint** MUST:
1. Broadcast via at least one transport (mDNS or BLE)
2. Serve `/dmed/card` with a valid Capability Card (Appendix A schema)
3. Serve `/dmed/actions` listing available actions
4. Accept `POST /dmed/action` and return valid responses
5. Return appropriate HTTP error codes

A **DMED v0.2 compliant client** MUST:
1. Discover endpoints via at least one transport
2. Fetch and parse the Capability Card
3. Be able to send actions via `POST /dmed/action`
4. Handle error responses gracefully

A **DMED v0.2 compliant Discovery Framework** MUST:
1. Support at least two transports simultaneously (e.g. BLE + mDNS)
2. Deduplicate devices by `instance_id` across transports
3. Expose a unified device list regardless of which transport found the device
4. Select transport by `priority` field from the Capability Card
5. See Appendix H for full requirements

A **DMED v0.2 compliant MCP Gateway** MUST:
1. Discover local DMED devices via the Discovery Framework
2. Expose each device action as an MCP tool
3. Proxy MCP tool calls to the physical device and return results
4. Require API key authentication for all WAN-accessible endpoints
5. See Appendix I for full requirements

---

## Appendices

See the `spec/` directory for detailed appendices:

**Protocol (Tier 1)**
- [A: JSON Schema](./spec/appendix-a-json-schema.md) — Normative Capability Card schema
- [B: BLE Transport](./spec/appendix-b-ble-transport.md) — GATT profile, UUID assignments, beacon format
- [C: mDNS Transport](./spec/appendix-c-mdns-transport.md) — DNS-SD service type, TXT record fields
- [D: Internet Transport](./spec/appendix-d-internet-transport.md) — WAN discovery (direct + gateway)
- [E: Service Type Registry](./spec/appendix-e-service-types.md) — Standard service type codes
- [F: Examples](./spec/appendix-f-examples.md) — Worked examples for common devices
- [G: Implementation Guide](./spec/appendix-g-implementation-guide.md) — Step-by-step device implementation

**Discovery Framework (Tier 2)**
- [H: Discovery Framework](./spec/appendix-h-discovery-framework.md) — Multi-transport SDK specification

**MCP Gateway (Tier 3)**
- [I: MCP Gateway](./spec/appendix-i-mcp-gateway.md) — Local-to-WAN bridge specification
