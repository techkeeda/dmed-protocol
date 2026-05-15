# Tutorial: Share on Bluetooth (BLE)

Make a device discoverable via Bluetooth Low Energy — ideal for IoT, wearables, and battery-powered devices.

---

## How It Works

```
Your Device (ESP32/nRF52)           Phone / Laptop
┌─────────────────────┐            ┌─────────────────┐
│ BLE Advertisement   │───radio───►│ BLE Scan        │
│ (7-byte DMED beacon) │            │                 │
│                     │            │ Sees beacon     │
│ GATT Card Service   │◄──────────│ Reads card      │
│                     │            │                 │
│ WiFi MCP endpoint   │◄══════════│ MCP over HTTP   │
│ (or GATT MCP)       │            │                 │
└─────────────────────┘            └─────────────────┘
```

BLE is used for **discovery only** — the actual MCP session typically happens over WiFi (higher bandwidth). BLE tells the client "I exist, here's how to reach me over WiFi."

## BLE Beacon Structure

The DMED beacon fits in a standard BLE advertisement (31 bytes total):

```
AD Struct 1: Flags (3 bytes)
AD Struct 2: 128-bit Service UUID (18 bytes)
AD Struct 3: Service Data — DMED payload (7-9 bytes)
─────────────────────────────────────────────────
Total: 28-30 bytes (fits in single advertisement)
```

DMED payload (7 bytes minimum):
```
Byte 0:    [version:4][flags:4]     = 0x14 (v1, MULTI flag)
Byte 1:    service_type             = 0x01 (iot_device)
Byte 2-5:  instance_id (big-endian) = 0xA1B2C3D4
Byte 6:    tx_power (dBm)           = 0xEC (-20)
```

## UUIDs

| Purpose | UUID |
|---------|------|
| Beacon Service | `D14D0000-1000-4000-8000-00805F9B34FB` |
| Card GATT Service | `D14D0001-1000-4000-8000-00805F9B34FB` |
| Card Data Characteristic | `D14D0002-1000-4000-8000-00805F9B34FB` |
| Card Length Characteristic | `D14D0003-1000-4000-8000-00805F9B34FB` |

## Implementation: ESP32 (Arduino)

```cpp
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Use the C library for beacon encoding
#define DMED_IMPLEMENTATION
#include "dmed.h"

void setup() {
    BLEDevice::init("My Sensor");

    // Build beacon
    dmed_beacon_t beacon = {
        .version = 1,
        .flags = DMED_FLAG_MULTI,
        .service_type = DMED_ST_IOT_DEVICE,
        .instance_id = dmed_instance_id_from_string("esp32-sensor-001"),
        .tx_power = -20,
        .has_tx_power = 1,
    };
    uint8_t payload[DMED_BEACON_MAX_SIZE];
    int len = dmed_beacon_encode(&beacon, payload, sizeof(payload));

    // Advertise
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    BLEAdvertisementData data;
    data.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    data.setManufacturerData(std::string((char*)payload, len));
    adv->setAdvertisementData(data);

    // GATT service for capability card
    BLEServer *server = BLEDevice::createServer();
    BLEService *svc = server->createService("D14D0001-1000-4000-8000-00805F9B34FB");

    // Card JSON (point to WiFi endpoint)
    const char *card_json = "{\"dmed_version\":\"0.1.0\",\"instance_id\":\"...\","
        "\"name\":\"My Sensor\",\"service_type\":\"iot_device\","
        "\"transports\":[{\"type\":\"http\",\"url\":\"http://192.168.1.50:8080/mcp\"}],"
        "\"auth\":{\"type\":\"none\"},\"capabilities\":{\"tools\":["
        "{\"name\":\"get_temp\",\"description\":\"Get temperature\"}],"
        "\"resources\":[],\"prompts\":[]}}";

    BLECharacteristic *ch = svc->createCharacteristic(
        "D14D0002-1000-4000-8000-00805F9B34FB", BLECharacteristic::PROPERTY_READ);
    ch->setValue(card_json);
    svc->start();
    adv->start();
}

void loop() { delay(1000); }
```

## Implementation: Linux (BlueZ + C)

For Linux devices with BlueZ:

```bash
# Advertise DMED beacon using hcitool (quick test)
sudo hcitool -i hci0 cmd 0x08 0x0008 1E 02 01 06 11 07 \
  FB349B5F0800 0080 0040 0010 0000 4DD1 \
  08 FF 14 01 A1 B2 C3 D4 EC
```

For production, use the BlueZ D-Bus API or `btmgmt` to register advertisements programmatically.

## Client: Scanning for BLE DMED Beacons

### Python (using `bleak`)

```python
import asyncio
from bleak import BleakScanner
from dmed import Beacon

DMED_SERVICE_UUID = "d14d0000-1000-4000-8000-00805f9b34fb"

async def scan():
    devices = await BleakScanner.discover(timeout=5)
    for d in devices:
        if DMED_SERVICE_UUID in [str(u).lower() for u in (d.metadata.get("uuids") or [])]:
            # Found DMED device — parse manufacturer data as beacon
            mfr_data = d.metadata.get("manufacturer_data", {})
            for key, value in mfr_data.items():
                beacon = Beacon.decode(bytes(value))
                print(f"Found: {d.name} (id={beacon.instance_id_hex})")

asyncio.run(scan())
```

### iOS (CoreBluetooth)

```swift
// Scan for DMED service UUID
let dmedUUID = CBUUID(string: "D14D0000-1000-4000-8000-00805F9B34FB")
centralManager.scanForPeripherals(withServices: [dmedUUID])

// In didDiscover delegate:
// Parse advertisementData[CBAdvertisementDataManufacturerDataKey] as DMED beacon
```

### Android (Kotlin)

```kotlin
val DMED_UUID = ParcelUuid.fromString("D14D0000-1000-4000-8000-00805F9B34FB")
val filter = ScanFilter.Builder().setServiceUuid(DMED_UUID).build()
bluetoothLeScanner.startScan(listOf(filter), settings, callback)
```

## Design Decisions

| Choice | Rationale |
|--------|-----------|
| BLE for discovery only | BLE bandwidth too low for rich MCP interaction |
| WiFi handoff | Card contains HTTP URL; client switches to WiFi for MCP |
| 7-byte beacon | Fits in BLE adv with room for UUID + flags |
| GATT for card | Works even without WiFi (direct BLE connection) |

## Power Considerations

| Adv Interval | Battery Life (CR2032) | Discoverability |
|-------------|----------------------|-----------------|
| 100ms | ~1 month | Instant (< 1s) |
| 500ms | ~6 months | Fast (< 3s) |
| 2000ms | ~2 years | Slow (< 10s) |

Recommendation: 500ms for always-on devices, 100ms for first 30s after boot then reduce.
