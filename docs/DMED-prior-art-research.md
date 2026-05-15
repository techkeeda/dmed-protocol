# DMED (Dynamic MCP Discovery) — Prior Art Research

**Date:** 2026-05-14
**Status:** Research phase
**Author:** Nilesh

## Concept Summary

DMED is a proposed protocol for **dynamic, cross-transport discovery of MCP (Model Context Protocol) servers**. A consumer mobile app scans across Bluetooth, WiFi, Ethernet, NFC, etc., discovers nearby MCP-capable services, and lets users interact with them via AI assistants.

Key differentiators:
- Unified discovery across multiple physical transports (BT, WiFi, Ethernet, NFC)
- Consumer-facing mobile app as universal MCP client
- Zero-config, scan-and-connect UX (like finding Chromecast devices)
- Standard beacon format for MCP capability advertisement over physical media

---

## Existing MCP Discovery Mechanisms

### 1. IETF Draft: DNS TXT Records for MCP Discovery

- **Spec:** `draft-morrison-mcp-dns-discovery-02` (April 2026)
- **URL:** https://www.ietf.org/archive/id/draft-morrison-mcp-dns-discovery-02.html
- **Mechanism:** DNS TXT record at `_mcp.<domain>` advertising endpoint URL, transport protocol, public key, capabilities
- **Scope:** Internet/domain-scoped. Requires domain ownership.
- **Limitation:** Not applicable to local/physical device discovery. No multi-transport support.

### 2. `.well-known/mcp` Discovery Endpoint

- **Discussion:** https://github.com/orgs/modelcontextprotocol/discussions/84
- **Mechanism:** HTTPS endpoint at `/.well-known/mcp` serving server metadata
- **Scope:** Web/cloud servers only
- **Limitation:** Requires knowing the domain first. No local discovery.

### 3. MCP Registry (Official)

- **URL:** https://github.com/modelcontextprotocol/registry
- **Mechanism:** Centralized community-driven catalog of MCP servers (like npm)
- **Scope:** Cloud-based directory
- **Limitation:** Static, centralized. No runtime/proximity discovery.

### 4. Docker Dynamic MCP (Catalog & Toolkit)

- **URL:** https://docs.docker.com/ai/mcp-catalog-and-toolkit/dynamic-mcp/
- **Mechanism:** Agents dynamically discover and load MCP servers at runtime from Docker Hub catalog
- **Scope:** Container ecosystem
- **Limitation:** Cloud/registry-based. Not physical proximity.

### 5. GitHub Discussion #69: Service Discovery for MCPs (Nov 2024)

- **URL:** https://github.com/modelcontextprotocol/modelcontextprotocol/discussions/69
- **Proposal:** `mcp://` URI scheme for dynamic service identification and metadata retrieval
- **Status:** Acknowledged by maintainers, no spec produced
- **Key ideas:** URI-based discovery, metadata access, dynamic integration

### 6. GitHub Discussion #1173: Local Network Discoverability (Apr 2025)

- **URL:** https://github.com/modelcontextprotocol/modelcontextprotocol/discussions/1173
- **Proposal:** mDNS/Bonjour-style zero-config discovery for Streamable HTTP MCP servers on local networks
- **Status:** 1 reply, no official response, no spec
- **Closest prior art to DMED** — but limited to WiFi/LAN, no multi-transport, no consumer app vision

---

## Related Protocols & Standards

### Google A2A (Agent-to-Agent Protocol)

- **URL:** https://developers.googleblog.com/en/a2a-a-new-era-of-agent-interoperability/
- **Launched:** April 2025, now under Linux Foundation, 150+ partners
- **Discovery:** Agent Cards at `/.well-known/agent.json`
- **Scope:** Agent-to-agent communication (complementary to MCP)
- **Limitation:** HTTP-only discovery. No physical transport support.

### mDNS / DNS-SD (Bonjour/Avahi)

