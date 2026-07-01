# DMED Scanner — Android BLE App

A minimal Android app that discovers DMED-enabled devices via BLE beacons and reads their capability cards over GATT.

## Features

- **BLE Scan** — Discovers devices advertising the DMED beacon UUID (`D14D0000-...`)
- **GATT Card Read** — Connects to device and reads capability card from DMED Card Service
- **Device List** — Shows discovered devices with name, icon, BLE address
- **Chat UI** — Tap a device → read card via BLE → send actions via HTTP (if transport advertised)

## How It Works

```
[BLE Scan] → discovers D14D0000 beacon advertisements
  ↓
[Select device] → GATT connect → read Card Length + Card Data characteristics
  ↓
[Chat] → shows tools as quick-action chips
       → sends POST /dmed/action over HTTP (from card's transport field)
       → displays response in chat bubbles
```

## BLE UUIDs (DMED Spec Appendix B)

| Purpose | UUID |
|---------|------|
| Beacon Service (advertisement) | `D14D0000-1000-4000-8000-00805F9B34FB` |
| Card Service (GATT) | `D14D0001-1000-4000-8000-00805F9B34FB` |
| Card Data Characteristic | `D14D0002-1000-4000-8000-00805F9B34FB` |
| Card Length Characteristic | `D14D0003-1000-4000-8000-00805F9B34FB` |

## Build

```bash
./gradlew assembleDebug
```

## Requirements

- Android 8.0+ (API 26)
- BLE hardware required
- Location permission (for BLE scanning)
- BLUETOOTH_SCAN + BLUETOOTH_CONNECT (Android 12+)

## Testing

1. Start a DMED BLE endpoint (device advertising DMED beacon)
2. Open the app on your phone
3. Tap "Scan for DMED BLE Devices"
4. Tap a discovered device — card is read via GATT
5. Use action chips or type action names (sent over HTTP if card includes transport info)

## Project Structure

```
app/src/main/java/com/dmed/scanner/
├── MainActivity.kt              # Entry point + BLE permission handling
├── data/Models.kt               # Data classes (endpoint, card, transport, chat)
├── discovery/DmedDiscovery.kt   # BLE scanner for DMED beacon UUID
├── network/
│   ├── BleCardReader.kt         # GATT client — reads card from device
│   └── DmedClient.kt            # HTTP client for actions
└── ui/
    ├── MainViewModel.kt         # State management
    ├── ScannerScreen.kt         # Device list with BLE scan button
    └── ChatScreen.kt            # Chat bubbles + action chips
```
