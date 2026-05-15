# DMED — Dynamic MCP Endpoint Discovery Protocol

**Make anything discoverable. Talk to anything.**

DMED is an open protocol that enables MCP (Model Context Protocol) endpoints to broadcast their capabilities over WiFi, Ethernet, and Bluetooth, so AI clients can automatically discover and interact with them — no manual configuration needed.

## The Vision

```
📱 Open app → Scan network → See all AI-interactable things → Talk to them
```

Imagine a world where your coffee machine, database, security camera, or any service announces itself on the network. Any DMED-compatible AI client finds it, reads its capabilities, and lets you interact with it through natural language.

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

## Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      DMED Protocol Architecture (v0.2)                        │
└─────────────────────────────────────────────────────────────────────────────┘

┌───────────────────────────────┐           ┌─────────────────────────────────┐
│        DMED CLIENT            │           │        MCP ENDPOINT              │
│        (Discoverer)           │           │        (Broadcaster)             │
├───────────────────────────────┤           ├─────────────────────────────────┤
│                               │           │                                 │
│  ┌─────────────────────────┐  │           │  ┌───────────────────────────┐  │
│  │     DMEDScanner         │  │           │  │      DMEDServer           │  │
│  │  • mDNS listener       │  │           │  │  • Tool registration      │  │
│  │  • BLE scanner          │  │           │  │  • Tool handler logic     │  │
│  │  • Endpoint registry    │  │           │  │  • State management       │  │
│  └───────────┬─────────────┘  │           │  └──────────┬────────────────┘  │
│              │ discover        │           │             │ broadcast         │
│              ▼                 │           │             ▼                   │
│  ┌─────────────────────────┐  │           │  ┌───────────────────────────┐  │
│  │   Discovery Layer       │◄─┼── ─ ── ──┼──┤   Discovery Layer         │  │
│  │  • _dmed._tcp (mDNS)   │  │  beacon   │  │  • mDNS broadcast        │  │
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
│                          TRANSPORT LAYER                                      │
├──────────────────────┬──────────────────────┬───────────────────────────────┤
│  WiFi / Ethernet     │    Bluetooth (BLE)   │         Internet              │
│  mDNS/DNS-SD         │    GATT Service      │      DNS TXT + HTTPS          │
│  _dmed._tcp          │    ~10m range        │       Global reach            │
│  LAN only            │                      │                               │
└──────────────────────┴──────────────────────┴───────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│                        PROTOCOL LIFECYCLE                                     │
│                                                                              │
│  ┌──────────┐    ┌──────────┐    ┌──────────────┐    ┌──────────────┐      │
│  │ DISCOVER │───►│ CONNECT  │───►│   INTERACT   │───►│  DISCONNECT  │      │
│  │          │    │          │    │              │    │              │      │
│  │ Scan for │    │ Fetch    │    │ Send actions │    │ Stop sending │      │
│  │ beacons  │    │ card     │    │ or full MCP  │    │ (stateless)  │      │
│  └──────────┘    └──────────┘    └──────────────┘    └──────────────┘      │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

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

## Project Structure

```
├── DMED_PROMPT.md                         # AI prompt library — feed to any AI tool
├── DMED-protocol-spec-v0.2.md             # Protocol specification v0.2 (current)
├── DMED-protocol-spec-v0.1.md             # Protocol specification v0.1 (archived)
├── DMED-prior-art-research.md             # Prior art & research
├── spec/                                  # Full protocol specification
│   ├── DMED-spec-v0.1.md                  # Core spec
│   ├── appendix-a-json-schema.md          # JSON schema definitions
│   ├── appendix-b-ble-transport.md        # Bluetooth Low Energy transport
│   ├── appendix-c-mdns-transport.md       # mDNS/DNS-SD transport
│   ├── appendix-d-internet-transport.md   # Internet/WAN transport
│   ├── appendix-e-service-types.md        # Endpoint categories
│   ├── appendix-f-examples.md             # Protocol examples
│   └── appendix-g-implementation-guide.md # Implementation guide
├── lib/                                   # Reference libraries
│   ├── c/                                 # C library
│   ├── cpp/                               # C++ library
│   └── python/                            # Python library
├── docs/                                  # Documentation
│   ├── getting-started.md
│   ├── api-reference-c.md
│   ├── api-reference-cpp.md
│   ├── api-reference-python.md
│   └── tutorials/
│       ├── share-on-local-wifi.md
│       ├── share-on-bluetooth.md
│       ├── share-on-internet.md
│       └── build-a-client.md
├── examples/                              # Working examples
│   ├── smart-coffee-machine/              # Node.js MCP endpoint + DMED
│   ├── dmed-client-scanner/               # Node.js discovery client
│   ├── dmed_server.py                     # Python server example
│   ├── dmed_client.py                     # Python client example
│   ├── thought_stream.c                   # C server example
│   └── thought_client.c                   # C client example
```

## Specification

The full protocol spec is in [`DMED-protocol-spec-v0.2.md`](./DMED-protocol-spec-v0.2.md) with appendices covering:

- **Transport layers:** mDNS/DNS-SD, Bluetooth Low Energy, Internet/WAN
- **Manifest schema:** JSON schema for `dmed-manifest.json`
- **Endpoint categories:** IoT, API, database, media, utility, commerce
- **Implementation guide:** Step-by-step for all supported languages

## Libraries

| Language | Path | Status |
|----------|------|--------|
| Python | [`lib/python/`](./lib/python/) | ✅ Available |
| C | [`lib/c/`](./lib/c/) | ✅ Available |
| C++ | [`lib/cpp/`](./lib/cpp/) | ✅ Available |

## Status

🚧 **Draft Specification (v0.2)**

This is an early-stage open protocol. Contributions, feedback, and ideas are welcome.

### Roadmap

- [x] Core protocol specification
- [x] mDNS/DNS-SD transport
- [x] Bluetooth BLE transport spec
- [x] Internet/WAN transport spec
- [x] Reference libraries (C, C++, Python)
- [x] AI prompt library for code generation
- [x] Working examples (Node.js, Python, C)
- [x] Interaction protocol (lightweight actions) — v0.2
- [ ] Mobile client reference app
- [ ] Security & authentication hardening
- [ ] Protocol v1.0 finalization

## Contributing

See [CONTRIBUTING.md](./CONTRIBUTING.md) for guidelines.

## License

[MIT](./LICENSE)

---

**Author:** Nilesh Valmik Ladhe  
**Email:** worknileshladhe@gmail.com | *[secondary email — TBD]*  
**Protocol Version:** 0.2 (Draft)
