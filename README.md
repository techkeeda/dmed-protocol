# DMED — Dynamic MCP Endpoint Discovery Protocol

![Version](https://img.shields.io/badge/protocol-v0.2%20draft-orange)
![License](https://img.shields.io/badge/license-MIT-blue)
![Status](https://img.shields.io/badge/status-early%20stage-yellow)

**Make anything discoverable. Talk to anything.**

DMED is an open protocol that enables MCP (Model Context Protocol) endpoints to broadcast
their capabilities over WiFi, Ethernet, and Bluetooth — so AI clients can automatically
discover and interact with them with zero manual configuration.

---

## ⚠️ Security Notice

DMED v0.2 is a **draft protocol intended for local experimentation only**.

- Authentication and authorization are **not yet implemented**
- Do not expose DMED endpoints on untrusted or public networks
- Do not use in production environments
- Security hardening is planned for v1.0

[See roadmap →](#roadmap)

---

## Why DMED?

MCP made it easy to *build* AI-connected tools. But connecting to them still requires
manual setup — copy a URL, paste an API key, edit a config file.

DMED solves the *last mile*: getting an AI client to find and use a tool with zero
configuration, the same way your laptop finds a printer on WiFi.

- **No URLs to copy** — endpoints announce themselves on the network
- **No keys to paste** — discovery happens at the network layer
- **No config files** — clients build the connection automatically
- **Any device** — IoT sensors, databases, local services, Raspberry Pis, whatever exposes an MCP endpoint

```
📱 Open app → Scan network → See all AI-interactable things → Talk to them
```

---

## How It Works

```
┌─────────────────┐         mDNS/BLE          ┌──────────────────┐
│   DMED Client   │  ◄──── _dmed._tcp ────►   │   MCP Endpoint   │
│   (AI App)      │                            │   (Your Thing)   │
│                 │  ── fetch manifest ──►      │                  │
│                 │  ◄── capabilities ───       │                  │
│                 │  ── invoke tools ──►        │                  │
└─────────────────┘                            └──────────────────┘
```

1. **MCP Endpoint** broadcasts itself via mDNS (`_dmed._tcp`) or Bluetooth
2. **DMED Client** discovers it, fetches its `dmed-manifest.json`
3. **User** interacts with the endpoint through natural language via AI

### What a manifest looks like

```json
{
  "dmed": "0.2",
  "id": "coffee-machine-kitchen",
  "name": "Kitchen Coffee Machine",
  "description": "Smart coffee maker with brew controls",
  "transport": "mdns",
  "mcp_endpoint": "http://192.168.1.42:3000/mcp",
  "actions": [
    { "id": "brew", "description": "Start brewing", "params": ["strength", "cups"] },
    { "id": "status", "description": "Get current machine status" }
  ]
}
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      DMED Protocol Architecture (v0.2)                      │
└─────────────────────────────────────────────────────────────────────────────┘

┌───────────────────────────────┐           ┌─────────────────────────────────┐
│        DMED CLIENT            │           │        MCP ENDPOINT             │
│        (Discoverer)           │           │        (Broadcaster)            │
├───────────────────────────────┤           ├─────────────────────────────────┤
│                               │           │                                 │
│  ┌─────────────────────────┐  │           │  ┌───────────────────────────┐  │
│  │     DMEDScanner         │  │           │  │      DMEDServer           │  │
│  │  • mDNS listener        │  │           │  │  • Tool registration      │  │
│  │  • BLE scanner          │  │           │  │  • Tool handler logic     │  │
│  │  • Endpoint registry    │  │           │  │  • State management       │  │
│  └───────────┬─────────────┘  │           │  └──────────┬────────────────┘  │
│              │ discover        │           │             │ broadcast         │
│              ▼                 │           │             ▼                   │
│  ┌─────────────────────────┐  │           │  ┌───────────────────────────┐  │
│  │   Discovery Layer       │◄─┼── ─ ── ──┼──┤   Discovery Layer         │  │
│  │  • _dmed._tcp (mDNS)    │  │  beacon   │  │  • mDNS broadcast         │  │
│  │  • BLE scan             │  │           │  │  • BLE advertisement      │  │
│  └───────────┬─────────────┘  │           │  └───────────────────────────┘  │
│              │ found           │           │                                 │
│              ▼                 │           │  ┌───────────────────────────┐  │
│  ┌─────────────────────────┐  │           │  │   HTTP API Layer          │  │
│  │     DMEDClient          │  │   HTTP    │  ├───────────────────────────┤  │
│  │                         │  │           │  │                           │  │
│  │  .connect() ───────────────┼──────────►┼──│  GET  /dmed/card          │  │
│  │                         │  │           │  │       → Capability Card   │  │
│  │  .list_actions() ─────────┼──────────►┼──│  GET  /dmed/actions       │  │
│  │                         │  │           │  │       → Action schemas    │  │
│  │  .send_action() ──────────┼──────────►┼──│  POST /dmed/action        │  │
│  │    {action, params}     │  │           │  │       → {status, result}  │  │
│  │  .call_tool() ────────────┼──────────►┼──│  POST /mcp                │  │
│  │    (full MCP JSON-RPC)  │  │           │  │       → JSON-RPC response │  │
│  └─────────────────────────┘  │           │  └───────────────────────────┘  │
│                               │           │                                 │
└───────────────────────────────┘           └─────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                          TRANSPORT LAYER                                    │
├──────────────────────┬──────────────────────┬───────────────────────────────┤
│  WiFi / Ethernet     │    Bluetooth (BLE)   │         Internet              │
│  mDNS/DNS-SD         │    GATT Service      │      DNS TXT + HTTPS          │
│  _dmed._tcp          │    ~10m range        │       Global reach            │
│  LAN only            │                      │                               │
└──────────────────────┴──────────────────────┴───────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                        PROTOCOL LIFECYCLE                                   │
│                                                                             │
│  ┌──────────┐    ┌──────────┐    ┌──────────────┐    ┌──────────────┐     │
│  │ DISCOVER │───►│ CONNECT  │───►│   INTERACT   │───►│  DISCONNECT  │     │
│  │          │    │          │    │              │    │              │     │
│  │ Scan for │    │ Fetch    │    │ Send actions │    │ Stop sending │     │
│  │ beacons  │    │ card     │    │ or full MCP  │    │ (stateless)  │     │
│  └──────────┘    └──────────┘    └──────────────┘    └──────────────┘     │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Quick Start

### Make something discoverable

Feed [`DMED_PROMPT.md`](./DMED_PROMPT.md) to any AI tool and tell it:
> "I want to make my [THING] discoverable as an MCP endpoint. It should expose [CAPABILITIES]."

The AI will generate a complete DMED-enabled MCP endpoint for you.

### Try the examples

```bash
# Terminal 1: Start a smart coffee machine MCP endpoint
cd examples/smart-coffee-machine
npm install && npm start

# Terminal 2: Discover it
cd examples/dmed-client-scanner
npm install && npm run scan
```

---

## Project Structure

```
├── DMED_PROMPT.md                          # AI prompt library — feed to any AI tool
├── DMED-protocol-spec-v0.2.md              # Protocol specification v0.2 (current)
├── DMED-protocol-spec-v0.1.md              # Protocol specification v0.1 (archived)
├── DMED-prior-art-research.md              # Prior art & research
├── spec/
│   ├── DMED-spec-v0.1.md                   # Core spec
│   ├── appendix-a-json-schema.md           # JSON schema definitions
│   ├── appendix-b-ble-transport.md         # Bluetooth Low Energy transport
│   ├── appendix-c-mdns-transport.md        # mDNS/DNS-SD transport
│   ├── appendix-d-internet-transport.md    # Internet/WAN transport
│   ├── appendix-e-service-types.md         # Endpoint categories
│   ├── appendix-f-examples.md              # Protocol examples
│   └── appendix-g-implementation-guide.md  # Implementation guide
├── lib/
│   ├── c/                                  # C library
│   ├── cpp/                                # C++ library
│   └── python/                             # Python library
├── docs/
│   ├── getting-started.md
│   ├── api-reference-c.md
│   ├── api-reference-cpp.md
│   ├── api-reference-python.md
│   └── tutorials/
│       ├── share-on-local-wifi.md
│       ├── share-on-bluetooth.md
│       ├── share-on-internet.md
│       └── build-a-client.md
├── examples/
│   ├── smart-coffee-machine/               # Node.js MCP endpoint + DMED
│   ├── dmed-client-scanner/                # Node.js discovery client
│   ├── dmed_server.py                      # Python server example
│   ├── dmed_client.py                      # Python client example
│   ├── thought_stream.c                    # C server example
│   └── thought_client.c                    # C client example
```

---

## Specification

The full protocol spec is in [`DMED-protocol-spec-v0.2.md`](./DMED-protocol-spec-v0.2.md) with appendices covering:

- **Transport layers:** mDNS/DNS-SD, Bluetooth Low Energy, Internet/WAN
- **Manifest schema:** JSON schema for `dmed-manifest.json`
- **Endpoint categories:** IoT, API, database, media, utility, commerce
- **Implementation guide:** Step-by-step for all supported languages

---

## Libraries

| Language             | Path          | Status                                      |
|----------------------|---------------|---------------------------------------------|
| Python               | `lib/python/` | ✅ Available                                |
| C                    | `lib/c/`      | ✅ Available                                |
| C++                  | `lib/cpp/`    | ✅ Available                                |
| JavaScript/TypeScript | `lib/js/`    | 🔜 Planned — [want to contribute?](./CONTRIBUTING.md) |

---

## How DMED Relates to MCP's Official Roadmap

The MCP specification (maintained by Anthropic and the Agentic AI Foundation) has its own
server discovery on the 2026 roadmap — specifically **MCP Server Cards**, which expose
server metadata at `.well-known` URLs for internet-scale discovery by browsers, crawlers,
and registries.

DMED is **complementary, not competing**:

| Scope          | MCP Official Roadmap         | DMED                          |
|----------------|------------------------------|-------------------------------|
| Internet / WAN | ✅ `.well-known` Server Cards | ✅ DNS TXT + HTTPS (appendix D) |
| Local network  | ❌ Not in scope              | ✅ mDNS / `_dmed._tcp`         |
| Bluetooth      | ❌ Not in scope              | ✅ BLE GATT advertisement      |
| IoT / embedded | ❌ Not in scope              | ✅ C/C++ libs, low-resource targets |

DMED's primary value is in **local and short-range discovery** — the layer the official
spec isn't covering. The WAN transport in DMED will be aligned with the final MCP
Server Cards format as the spec matures.

---

## Status

🚧 **Draft Specification (v0.2) — Early Stage**

This is an early-stage open protocol. The goal is to gather feedback, build reference
implementations, and iterate toward a stable v1.0. Contributions, feedback, and ideas
are welcome.

### Roadmap

- [x] Core protocol specification
- [x] mDNS/DNS-SD transport
- [x] Bluetooth BLE transport spec
- [x] Internet/WAN transport spec
- [x] Reference libraries (C, C++, Python)
- [x] AI prompt library for code generation
- [x] Working examples (Node.js, Python, C)
- [x] Interaction protocol (lightweight actions) — v0.2
- [ ] JavaScript / TypeScript library
- [ ] Mobile client reference app
- [ ] Security & authentication hardening
- [ ] Alignment with MCP Server Cards (official spec)
- [ ] GitHub Discussions for community feedback
- [ ] Protocol v1.0 finalization

---

## Discussion

Have a question, feedback on the spec, or a use case to share?
Open a [GitHub Discussion](https://github.com/techkeeda/dmed-protocol/discussions) —
protocol design questions, IoT use cases, and implementation help are all welcome.

---

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for guidelines.

---

## License

[MIT](./LICENSE)

---

**Author:** Nilesh Valmik Ladhe
**Email:** worknileshladhe@gmail.com
**Protocol Version:** 0.2 (Draft)

