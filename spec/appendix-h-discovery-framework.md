# Appendix H: DMED Discovery Framework (Tier 2)

## H.1 Overview

The Discovery Framework is a multi-transport SDK that provides a unified API for finding and interacting with DMED devices regardless of their physical transport. From the application's perspective, transport is an implementation detail — the framework handles BLE scanning, mDNS browsing, and any other supported transport simultaneously, and surfaces a single deduplicated device list.

This appendix defines the required behaviour for any conformant Discovery Framework implementation.

---

## H.2 Supported Transports

A Discovery Framework implementation MUST support at least two of the following transports simultaneously:

| Transport | Discovery Mechanism | Card Fetch | Action Dispatch |
|---|---|---|---|
| **BLE** | Advertisement with DMED Beacon UUID (`D14D0000-...`) | GATT read (Appendix B) | GATT write/notify |
| **mDNS / DNS-SD** | `_dmed._tcp` service browse (Appendix C) | HTTP GET `/dmed/card` | HTTP POST `/dmed/action` |
| **Ethernet / IP** | mDNS over wired LAN (same as mDNS) | HTTP GET `/dmed/card` | HTTP POST `/dmed/action` |
| **Thread/Matter** | Border router bridge → mDNS on LAN | HTTP GET `/dmed/card` | HTTP POST `/dmed/action` |
| **Zigbee/Z-Wave** | Hub bridge → DMED adapter on LAN | HTTP GET `/dmed/card` | HTTP POST `/dmed/action` |
| **USB/Serial** | Adapter daemon → local HTTP | HTTP GET `/dmed/card` | HTTP POST `/dmed/action` |

Transport bridges (Thread→mDNS, Zigbee→HTTP) are valid conformant implementations.

---

## H.3 Unified Device Model

All discovered devices, regardless of transport, are represented with the same model:

```
DmedDevice {
  instance_id   string    // From beacon payload or capability card; used for deduplication
  name          string    // Human-readable name
  transports    list      // All transports on which this device was found
  address       string    // Transport-specific address of the best available transport
  card          DmedCard? // Capability card; populated after connect()
}
```

The `instance_id` is the primary identity. Two discovery events with the same `instance_id` refer to the same physical device regardless of which transport found them.

---

## H.4 Deduplication

When the same device is visible on multiple transports simultaneously (e.g. BLE + mDNS), the framework MUST present it as a single `DmedDevice`, not two separate entries.

**Deduplication rules:**

1. If a device is found via BLE with `instance_id = "a1b2c3d4"`, it is added to the device list.
2. If the same `instance_id` is later found via mDNS, the existing entry is updated to add the mDNS transport to its transport list.
3. The device appears once in the device list with both transports available.
4. If `instance_id` is empty (beacon did not include one), deduplication falls back to the transport-level address. Two devices with empty instance IDs are never deduplicated unless their addresses match exactly.

---

## H.5 Transport Selection

When dispatching an action or fetching a capability card, the framework MUST select the best available transport using the following algorithm:

1. Filter to transports that are currently reachable.
2. Among reachable transports, sort by `priority` (ascending — lower number = higher priority).
3. Select the first entry.
4. If the selected transport fails, retry with the next in the list.
5. If all transports fail, return a `NoTransportError`.

Priority values are defined in the device's Capability Card `transports` array. If no priority is specified, default to `10`.

**Example:**

```json
"transports": [
  { "type": "http",    "url": "http://192.168.1.42:3100/mcp", "priority": 1 },
  { "type": "ble_gatt","service_uuid": "D14D0001-...",        "priority": 2 }
]
```

HTTP is preferred (priority 1). If the device leaves WiFi range, the framework automatically falls back to BLE (priority 2).

---

## H.6 Canonical API

The following API MUST be provided by any conformant Discovery Framework implementation. Language-specific naming conventions apply (e.g. `startScan` in Java/Kotlin, `start_scan` in Python, `start()` in TypeScript).

### H.6.1 Discovery Lifecycle

```
DmedDiscovery.scan()
  Starts all configured transport listeners simultaneously.
  Begins emitting device events.

DmedDiscovery.stop()
  Stops all transport listeners.
  No further device events are emitted.
```

### H.6.2 Device Events

```
event: "device"  (device: DmedDevice)
  Fired when a new device is found (first sighting of a given instance_id).
  Not fired for updates to an existing device's transport list.

event: "lost"    (device: DmedDevice)
  Fired when a device is no longer reachable on any transport.
  For mDNS: fired on goodbye packet or TTL expiry.
  For BLE: fired when the device is no longer advertising.
```

### H.6.3 Connect

```
DmedDiscovery.connect(device: DmedDevice) → DmedDevice
  Fetches the Capability Card from the device over the best available transport.
  Returns the device with card populated.
  Throws CardFetchError if the card cannot be retrieved.
```

### H.6.4 Dispatch

```
DmedDiscovery.dispatch(device: DmedDevice, action: string, params: object) → ActionResponse
  Sends an action to the device over the best available transport.
  Selects transport by priority (see H.5).
  Retries on the next transport if the selected one fails.
  Throws NoTransportError if all transports fail.
  Throws UnknownActionError if the device returns HTTP 404.
```

---

## H.7 AI Brain (Optional Extension)

A Discovery Framework implementation MAY include an optional AI layer that provides natural language interaction with discovered devices. When present:

1. The AI layer receives the device's Capability Card (tools + descriptions + input schemas).
2. The user's natural language input is sent to an AI model alongside the capability card.
3. The AI model either returns a text response or a tool call.
4. If a tool call: the framework dispatches the action and returns the result to the AI for narration.
5. The AI's narrated response is returned to the application.

This layer is optional and above the transport/dispatch layer. The core Discovery Framework operates identically with or without it.

---

## H.8 Conformance Requirements

A **DMED v0.2 compliant Discovery Framework** MUST:

1. Support simultaneous scanning on at least two transports
2. Implement deduplication by `instance_id` (H.4)
3. Implement transport selection by priority with fallback (H.5)
4. Provide all four canonical API operations: `scan`, `stop`, `connect`, `dispatch` (H.6)
5. Emit `device` and `lost` events correctly
6. Parse and validate Capability Cards per Appendix A schema
7. Handle `NoTransportError`, `CardFetchError`, and `UnknownActionError` gracefully

A **DMED v0.2 compliant Discovery Framework** SHOULD:

1. Support all six transports listed in H.2
2. Implement transport fallback on failure (not just priority selection at start)
3. Refresh device lists when transport TTLs expire
4. Expose transport metadata on `DmedDevice` (which transports found this device)

---

## H.9 Reference Implementations

| Platform | Transports | Location |
|---|---|---|
| Android (Kotlin) | BLE + mDNS | `android/dmed-scanner-working/` |
| JavaScript/TypeScript | mDNS + HTTP | `lib/js/` |
| Python | mDNS + HTTP | `lib/python/` |
| C (Linux) | BLE (BlueZ/D-Bus) | `examples/bleclient-dmed-working/` |
