# DMED Architecture

**DMED — Dynamic MCP Endpoint Discovery**
**Author:** Nilesh Ladhe
**Version:** 0.2 (Draft)

---

## Vision

Physical devices — a coffee machine, a VoIP phone, a thermostat, a lock — should be discoverable and controllable by AI agents the same way web APIs are. No pairing apps. No manual configuration. No central registries.

DMED makes a device self-describing. It broadcasts its own capabilities over whatever hardware transport it supports. Any scanner — mobile app, desktop agent, embedded controller — picks that up and can immediately interact with the device. When needed, those same local devices can be bridged to the internet and made available to remote AI agents via the Model Context Protocol (MCP).

```
Physical World                    Local Network / BLE / Ethernet
─────────────────────────────────────────────────────────────────
  [Coffee Machine]  ── mDNS ────────────────────────────────┐
  [VoIP Phone]      ── BLE ─────────────────────────────── │ ──→  [DMED Scanner]
  [Thermostat]      ── Ethernet ───────────────────────────── │       (mobile / desktop)
  [Smart Lock]      ── Thread/Matter ──────────────────────── ┘             │
                                                                             │ DMED Discovery Framework
                                                                             ↓
                                                                    [DMED-MCP Gateway]
                                                                             │ MCP (internet)
                                                                             ↓
                                                                    [Remote Claude / AI Agent]
```

---

## Core Idea: The Capability Card

Every DMED-enabled device broadcasts one thing: a **capability card**. This is a structured JSON document that tells any scanner:

- What the device is (name, description, type)
- What it can do (actions, with input schemas)
- How to reach it (which transports it supports)

```json
{
  "dmed_version": "0.2.0",
  "instance_id": "c0ffee01",
  "name": "Smart Coffee Machine",
  "description": "Control your coffee machine — brew drinks, check status, set temperature",
  "service_type": "iot_device",
  "transports": [
    { "type": "http", "url": "http://192.168.1.42:3100/mcp", "priority": 1 },
    { "type": "ble",  "service_uuid": "D14D0001-1000-4000-8000-00805F9B34FB", "priority": 2 }
  ],
  "auth": { "type": "none" },
  "capabilities": {
    "tools": [
      {
        "name": "brew_coffee",
        "description": "Brew a coffee with specified type and size",
        "inputSchema": {
          "type": "object",
          "properties": {
            "drink_type": { "type": "string", "enum": ["espresso", "americano", "latte", "cappuccino"] },
            "size":       { "type": "string", "enum": ["small", "medium", "large"] }
          },
          "required": ["drink_type", "size"]
        }
      },
      {
        "name": "get_status",
        "description": "Get current machine status including water level, temperature, and bean level",
        "inputSchema": { "type": "object", "properties": {} }
      }
    ]
  },
  "metadata": { "icon": "☕", "category": "iot" }
}
```

A device that broadcasts this card is **DMED-enabled**. There is no registration step, no cloud dependency, no pairing ceremony.

---

## The Three Tiers

DMED is structured in three tiers. Each tier builds on the one below it.

```
┌─────────────────────────────────────────────┐
│  Tier 3: DMED-MCP Gateway                   │  WAN / Internet
│  Bridges local DMED devices to MCP          │
├─────────────────────────────────────────────┤
│  Tier 2: DMED Discovery Framework           │  Local / Short-range
│  Multi-transport scanner SDK                │
├─────────────────────────────────────────────┤
│  Tier 1: DMED Protocol                      │  The Standard
│  Capability card format + action dispatch   │
└─────────────────────────────────────────────┘
```

> Note: The DMED Protocol spec also defines an internal 4-layer protocol stack (Transport → Discovery → Connect → Interact). The tiers here are higher-level architectural components, not protocol layers.

---

## Tier 1 — DMED Protocol

The protocol defines three things: what a capability card looks like, how actions are dispatched, and what the lifecycle of an interaction is.

### Lifecycle

```
Discover → Connect → Interact → Disconnect
```

| Phase | What happens |
|---|---|
| **Discover** | Scanner finds device via transport-specific mechanism |
| **Connect** | Scanner fetches the capability card |
| **Interact** | Scanner calls actions on the device |
| **Disconnect** | Stateless — no teardown needed |

### HTTP Endpoints (when device is on IP network)

| Method | Path | Description |
|---|---|---|
| `GET` | `/dmed/card` | Returns the capability card |
| `GET` | `/dmed/actions` | Lists available actions with schemas |
| `POST` | `/dmed/action` | Dispatches a single action |
| `POST` | `/mcp` | Full MCP JSON-RPC (for richer tool use) |

### Action Dispatch

```json
POST /dmed/action
{ "action": "brew_coffee", "params": { "drink_type": "espresso", "size": "small" } }

→ 200 OK
{ "status": "ok", "action": "brew_coffee", "result": { "text": "☕ Your small espresso is ready!" } }
```

### Transport Independence

