# Getting Started with DMED

Make anything discoverable by AI assistants in 5 minutes.

---

## What You'll Build

A DMED server that broadcasts its existence on your local network. Any DMED client (phone, laptop, AI assistant) will automatically find it and can interact with it via MCP.

## Prerequisites

Pick your language:

| Language | Requirements |
|----------|-------------|
| Python | Python 3.8+, `pip install zeroconf flask` |
| C | GCC, Linux/macOS |
| C++ | G++ with C++17, Linux/macOS |

## Option A: Python (Fastest)

```python
from dmed import Card, Tool, Transport, DMEDServer

# 1. Define what you're sharing
card = Card(
    instance_id="deadbeef",
    name="My First DMED Server",
    service_type="tool_utility",
    description="A simple calculator you can talk to",
    transports=[Transport("http")],
    tools=[
        Tool("add", "Add two numbers"),
        Tool("multiply", "Multiply two numbers"),
    ]
)

# 2. Implement your tools
def handle(name, args):
    if name == "add":
        return args["a"] + args["b"]
    if name == "multiply":
        return args["a"] * args["b"]

# 3. Start (auto-advertises on local network)
DMEDServer(card, port=8080, tool_handler=handle).start()
```

Run it:
```bash
cd lib/python
pip install zeroconf flask
python3 -c "
from dmed import *
card = Card('deadbeef', 'My Calculator', 'tool_utility',
            transports=[Transport('http')],
            tools=[Tool('add','Add two numbers'), Tool('multiply','Multiply two numbers')])
DMEDServer(card, 8080, lambda n,a: a['a']+a['b'] if n=='add' else a['a']*a['b']).start()
"
```

## Option B: C (Embedded / Minimal)

```c
#define DMED_IMPLEMENTATION
#include "dmed.h"

// Encode a beacon to broadcast
dmed_beacon_t beacon = {
    .version = 1,
    .flags = DMED_FLAG_MULTI,
    .service_type = DMED_ST_TOOL_UTILITY,
    .instance_id = dmed_instance_id_from_string("my-calculator"),
};
uint8_t buf[9];
int len = dmed_beacon_encode(&beacon, buf, sizeof(buf));
// → Send `buf` over BLE advertisement or embed in mDNS TXT
```

See `examples/thought_stream.c` for a complete C server with HTTP.

## Option C: C++ (Desktop / Server)

```cpp
#include "dmed.hpp"
using namespace dmed;

Beacon b{.version=1, .flags=MULTI, .service_type=ServiceType::ToolUtility,
         .instance_id=instance_id_from("my-calculator")};
auto [buf, len] = b.encode();
// → Send buf over your transport

Card card{.instance_id="deadbeef", .name="My Calculator", .service_type="tool_utility",
          .transports={{"http","http://192.168.1.10:8080/mcp",1}},
          .tools={{"add","Add two numbers"},{"multiply","Multiply two numbers"}}};
std::string json = card.to_json();
// → Serve this JSON at /dmed/card
```

## Verify It Works

From another terminal:

```bash
# Discover (if using Python server with mDNS)
avahi-browse -r _dmed._tcp

# Or directly fetch the card
curl http://localhost:8080/dmed/card | jq .

# List available actions (v0.2)
curl http://localhost:8080/dmed/actions | jq .

# Send a lightweight action (v0.2)
curl -X POST http://localhost:8080/dmed/action \
  -H "Content-Type: application/json" \
  -d '{"action":"add","params":{"a":5,"b":3}}'

# Or use full MCP JSON-RPC
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"add","arguments":{"a":5,"b":3}}}'
```

Expected output (lightweight action):
```json
{"status":"ok","action":"add","result":{"text":"8"}}
```

Expected output (full MCP):
```json
{"jsonrpc":"2.0","id":1,"result":{"content":[{"type":"text","text":"8"}]}}
```

## What Just Happened?

```
1. Your server registered "_dmed._tcp" on the local network (mDNS)
2. Any DMED client scanning the network sees your endpoint
3. Client fetches your Capability Card (GET /dmed/card)
4. Client lists your actions (GET /dmed/actions)
5. Client sends actions (POST /dmed/action) — lightweight, no session needed
6. Or: Client establishes full MCP session (POST /mcp) for advanced use
```

## Next Steps

- [Share on Local WiFi](tutorials/share-on-local-wifi.md) — detailed mDNS setup
- [Share on Bluetooth](tutorials/share-on-bluetooth.md) — BLE beacon for IoT
- [Share on Internet](tutorials/share-on-internet.md) — DNS TXT for cloud services
- [Build a Client](tutorials/build-a-client.md) — scan, discover, connect
- [Cookbook](cookbook.md) — 10+ real-world recipes
