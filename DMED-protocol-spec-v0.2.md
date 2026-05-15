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

DMED enables automatic discovery of and interaction with MCP endpoints over local networks (WiFi/Ethernet via mDNS, Bluetooth via BLE) and the internet.

## 2. Scope

DMED covers:
- **Discovery** — Finding MCP endpoints broadcasting on the network
- **Connect** — Fetching endpoint capabilities (Card/Manifest)
- **Interact** — Sending lightweight actions/commands to the endpoint
- **Full MCP** — Optional full JSON-RPC MCP session for advanced use

## 3. Architecture

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
│  Layer 1: Transport      │ WiFi / Ethernet / Bluetooth / Internet│
└──────────────────────────┴──────────────────────────────────────┘
```

## 4. Protocol Lifecycle

### Phase 1: Discovery (Beacon)

MCP endpoints broadcast their presence:
- **mDNS:** Publish `_dmed._tcp` or `_mcp-dmed._tcp` service with TXT records
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
2. Serve `/dmed/card` with a valid Capability Card
3. Serve `/dmed/actions` listing available actions
4. Accept `POST /dmed/action` and return valid responses
5. Return appropriate HTTP error codes

A **DMED v0.2 compliant client** MUST:
1. Discover endpoints via at least one transport
2. Fetch and parse the Capability Card
3. Be able to send actions via `POST /dmed/action`
4. Handle error responses gracefully

---

## Appendices

See the `spec/` directory for detailed appendices:
- [A: JSON Schema](./spec/appendix-a-json-schema.md)
- [B: BLE Transport](./spec/appendix-b-ble-transport.md)
- [C: mDNS Transport](./spec/appendix-c-mdns-transport.md)
- [D: Internet Transport](./spec/appendix-d-internet-transport.md)
- [E: Service Type Registry](./spec/appendix-e-service-types.md)
- [F: Examples](./spec/appendix-f-examples.md)
- [G: Implementation Guide](./spec/appendix-g-implementation-guide.md)