- **Standard:** RFC 6762 (mDNS), RFC 6763 (DNS-SD)
- **Mechanism:** Multicast DNS for zero-config service discovery on local networks
- **Used by:** AirPlay, Chromecast, printers, HomeKit
- **Relevance:** Proven pattern for WiFi/LAN discovery. Could be one transport binding for DMED.

### Bluetooth Service Discovery Protocol (SDP) / GATT

- **Standard:** Bluetooth Core Spec
- **Mechanism:** Devices advertise service UUIDs; clients scan and discover
- **Relevance:** Proven pattern for BLE discovery. Could be another transport binding for DMED.

### UPnP / SSDP

- **Standard:** UPnP Device Architecture
- **Mechanism:** SSDP multicast for device discovery, XML device descriptions
- **Used by:** Smart home devices, media servers
- **Relevance:** Similar broadcast-and-discover pattern, but XML-heavy and aging.

### Matter Protocol (Smart Home)

- **URL:** https://csa-iot.org/all-solutions/matter/
- **Mechanism:** Multi-transport device discovery (WiFi, Thread, BLE for commissioning)
- **Relevance:** Closest architectural analog — multi-transport discovery for IoT. But domain-specific to smart home, not AI tools.

### W3C Web of Things (WoT)

- **URL:** https://www.w3.org/WoT/
- **Mechanism:** Thing Descriptions (TD) as JSON-LD documents describing device capabilities
- **Relevance:** Capability description format for IoT devices. Could inform DMED's capability schema.

---

## Existing BLE + MCP Implementations

| Project | URL | Notes |
|---------|-----|-------|
| ble-mcp-server | https://github.com/es617/ble-mcp-server | BLE MCP server for Claude Code |
| bluetooth-mcp-server | https://github.com/Hypijump31/bluetooth-mcp-server | Bluetooth MCP server for Claude AI |

These are individual implementations proving MCP-over-BLE is feasible, but they have no discovery protocol.

---

## Gap Analysis: What DMED Would Uniquely Provide

| Capability | Existing Solutions | DMED |
|-----------|-------------------|-----|
| Internet/DNS discovery | IETF draft, `.well-known/mcp` | Out of scope (complementary) |
| Cloud registry | MCP Registry, Docker Catalog | Out of scope (complementary) |
| Local WiFi discovery | Discussion #1173 (proposal only, no spec) | ✓ Standardized |
| Bluetooth/BLE discovery | Individual hacks, no standard | ✓ Standardized |
| NFC tap-to-connect | Nothing for MCP | ✓ Standardized |
| Cross-transport unified protocol | **Nothing exists** | ✓ Core innovation |
| Consumer mobile app | **Nothing exists** | ✓ Core innovation |
| Standard beacon/advertisement format | **Nothing exists** | ✓ Core innovation |
| Physical proximity awareness | Apple App Clips (not MCP) | ✓ MCP-native |

---

## Conclusion

**DMED is novel.** The individual building blocks exist (mDNS, BLE SDP, MCP, A2A), but no one has:

1. Defined a unified discovery protocol across BT + WiFi + Ethernet + NFC for MCP
2. Specified a standard beacon format for MCP capability advertisement over physical transports
3. Built a consumer mobile app that acts as a universal MCP client with scan-and-connect UX
4. Named or formalized "Dynamic MCP Discovery" as a protocol

The closest prior art is MCP Discussion #1173 (mDNS for local MCP) and Matter (multi-transport IoT discovery), but neither covers the full DMED vision.

---

## Next Steps

- [ ] Draft DMED protocol specification (transport bindings, beacon format, capability schema, handshake flow)
- [ ] Design mobile app architecture (scanner, MCP session manager, AI assistant integration)
- [ ] Build proof-of-concept (mDNS beacon + BLE beacon + simple client)
- [ ] Engage with MCP community (Discussion #1173, registry discussions)
