# Appendix B: BLE Transport Detail

## B.1 UUID Assignments

Until official Bluetooth SIG registration, implementations MUST use these 128-bit UUIDs:

| Purpose | UUID |
|---------|------|
| DMED Beacon Service (advertisement) | `D14D0000-1000-4000-8000-00805F9B34FB` |
| DMED Card Service (GATT) | `D14D0001-1000-4000-8000-00805F9B34FB` |
| Card Characteristic (read) | `D14D0002-1000-4000-8000-00805F9B34FB` |
| Card Length Characteristic (read) | `D14D0003-1000-4000-8000-00805F9B34FB` |

The prefix `D14D` is a mnemonic for "DMED" (D=D, 1=M, 4=D in leetspeak).

## B.2 Advertisement PDU Structure

DMED servers MUST advertise using **ADV_IND** (connectable undirected) or **ADV_NONCONN_IND** (non-connectable undirected) PDUs.

### Advertisement Data (31 bytes max)

```
┌─────────────────────────────────────────────────────────────────┐
│ AD Structure 1: Flags (3 bytes)                                  │
│   Length: 0x02                                                    │
│   Type:   0x01 (Flags)                                           │
│   Value:  0x06 (LE General Discoverable + BR/EDR Not Supported)  │
├─────────────────────────────────────────────────────────────────┤
│ AD Structure 2: Complete 128-bit Service UUID (18 bytes)          │
│   Length: 0x11                                                    │
│   Type:   0x07 (Complete List of 128-bit Service UUIDs)          │
│   Value:  D14D0000-1000-4000-8000-00805F9B34FB (little-endian)  │
├─────────────────────────────────────────────────────────────────┤
│ AD Structure 3: Service Data (9-11 bytes)                        │
│   Length: 0x08-0x0A                                              │
│   Type:   0x21 (Service Data - 128-bit UUID) — OR —             │
│           0xFF (Manufacturer Specific Data)                       │
│   Value:  DMED Beacon payload (see below)                         │
└─────────────────────────────────────────────────────────────────┘
```

### Beacon Payload (within Service Data)

```
Byte 0:    [version:4 bits][flags:4 bits]
Byte 1:    service_type (uint8)
Byte 2-5:  instance_id (uint32, big-endian)
Byte 6:    tx_power (int8, dBm) — OPTIONAL
Byte 7-8:  name_hash (uint16, big-endian, CRC-16/CCITT) — OPTIONAL
```

### Scan Response Data (31 bytes max)

```
┌─────────────────────────────────────────────────────────────────┐
│ AD Structure: Complete Local Name (up to 29 bytes)               │
│   Length: varies                                                  │
│   Type:   0x09 (Complete Local Name)                             │
│   Value:  UTF-8 encoded server name (truncated to fit)           │
└─────────────────────────────────────────────────────────────────┘
```

## B.3 GATT Service Definition

After a client discovers the beacon and wants to resolve the Capability Card:

### Service: DMED Card Service (`D14D0001-...`)

| Characteristic | UUID | Properties | Description |
|---------------|------|-----------|-------------|
| Card Length | `D14D0003-...` | Read | uint32 (big-endian): total card JSON size in bytes |
| Card Data | `D14D0002-...` | Read | Card JSON, read with offset for chunked transfer |

### Reading Large Capability Cards

BLE ATT MTU is typically 23-517 bytes. For cards larger than (MTU - 3) bytes:

1. Client reads `Card Length` characteristic to get total size.
2. Client reads `Card Data` using **Read Long Characteristic Value** (ATT Read Blob Request) with increasing offsets.
3. Client concatenates all chunks.
4. Client parses the complete JSON.

**Pseudocode:**
```
card_length = read_characteristic(CARD_LENGTH_UUID)  // uint32
card_bytes = []
offset = 0
while offset < card_length:
    chunk = read_blob(CARD_DATA_UUID, offset)
    card_bytes.append(chunk)
    offset += len(chunk)
    if len(chunk) < (mtu - 5):  // short read = last chunk
        break
card_json = utf8_decode(card_bytes)
card = json_parse(card_json)
```

## B.4 Connection Parameters

Recommended BLE connection parameters for card retrieval:

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Connection Interval | 15-30 ms | Fast card transfer |
| Slave Latency | 0 | Responsive |
| Supervision Timeout | 4000 ms | Tolerant of brief interference |
| MTU | Request 512 | Minimize round-trips for card |

After card retrieval, the BLE connection SHOULD be terminated if the MCP session will use a different transport (e.g., HTTP over WiFi).

## B.5 Advertisement Interval

| Use Case | Interval | Power Impact |
|----------|----------|-------------|
| High discoverability (retail, public) | 100-200 ms | Higher power |
| Normal (home IoT) | 300-500 ms | Moderate |
| Low power (battery devices) | 1000-2000 ms | Minimal |

Servers SHOULD use 200ms for the first 30 seconds after boot (fast discovery), then reduce to their steady-state interval.

## B.6 BLE Security

| Phase | Security Level |
|-------|---------------|
| Beacon broadcast | None (public) |
| GATT card read (auth=none) | No pairing required |
| GATT card read (auth=pin) | LE Secure Connections pairing with passkey |
| MCP over BLE GATT | Encrypted link (LE Secure Connections) |

For servers with `auth.type = "none"`, the GATT card characteristic MUST be readable without pairing.

For servers with `auth.type = "pin"`, the server MAY require BLE pairing before card read, using the PIN as the passkey.

## B.7 Example: Complete BLE Advertisement (Hex Dump)

A smart light bulb advertising DMED:

```
# Advertisement Data (28 bytes)
02 01 06                          # Flags: LE General Discoverable
11 07 FB349B5F0800 0080 0040 0010 0000 4DD1  # 128-bit UUID (little-endian)
08 21 FB349B5F0800 0080 0040 0010 0000 4DD1  # Service Data header
   10 01 A1B2C3D4 EC                         # Beacon: v=1,fl=0,st=01,id=A1B2C3D4,tx=-20

# Scan Response (18 bytes)
11 09 4B 69 74 63 68 65 6E 20 4C 69 67 68 74  # "Kitchen Light"
```
