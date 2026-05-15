# DMED Protocol Specification v0.1

## Dynamic MCP Discovery Protocol

**Document Version:** 0.1.0
**Date:** 2026-05-15
**Status:** Draft Specification
**License:** Apache 2.0
**Authors:** Nilesh

---

## Abstract

The Dynamic MCP Discovery Protocol (DMED) is an open standard that enables automatic discovery of Model Context Protocol (MCP) servers across Bluetooth Low Energy, WiFi, Ethernet, and Internet transports. DMED provides a unified discovery layer so that any MCP client can find and connect to nearby services, devices, tools, and information endpoints without prior configuration.

This specification defines the beacon format, capability card schema, transport bindings, security model, and client/server behavior required for interoperable implementations.

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Terminology](#2-terminology)
3. [Architecture](#3-architecture)
4. [Beacon Format](#4-beacon-format)
5. [Capability Card](#5-capability-card)
6. [Transport Bindings](#6-transport-bindings)
7. [Client Behavior](#7-client-behavior)
8. [Server Behavior](#8-server-behavior)
9. [Security](#9-security)
10. [Error Handling](#10-error-handling)
11. [Versioning & Extensibility](#11-versioning--extensibility)
12. [Conformance](#12-conformance)

Appendices:
- [A: JSON Schema — Capability Card](#appendix-a-json-schema--capability-card)
- [B: BLE Transport Detail](#appendix-b-ble-transport-detail)
- [C: mDNS Transport Detail](#appendix-c-mdns-transport-detail)
- [D: Internet Transport Detail](#appendix-d-internet-transport-detail)
- [E: Service Type Registry](#appendix-e-service-type-registry)
- [F: Examples — Complete Flows](#appendix-f-examples--complete-flows)
- [G: Reference Implementation Guide](#appendix-g-reference-implementation-guide)

---

## 1. Introduction

### 1.1 Problem Statement

MCP (Model Context Protocol) defines how AI clients communicate with tool-providing servers, but provides no mechanism for discovering those servers. Today, MCP server URLs must be manually configured. This prevents:

- Consumer devices from being instantly accessible to AI assistants
- IoT devices from advertising their AI-controllable capabilities
- Physical spaces (shops, factories, hospitals) from offering contextual AI services
- Users from discovering what they can interact with in their environment

### 1.2 Solution

DMED adds a discovery layer beneath MCP. A DMED server broadcasts a compact beacon identifying itself. A DMED client scans for beacons, retrieves detailed capability information, and establishes a standard MCP session. DMED does not modify MCP — it only helps clients find MCP servers.

### 1.3 Design Goals

1. **Implementable in a weekend** — Minimal complexity, clear wire formats
2. **Runs on microcontrollers** — Beacon fits in 7 bytes; server needs <1KB RAM for discovery
3. **Transport-agnostic** — Same logical protocol over BLE, WiFi, Ethernet, Internet
4. **Progressive disclosure** — Tiny beacon → detailed card → full MCP session
5. **Secure by default** — No PII in beacons; auth required before tool invocation
6. **Forward-compatible** — Unknown fields are ignored; version negotiation built in

### 1.4 Requirements Language

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this document are to be interpreted as described in RFC 2119.

---

## 2. Terminology

| Term | Definition |
|------|-----------|
| **DMED Server** | An endpoint that broadcasts DMED beacons and serves a Capability Card. It also runs an MCP server. |
| **DMED Client** | An application that scans for DMED beacons, retrieves Capability Cards, and establishes MCP sessions. |
| **Beacon** | A compact binary or text advertisement broadcast by a DMED server on one or more transports. |
| **Capability Card** | A JSON document describing the server's identity, available transports, authentication requirements, and MCP capabilities. |
| **Instance ID** | A 32-bit identifier unique to a DMED server instance. Used for deduplication across transports. |
| **Service Type** | An 8-bit category code classifying what kind of service the server provides. |
| **Transport Binding** | The specification of how DMED messages are encoded for a specific network technology. |
| **MCP Session** | A standard Model Context Protocol connection established after DMED discovery completes. |

---

## 3. Architecture

### 3.1 Protocol Layers

```
┌─────────────────────────────────────┐
│  APPLICATION (AI Assistant / User)   │
├─────────────────────────────────────┤
│  MCP (Model Context Protocol)        │  ← Standard MCP session
├─────────────────────────────────────┤
│  DMED (Dynamic MCP Discovery)         │  ← This specification
├──────────┬──────────┬───────────────┤
│   BLE    │  mDNS    │  DNS/HTTPS    │  ← Transport bindings
├──────────┼──────────┼───────────────┤
│Bluetooth │  WiFi/   │  Internet     │  ← Physical layer
│  Radio   │ Ethernet │              │
└──────────┴──────────┴───────────────┘
```

### 3.2 Discovery Flow (Normative)

Discovery proceeds in three mandatory phases:

```
┌────────┐                              ┌────────┐
│ CLIENT │                              │ SERVER │
└───┬────┘                              └───┬────┘
    │                                       │
    │  Phase 1: SCAN                        │
    │  ◄──────────────────────────────────  │  Server broadcasts Beacon
    │  Client receives Beacon               │  (continuous, on all transports)
    │                                       │
    │  Phase 2: RESOLVE                     │
    │  ─────────────────────────────────►   │  Client requests Capability Card
    │  ◄──────────────────────────────────  │  Server returns Capability Card
    │                                       │
    │  Phase 3: CONNECT                     │
    │  ─────────────────────────────────►   │  Client initiates MCP session
    │  ◄─────────────────────────────────►  │  Standard MCP communication
    │                                       │
```

**Phase 1 — SCAN:** The client passively listens for beacons. The client MUST NOT transmit during this phase (passive scanning). The server continuously broadcasts beacons at a defined interval.

**Phase 2 — RESOLVE:** The client actively connects to the server and retrieves the Capability Card. This is the first bidirectional communication. The client uses transport-specific mechanisms (BLE GATT read, HTTP GET, etc.).

**Phase 3 — CONNECT:** The client establishes a standard MCP session using connection details from the Capability Card. From this point forward, communication follows the MCP specification exactly.

### 3.3 Multi-Transport Deduplication

A single DMED server MAY advertise on multiple transports simultaneously. The `instance_id` field MUST be identical across all transports for the same logical server. Clients MUST deduplicate discovered servers by `instance_id`, merging transport options into a single logical entry.

---

## 4. Beacon Format

### 4.1 Abstract Beacon Structure

Every DMED beacon, regardless of transport, carries these logical fields:

| Field | Bits | Required | Description |
|-------|------|----------|-------------|
| `version` | 4 | REQUIRED | DMED protocol version. This spec defines version `1`. |
| `flags` | 4 | REQUIRED | Bitfield (see 4.2) |
| `service_type` | 8 | REQUIRED | Category code (see Appendix E) |
| `instance_id` | 32 | REQUIRED | Unique server instance identifier |
| `tx_power` | 8 | OPTIONAL | Calibrated TX power in dBm (signed int8). For proximity estimation. |
| `name_hash` | 16 | OPTIONAL | CRC-16/CCITT of UTF-8 server name. For quick filtering. |

**Minimum beacon size: 6 bytes** (version + flags + service_type + instance_id)
**Maximum beacon size: 9 bytes** (all fields)

### 4.2 Flags Bitfield

| Bit | Name | Meaning when set (1) |
|-----|------|---------------------|
| 0 | `AUTH` | Server requires authentication before MCP tool invocation |
| 1 | `TLS` | MCP session transport is encrypted (TLS/DTLS) |
| 2 | `MULTI` | Server exposes more than one MCP tool |
| 3 | `CLOUD` | Server is backed by / proxies to an internet service |

### 4.3 Instance ID Generation

The `instance_id` MUST be:
- Unique within the local broadcast domain (no collisions with other DMED servers on the same network)
- Stable across reboots (unless privacy rotation is desired — see Section 9.3)
- Identical across all transport bindings for the same logical server

Recommended generation methods:
1. Random 32-bit value generated once at first boot, persisted to non-volatile storage
2. CRC-32 of device serial number or MAC address
3. Lower 32 bits of SHA-256(device_unique_identifier)

### 4.4 Binary Wire Format

For transports that carry binary data (BLE):

```
Offset  Size    Field
──────  ──────  ─────────────
0       1 byte  [version:4 bits][flags:4 bits]
1       1 byte  service_type
2       4 bytes instance_id (big-endian, network byte order)
6       1 byte  tx_power (signed int8) — OPTIONAL
7       2 bytes name_hash (big-endian) — OPTIONAL
```

### 4.5 Text Wire Format

For transports that carry text key-value pairs (mDNS TXT, DNS TXT):

```
v=<version>
id=<instance_id as 8-char lowercase hex>
st=<service_type as 2-char lowercase hex>
fl=<flags as 1-char hex>
tp=<tx_power as signed decimal> (OPTIONAL)
nh=<name_hash as 4-char lowercase hex> (OPTIONAL)
nm=<human-readable name, UTF-8, max 63 bytes> (OPTIONAL, mDNS only)
```

---

## 5. Capability Card

### 5.1 Overview

The Capability Card is a JSON document that provides all information a client needs to:
1. Display the server to the user (name, description, icon)
2. Determine authentication requirements
3. Choose a transport for the MCP session
4. Understand what tools/resources are available before connecting

### 5.2 Content-Type

Capability Cards MUST be served with:
```
Content-Type: application/json; charset=utf-8
```

### 5.3 Schema (Normative)

See [Appendix A](#appendix-a-json-schema--capability-card) for the complete JSON Schema.

### 5.4 Field Definitions

#### 5.4.1 Required Fields

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| `dmed_version` | string | Semver, e.g. "0.1.0" | DMED protocol version this card conforms to |
| `instance_id` | string | 8-char hex | MUST match beacon instance_id |
| `name` | string | 1-128 chars, UTF-8 | Human-readable server name |
| `service_type` | string | See Appendix E | Category identifier (string form) |
| `transports` | array | Min 1 element | Available connection methods (see 5.4.3) |
| `capabilities` | object | See 5.4.4 | MCP capabilities summary |

#### 5.4.2 Optional Fields

| Field | Type | Description |
|-------|------|-------------|
| `description` | string (max 512 chars) | Longer human-readable description |
| `vendor` | object | `{name: string, url?: string, contact?: string}` |
| `auth` | object | Authentication details (see 5.4.5) |
| `metadata` | object | Arbitrary key-value pairs |
| `icon_url` | string (URL) | Square icon, min 64x64px, PNG or SVG |
| `expires` | integer (unix timestamp) | When client should re-fetch this card |
| `location` | object | `{latitude?: number, longitude?: number, description?: string}` |
| `tags` | array of strings | Searchable keywords |
| `languages` | array of strings | BCP 47 language tags supported |

#### 5.4.3 Transport Object

Each element in the `transports` array:

```json
{
  "type": "http | https | ble_gatt | ws | wss",
  "url": "http://192.168.1.42:8080/mcp",
  "priority": 1,
  "bandwidth": "high | medium | low",
  "latency_ms": 50
}
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `type` | string | REQUIRED | Transport protocol identifier |
| `url` | string | REQUIRED for http/https/ws/wss | Full URL to MCP endpoint |
| `address` | string | REQUIRED for ble_gatt | BLE MAC address |
| `service_uuid` | string | REQUIRED for ble_gatt | GATT service UUID for MCP |
| `priority` | integer | OPTIONAL | Lower = preferred. Default: 10 |
| `bandwidth` | string | OPTIONAL | Expected throughput class |
| `latency_ms` | integer | OPTIONAL | Expected round-trip latency |

#### 5.4.4 Capabilities Object

```json
{
  "tools": [
    {
      "name": "tool_name",
      "description": "What this tool does",
      "input_schema_summary": "brightness: integer 0-100"
    }
  ],
  "resources": [
    {
      "uri": "resource://temperature/current",
      "name": "Current Temperature",
      "description": "Real-time temperature reading"
    }
  ],
  "prompts": [
    {
      "name": "help",
      "description": "Get usage instructions"
    }
  ]
}
```

The capabilities section is a **summary** — full tool schemas are available via standard MCP `tools/list` after session establishment.

#### 5.4.5 Auth Object

```json
{
  "type": "none | pin | api_key | oauth2 | mtls",
  "pin_mechanism": "device_screen | sms | email",
  "oauth2": {
    "authorization_url": "https://auth.example.com/authorize",
    "token_url": "https://auth.example.com/token",
    "scopes": ["read", "write"],
    "client_id": "dmed_public_client"
  },
  "mtls": {
    "ca_cert_url": "https://factory.internal/ca.pem",
    "client_cert_required": true
  },
  "api_key": {
    "header": "X-API-Key",
    "registration_url": "https://example.com/register"
  }
}
```

| Auth Type | Description | When to Use |
|-----------|-------------|-------------|
| `none` | No authentication required | Public kiosks, open info endpoints |
| `pin` | User enters PIN shown on device | Home IoT, personal devices |
| `api_key` | Pre-shared API key in HTTP header | Developer tools, metered services |
| `oauth2` | Standard OAuth 2.0 authorization code flow | Cloud services, enterprise |
| `mtls` | Mutual TLS with client certificate | Industrial, high-security |

---

## 6. Transport Bindings

### 6.1 BLE Transport Binding

See [Appendix B](#appendix-b-ble-transport-detail) for complete BLE specification including:
- UUID assignments
- Advertisement PDU structure
- GATT service and characteristic definitions
- MTU handling for large Capability Cards
- Connection parameters

### 6.2 mDNS/DNS-SD Transport Binding (WiFi & Ethernet)

See [Appendix C](#appendix-c-mdns-transport-detail) for complete mDNS specification including:
- Service type registration
- TXT record format
- SRV record usage
- Browsing and resolution procedure
- IPv4 and IPv6 support

### 6.3 Internet Transport Binding (DNS + HTTPS)

See [Appendix D](#appendix-d-internet-transport-detail) for complete Internet specification including:
- DNS TXT record format
- `.well-known` URI registration
- TLS requirements
- CORS headers for browser clients

---

## 7. Client Behavior

### 7.1 State Machine

```
                    ┌─────────┐
                    │  IDLE   │
                    └────┬────┘
                         │ start_scan()
                         ▼
                    ┌─────────┐
              ┌────►│SCANNING │◄────────────────────┐
              │     └────┬────┘                     │
              │          │ beacon_received           │
              │          ▼                           │
              │     ┌──────────┐                    │
              │     │RESOLVING │─── timeout/error ──┘
              │     └────┬─────┘
              │          │ card_received
              │          ▼
              │     ┌──────────┐
              │     │ RESOLVED │
              │     └────┬─────┘
              │          │ user_connect() or auto_connect()
              │          ▼
              │     ┌───────────┐
              │     │CONNECTING │─── timeout/error ──┐
              │     └────┬──────┘                    │
              │          │ mcp_session_established   │
              │          ▼                           │
              │     ┌───────────┐                    │
              └─────│ CONNECTED │                    │
             lost   └───────────┘                    │
                                                     │
                    ┌─────────┐                      │
                    │  ERROR  │◄─────────────────────┘
                    └─────────┘
```

### 7.2 Scanning Rules

1. Clients MUST support at least one transport binding.
2. Clients SHOULD scan all available transports simultaneously.
3. Clients MUST use passive BLE scanning (no scan requests during Phase 1).
4. Clients MUST deduplicate by `instance_id` across transports.
5. Clients SHOULD present discovered servers to the user within 2 seconds of first beacon.
6. Clients MUST remove servers from the discovered list after `3 × advertisement_interval` without a beacon (default: 30 seconds for BLE, 75 seconds for mDNS).

### 7.3 Resolution Rules

1. Clients MUST fetch the Capability Card before establishing an MCP session.
2. Clients MUST validate that the card's `instance_id` matches the beacon's.
3. Clients MUST validate the card's `dmed_version` is compatible.
4. Clients SHOULD cache Capability Cards until the `expires` timestamp.
5. Clients MUST re-fetch the card if the beacon's `name_hash` changes.

### 7.4 Connection Rules

1. Clients MUST select a transport from the card's `transports` array.
2. Clients SHOULD prefer transports with lower `priority` values.
3. Clients MUST complete authentication before invoking MCP tools.
4. Clients MUST handle MCP session establishment per the MCP specification.
5. Clients SHOULD fall back to alternate transports if the preferred one fails.

### 7.5 User Interface Requirements

1. Clients MUST display the server `name` to the user before connecting.
2. Clients MUST indicate the `auth` type before initiating authentication.
3. Clients SHOULD display `service_type` as a category icon or label.
4. Clients SHOULD indicate signal strength / proximity when available.
5. Clients MUST NOT auto-connect to authenticated endpoints without user consent.
6. Clients MAY auto-connect to `auth: "none"` endpoints if user has enabled this preference.

---

## 8. Server Behavior

### 8.1 Advertisement Rules

1. Servers MUST broadcast beacons at a regular interval on at least one transport.
2. Servers MUST use the same `instance_id` across all transports.
3. Servers MUST set the `AUTH` flag if any tool requires authentication.
4. Servers MUST set the `TLS` flag if the MCP endpoint uses TLS/HTTPS.
5. Servers SHOULD advertise on all available transports for maximum discoverability.

**Recommended advertisement intervals:**
| Transport | Interval | Rationale |
|-----------|----------|-----------|
| BLE | 100ms - 1000ms | BLE spec recommendation for discoverability |
| mDNS | Respond to queries; proactive announce every 20-120 min | RFC 6762 compliance |
| DNS | TTL-based (3600s recommended) | Standard DNS caching |

### 8.2 Capability Card Serving

1. Servers MUST serve the Capability Card at the transport-specific endpoint.
2. Servers MUST return valid JSON conforming to the schema in Appendix A.
3. Servers MUST include all REQUIRED fields.
4. Servers SHOULD set `expires` to a reasonable future time (default: current_time + 300 seconds).
5. Servers MUST update the card when capabilities change (tools added/removed).
6. Servers SHOULD support HTTP conditional requests (ETag / If-None-Match) for card retrieval.

### 8.3 MCP Server Requirements

1. The MCP server MUST conform to the MCP specification (version 2025-03-26 or later).
2. The MCP server MUST implement `tools/list` which returns full tool schemas.
3. The MCP server SHOULD implement `initialize` with server info matching the Capability Card.
4. The MCP server MUST enforce authentication as declared in the Capability Card.

### 8.4 Lifecycle

```
┌──────────┐     ┌────────────┐     ┌───────────┐     ┌──────────┐
│  BOOT    │────►│ADVERTISING │────►│  SERVING  │────►│ SHUTDOWN │
└──────────┘     └────────────┘     └───────────┘     └──────────┘
                       │                   │
                       │ client connects   │ client disconnects
                       ▼                   ▼
                 ┌───────────┐      ┌───────────┐
                 │  RESOLVE  │      │MCP SESSION│
                 │  (card)   │      │  ACTIVE   │
                 └───────────┘      └───────────┘
```

On shutdown, servers SHOULD:
- Stop beacon advertisements immediately
- Allow active MCP sessions to complete or timeout (30 second grace period)
- Send mDNS goodbye packets (TTL=0) on WiFi/Ethernet

---

## 9. Security

### 9.1 Security Model

DMED separates discovery from access:
- **Discovery is public** — Beacons are broadcast openly. Anyone in range can see that a server exists.
- **Access is controlled** — Tool invocation requires authentication as specified in the Capability Card.

This mirrors physical-world patterns: you can see a shop exists (sign), but you need to interact with staff (auth) to get service.

### 9.2 Beacon Security

1. Beacons MUST NOT contain secrets, credentials, or PII.
2. Beacons MUST NOT contain enough information to invoke tools (no URLs, no tokens).
3. Servers MAY sign their Capability Card (see 9.4) to prove beacon authenticity.

### 9.3 Privacy

1. Servers that require tracking resistance SHOULD rotate `instance_id` every 15 minutes.
2. BLE servers SHOULD use Resolvable Private Addresses (RPA) per Bluetooth Core Spec.
3. Clients MUST use passive scanning — no client identity is transmitted during scan.
4. Capability Cards MUST NOT include PII in unauthenticated responses.
5. Servers MAY serve a minimal card to unauthenticated clients and a full card after auth.

### 9.4 Signed Capability Cards (OPTIONAL)

Servers MAY include a digital signature in the Capability Card to prove authenticity:

```json
{
  "dmed_version": "0.1.0",
  "instance_id": "a1b2c3d4",
  "name": "...",
  "...": "...",
  "_signature": {
    "algorithm": "Ed25519",
    "public_key": "base64url-encoded-32-bytes",
    "signature": "base64url-encoded-64-bytes",
    "signed_fields": ["instance_id", "name", "service_type", "transports", "capabilities"]
  }
}
```

Clients that validate signatures:
1. MUST verify the signature covers all `signed_fields`
2. MUST compute the signature over the canonical JSON serialization (sorted keys, no whitespace) of the signed fields
3. MAY cache the public key and alert the user if it changes (TOFU model)

### 9.5 Transport Security

| Transport | Encryption | Integrity | Authentication |
|-----------|-----------|-----------|----------------|
| BLE (beacon) | None (broadcast) | None | None |
| BLE (GATT card) | BLE encryption (optional) | BLE integrity | BLE pairing (optional) |
| HTTP (card) | None | None | None |
| HTTPS (card) | TLS 1.2+ | TLS | Server certificate |
| MCP over HTTP | None | None | Application-level |
| MCP over HTTPS | TLS 1.2+ | TLS | Server + app-level |
| MCP over WSS | TLS 1.2+ | TLS | Server + app-level |

**Recommendation:** For any server with `AUTH` flag set, the MCP endpoint SHOULD use HTTPS/WSS.

### 9.6 Threat Mitigations

| Threat | Attack | Mitigation |
|--------|--------|-----------|
| Impersonation | Attacker broadcasts beacon with victim's instance_id | Signed Capability Cards (9.4); TLS on card endpoint |
| Beacon spam | Attacker floods area with fake beacons | Client-side limit (max 100 discovered servers); rate limiting |
| Eavesdropping | Attacker reads MCP traffic | TLS on MCP session; BLE encryption |
| Replay | Attacker replays captured MCP requests | MCP session tokens; nonces in auth |
| Tracking | Observer tracks user by scanning patterns | Passive-only client scanning; no client beacons |
| DoS on server | Many clients resolve simultaneously | Server rate-limits card requests; connection limits |

---

## 10. Error Handling

### 10.1 Resolution Errors

| Error | Client Behavior |
|-------|----------------|
| Card endpoint unreachable | Retry 2x with 1s backoff, then mark server as unavailable |
| Card returns non-JSON | Mark server as incompatible, do not retry for 5 minutes |
| Card `instance_id` mismatch | Discard card, remove server from discovered list |
| Card `dmed_version` incompatible | Display "unsupported version" to user |
| Card missing required fields | Mark server as incompatible |
| HTTP 429 (rate limited) | Respect Retry-After header |
| HTTP 5xx | Retry 1x after 5s, then mark unavailable |

### 10.2 Connection Errors

| Error | Client Behavior |
|-------|----------------|
| MCP endpoint unreachable | Try next transport in priority order |
| Authentication failed | Display error to user; do not retry without user action |
| MCP handshake failed | Try next transport; if all fail, mark unavailable |
| Session dropped | Attempt reconnect 3x with exponential backoff (1s, 2s, 4s) |

### 10.3 Server Error Responses

When a server cannot serve a Capability Card, it MUST return an appropriate HTTP status:

| Status | Meaning |
|--------|---------|
| 200 | Success — card in response body |
| 304 | Not Modified (if ETag matches) |
| 429 | Too Many Requests — include Retry-After header |
| 503 | Service Unavailable — server is booting or shutting down |

---

## 11. Versioning & Extensibility

### 11.1 Protocol Version

- The beacon `version` field is 4 bits (values 0-15).
- This specification defines version `1`.
- Version `0` is reserved (MUST NOT be used).
- Versions 2-15 are reserved for future major revisions.

### 11.2 Capability Card Version

- The `dmed_version` field uses semantic versioning (MAJOR.MINOR.PATCH).
- Clients MUST accept cards where MAJOR matches their supported version.
- Clients MUST ignore unknown fields (forward compatibility).
- Servers MUST NOT remove fields between MINOR versions (backward compatibility).

### 11.3 Extension Mechanism

Vendors MAY add custom fields to the Capability Card using the `x-` prefix:

```json
{
  "dmed_version": "0.1.0",
  "name": "My Device",
  "x-vendor-firmware": "3.2.1",
  "x-vendor-battery": 85
}
```

Clients MUST ignore fields they do not understand. Clients MUST NOT reject cards containing unknown fields.

### 11.4 Service Type Extension

New service types (Appendix E) MAY be added without a protocol version bump. Clients that encounter an unknown `service_type` MUST treat it as "unknown" and MAY still resolve and connect.

---

## 12. Conformance

### 12.1 Conformance Levels

| Level | Name | Requirements |
|-------|------|-------------|
| 1 | **DMED Server — Minimal** | Beacon on ≥1 transport + Capability Card + MCP server |
| 2 | **DMED Server — Full** | Beacon on ≥2 transports + signed card + all optional fields |
| 3 | **DMED Client — Minimal** | Scan ≥1 transport + resolve card + establish MCP session |
| 4 | **DMED Client — Full** | Scan all transports + deduplication + caching + UI requirements |

### 12.2 Interoperability Requirements

1. A conformant DMED Client MUST be able to discover and connect to any conformant DMED Server, regardless of transport (as long as at least one common transport exists).
2. A conformant DMED Server MUST serve a valid Capability Card to any client that requests it (subject to rate limiting).
3. Implementations MUST NOT require proprietary extensions for basic discovery and connection.

### 12.3 Test Vectors

Implementations SHOULD validate against the test vectors in Appendix F.