The protocol is transport-agnostic. The action dispatch format is the same whether it travels over HTTP or BLE GATT. The `transports` array in the capability card tells the scanner which channels are available and in what priority order.

### Spec Files

- `DMED-protocol-spec-v0.2.md` — main spec
- `spec/appendix-a-json-schema.md` — capability card JSON schema
- `spec/appendix-b-ble-transport.md` — BLE GATT profile + UUID assignments
- `spec/appendix-c-mdns-transport.md` — mDNS/DNS-SD service type
- `spec/appendix-d-internet-transport.md` — WAN transport
- `spec/appendix-e-service-types.md` — standard service type registry
- `spec/appendix-f-examples.md` — worked examples
- `spec/appendix-g-implementation-guide.md` — implementation guide

### Things Templates

`things/` contains ready-to-use capability card templates for 8 categories of devices (lighting, fans, appliances, security, media, climate, energy, health). Copy, update the `instance_id` and `name`, implement the listed tools — your device is DMED-enabled.

### DMED Prompt Library

`DMED_PROMPT.md` is a prompt library for generating DMED-enabled endpoint code using AI. Ask Claude to build a DMED server and give it this file as context — it knows all the conventions, endpoint formats, and BLE profile details.

---

## Tier 2 — DMED Discovery Framework

The Discovery Framework is the SDK a scanner uses to find DMED devices across all transports simultaneously. From the scanner's perspective, transport is an implementation detail — it calls `scan()` and gets back a unified list of devices regardless of which transport found them.

### Transports

| Transport | Discovery Mechanism | Typical Device |
|---|---|---|
| **BLE** | Advertisement with DMED service UUID `D14D0000-...` | Wearables, appliances, embedded |
| **mDNS / DNS-SD** | `_dmed._tcp` service type | Desktop apps, smart home |
| **Ethernet / IP** | mDNS over wired LAN | Industrial, always-on appliances |
| **Thread / Matter** | Border router bridge → mDNS | Low-power IoT mesh |
| **Zigbee / Z-Wave** | Hub bridge → DMED adapter | Legacy home automation |
| **USB / Serial** | Adapter daemon on host | Embedded dev boards |

Any device on any of these transports can be DMED-enabled. The framework runs all listeners simultaneously and surfaces a unified result.

### Discovery Framework API (conceptual)

```
DmedDiscovery.scan()
  → emits DmedDevice events as devices are found (any transport)

DmedDevice {
  instance_id   // unique identifier — used for deduplication across transports
  name          // human-readable name from beacon / mDNS record
  transports    // list of transports this device was found on
  card          // capability card (populated after connect)
}

DmedDiscovery.connect(device)
  → fetches capability card over the best available transport
  → returns populated device

DmedDiscovery.dispatch(device, action, params)
  → sends action over the best available transport
  → returns ActionResponse
```

### Device Identity Across Transports

The same physical device may be visible on multiple transports simultaneously (BLE + mDNS). The framework deduplicates by `instance_id` from the beacon payload and capability card — not by address — so the device appears once in the list regardless of how many transports found it.

### Current Implementations

| Platform | Transports | Status | Location |
|---|---|---|---|
| Android (Kotlin/Compose) | BLE | Working | `android/dmed-scanner-working/` |
| Node.js client | mDNS | Working | `examples/dmed-client-scanner/` |
| Python client | HTTP | Working | `examples/dmed_client.py` |
| C client | BLE | Working | `examples/bleclient-dmed-working/` |

**Gap:** The Android scanner currently supports BLE only. The coffee machine server currently broadcasts mDNS only. Before the demo works end-to-end, one of them needs to support the other's transport — either add mDNS discovery to Android, or add BLE broadcast to the coffee machine server.

---

## Scanner Application — AI Brain

The scanner application (the mobile app or desktop agent) is a reference implementation of the Discovery Framework with an AI layer added on top.

Once connected to a device, the user interacts in natural language. Claude receives:

1. The device's capability card (what actions it supports and their input schemas)
2. The conversation history
3. The user's natural language message

Claude determines which action to call with what parameters, dispatches it over the appropriate transport, and narrates the result back to the user.

```
User: "make me a large latte"
         ↓
    Claude reads capability card tools
    → brew_coffee(drink_type="latte", size="large")
         ↓
    DmedDiscovery.dispatch(device, "brew_coffee", {drink_type: "latte", size: "large"})
         ↓
    Device: { status: "ok", result: { text: "☕ Your large latte is ready!" } }
         ↓
    Claude: "Your large latte is brewing ☕"
```

This turns every DMED device into a natural language interface — no need to know action names or parameter formats.

---

## Tier 3 — DMED-MCP Gateway

The gateway bridges local DMED devices to the internet, making them available to remote AI agents via MCP.

### Why This Matters

A remote Claude instance has no way to reach a BLE or mDNS device on your local network. The gateway is a local process that:

1. Runs on a machine with access to the local network and BLE
2. Discovers all DMED devices continuously
3. Exposes them as MCP tools over the internet
4. Proxies actions between the remote agent and the physical device

