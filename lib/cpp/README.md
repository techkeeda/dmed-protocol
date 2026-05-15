# DMED C++ Library (v0.2)

Header-only C++17 library for DMED beacon encode/decode, Capability Card generation, and action interaction.

## Features
- **Header-only** — just `#include "dmed.hpp"`
- **Zero heap for beacons** — stack-only encode/decode
- **Type-safe** — enums for ServiceType, std::optional for optional fields
- **Card builder** — generates valid JSON without external JSON library
- **v0.2: ActionRequest/Response** — structs for lightweight interaction
- **~3KB compiled**

## Install

Copy `include/dmed.hpp` into your project.

## Usage

```cpp
#include "dmed.hpp"
using namespace dmed;

// Encode beacon
Beacon b{.version=1, .flags=MULTI, .service_type=ServiceType::IotDevice,
         .instance_id=0xA1B2C3D4, .tx_power=-20};
uint8_t buf[9];
size_t len = b.encode(buf, sizeof(buf));

// Decode beacon
auto decoded = Beacon::decode(buf, len);
if (decoded) { /* use decoded->instance_id etc */ }

// Build capability card JSON
Card card{.instance_id="a1b2c3d4", .name="My Light", .service_type="iot_device",
          .transports={{"http","http://192.168.1.42:8080/mcp",1}},
          .tools={{"toggle","Toggle on/off"}}};
std::string json = card.to_json();
```

## Build test

```bash
cd src
g++ -std=c++17 -o dmed_test dmed_test.cpp -I../include && ./dmed_test
```

## v0.2: Action Interaction

```cpp
#include "dmed.hpp"
using namespace dmed;

// Build action request
ActionRequest req{.action = "brew_coffee", .params_json = R"({"size":"large"})"};
std::string body = req.to_json();
// → POST body to http://<endpoint>/dmed/action

// Parse response (after HTTP call)
ActionResponse resp{.ok = true, .action = "brew_coffee", .result_text = "☕ Ready!"};
```
