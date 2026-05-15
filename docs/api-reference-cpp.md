# API Reference — C++ Library (`dmed.hpp`)

Header-only C++17 library. Just include:
```cpp
#include "dmed.hpp"
using namespace dmed;
```

---

## Namespace: `dmed`

All types and functions live in the `dmed` namespace.

## Constants

```cpp
constexpr uint8_t PROTOCOL_VERSION = 1;
constexpr size_t  BEACON_MIN_SIZE = 6;
constexpr size_t  BEACON_MAX_SIZE = 9;
```

## Enums

### `Flag` (uint8_t, bitmask)

```cpp
enum Flag : uint8_t { AUTH=1, TLS=2, MULTI=4, CLOUD=8 };
```

Combine with `|`: `Flag::MULTI | Flag::TLS`

### `ServiceType` (uint8_t)

```cpp
enum class ServiceType : uint8_t {
    Unknown=0x00, IotDevice=0x01, Media=0x02, Appliance=0x03,
    Vehicle=0x04, RetailKiosk=0x05, Infrastructure=0x06, Computing=0x07,
    AiService=0x08, DataSource=0x09, ToolUtility=0x0A, Communication=0x0B,
    HealthMedical=0x0C, Industrial=0x0D, Environmental=0x0E, Security=0x0F,
    Information=0x10, Energy=0x11, Custom=0xFF,
};
```

---

## Classes

### `Beacon`

```cpp
struct Beacon {
    uint8_t     version = PROTOCOL_VERSION;
    uint8_t     flags = 0;
    ServiceType service_type = ServiceType::Unknown;
    uint32_t    instance_id = 0;
    std::optional<int8_t>   tx_power;
    std::optional<uint16_t> name_hash;

    size_t encode(uint8_t* buf, size_t len) const;
    static std::optional<Beacon> decode(const uint8_t* buf, size_t len);
};
```

#### `Beacon::encode`

```cpp
size_t encode(uint8_t* buf, size_t len) const;
```

Encode beacon to binary. Returns bytes written, or 0 if buffer too small.

```cpp
Beacon b{.version=1, .flags=MULTI, .service_type=ServiceType::IotDevice,
         .instance_id=0xA1B2C3D4, .tx_power=-20};
uint8_t buf[BEACON_MAX_SIZE];
size_t len = b.encode(buf, sizeof(buf));  // len = 7
```

#### `Beacon::decode`

```cpp
static std::optional<Beacon> decode(const uint8_t* buf, size_t len);
```

Decode binary to Beacon. Returns `std::nullopt` on invalid data.

```cpp
auto b = Beacon::decode(buf, len);
if (b) {
    printf("ID: %08X, type: %s\n", b->instance_id,
           std::string(service_type_str(b->service_type)).c_str());
}
```

---

### `Tool`

```cpp
struct Tool {
    std::string name;
    std::string description;
};
```

### `Transport`

```cpp
struct Transport {
    std::string type;   // "http", "https", "ws", "wss", "ble_gatt"
    std::string url;
    int priority = 10;  // lower = preferred
};
```

### `Card`

```cpp
struct Card {
    std::string instance_id;
    std::string name;
    std::string description;
    std::string service_type;
    std::vector<Transport> transports;
    std::string auth_type = "none";
    std::vector<Tool> tools;

    std::string to_json() const;
};
```

#### `Card::to_json`

Returns a valid DMED Capability Card JSON string. No external JSON library needed.

```cpp
Card card{
    .instance_id = "a1b2c3d4",
    .name = "My Sensor",
    .service_type = "environmental",
    .transports = {{"https", "https://sensor.local:443/mcp", 1}},
    .auth_type = "none",
    .tools = {{"get_temperature", "Read current temperature in Celsius"}}
};
std::string json = card.to_json();
// Serve this at /dmed/card
```

---

## Free Functions

### `crc16`

```cpp
uint16_t crc16(const void* data, size_t len);
uint16_t crc16(std::string_view s);
```

CRC-16/CCITT for name hashing.

```cpp
uint16_t hash = crc16("Kitchen Light");
```

### `instance_id_from`

```cpp
uint32_t instance_id_from(std::string_view s);
```

Generate stable 32-bit instance ID from any string (CRC-32).

```cpp
uint32_t id = instance_id_from("device-serial-XYZ");
```

### `service_type_str`

```cpp
std::string_view service_type_str(ServiceType st);
```

Convert enum to string: `ServiceType::IotDevice` → `"iot_device"`.

---

## Memory & Performance

- **Zero heap for beacons** — encode/decode uses only stack + caller buffer
- **Card uses std::string** — heap allocated, but minimal
- **No dependencies** — only C++17 standard library
- **Code size** — ~3KB compiled
- **Thread-safe** — no global/static mutable state