### How It Works

```
Remote Claude
     │
     │  MCP (HTTPS)
     ↓
DMED-MCP Gateway  (runs on local machine / home server / Raspberry Pi)
     │
     ├── BLE → [VoIP Phone]
     ├── mDNS → [Coffee Machine]
     └── Ethernet → [Thermostat]
```

The gateway presents itself to Claude as a standard MCP server. Each discovered DMED device becomes an MCP tool namespace. Each action on that device becomes an MCP tool. The tool descriptions and input schemas come directly from the device's capability card — the gateway requires no manual configuration.

### MCP Tool Mapping

```
DMED device:  "Smart Coffee Machine"
DMED action:  "brew_coffee" — "Brew a coffee with specified type and size"

→ MCP tool:   "coffee_machine__brew_coffee"
→ description: "Brew a coffee with specified type and size (Smart Coffee Machine)"
→ inputSchema: (verbatim from DMED capability card)
```

### Gateway API Surface (proposed)

```
GET  /mcp              → MCP server info
POST /mcp              → MCP JSON-RPC endpoint
GET  /dmed/devices     → list of all currently discovered local DMED devices
POST /dmed/refresh     → trigger a fresh scan
```

### Security

The gateway is a trust boundary. Local DMED is implicitly trusted (same physical space). WAN access requires:

- **API key auth** on the MCP endpoint (minimum)
- **Device allowlist** — not all local devices need to be WAN-accessible
- **Action allowlist** — a device can be read-only remotely but writable locally
- **TLS** — all WAN traffic encrypted

Security is deferred to v1.0 but the gateway must be designed with this boundary in mind from the start.

---

## End-to-End Flow — Full Picture

```
Scenario: Remote user asks Claude to brew coffee from their office

1. Remote Claude connects to user's DMED-MCP Gateway over MCP
2. Gateway has already discovered coffee machine via mDNS
3. Claude sees MCP tool: "coffee_machine__brew_coffee"
4. User: "start a large latte please"
5. Claude calls: coffee_machine__brew_coffee(drink_type="latte", size="large")
6. Gateway dispatches: POST /dmed/action {action: "brew_coffee", params: {drink_type: "latte", size: "large"}}
7. Coffee machine: {status: "ok", result: {text: "☕ Your large latte is ready!"}}
8. Gateway returns result to Claude over MCP
9. Claude: "Your large latte is brewing ☕"
```

---

## Relationship to MCP (Anthropic)

MCP's official roadmap covers **internet-scale discovery** via `.well-known/mcp.json` Server Cards — finding AI tools hosted on the public web.

DMED fills the **local and short-range** layer that MCP is not designed to cover:

| | MCP (official) | DMED |
|---|---|---|
| **Scope** | Internet / WAN | Local / BLE / LAN |
| **Discovery** | `.well-known` URL | BLE beacon / mDNS |
| **Device type** | Web services, APIs | Physical devices, embedded |
| **Transport** | HTTPS | Any (BLE, mDNS, Ethernet, …) |
| **Relationship** | Standard | Extends + bridges to MCP |

DMED is complementary, not competing. The gateway is where they meet — DMED discovers the physical world, MCP carries it to the cloud.

---

## Build Roadmap

### Now — Demo (v0.2)
- [ ] Commit coffee machine server fix (mDNS ESM import)
- [ ] Resolve transport gap: add mDNS discovery to Android scanner OR add BLE broadcast to coffee machine
- [ ] Fix DMED-only BLE filtering in Android scanner (currently shows all BLE devices)
- [ ] Add Claude API brain to Android chat screen (NL → action dispatch)
- [ ] End-to-end demo: phone finds coffee machine → natural language → device responds

### Next — Discovery Framework (v0.3)
- [ ] Android scanner: add mDNS discovery alongside BLE
- [ ] Device deduplication by `instance_id` across transports
- [ ] JS/TypeScript library (`lib/js/`)
- [ ] Discovery Framework spec document (transport abstraction layer)

### Later — MCP Gateway (v0.4)
- [ ] DMED-MCP Gateway (Node.js or Python)
- [ ] Auto-registers discovered devices as MCP tools
- [ ] Local web UI for device allowlist management
- [ ] Basic API key auth for WAN access

### v1.0
- [ ] Security hardening (auth, TLS, allowlists)
- [ ] Alignment with MCP Server Cards format
- [ ] Community feedback and spec finalization
- [ ] Reference gateway implementation for Raspberry Pi

---

## Summary

DMED is three things:

1. **A protocol** — a standard for how devices describe themselves and accept commands, over any transport
2. **A discovery framework** — a multi-transport SDK that surfaces DMED devices from BLE, mDNS, Ethernet, and more as a unified list
3. **An MCP gateway** — a bridge that makes local physical devices available to remote AI agents over the internet

The physical world becomes a set of tools any AI agent can discover, understand, and use — without configuration, without pairing, without registries.
