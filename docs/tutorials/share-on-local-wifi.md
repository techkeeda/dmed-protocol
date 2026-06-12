# Tutorial: Share Anything on Local WiFi

Make any service, device, or data source discoverable on your local network using mDNS/DNS-SD.

---

## How It Works

```
Your Device                          Any DMED Client
┌─────────────┐                     ┌─────────────┐
│ mDNS beacon │──── WiFi/LAN ──────►│ mDNS scan   │
│ HTTP card   │◄────────────────────│ GET /card   │
│ MCP server  │◄═══════════════════►│ MCP session │
└─────────────┘                     └─────────────┘
```

Anyone on the same WiFi/Ethernet network automatically discovers your service.

## Step 1: Decide What You're Sharing

Ask yourself:
- What **tools** does my service offer? (actions someone can invoke)
- What **resources** does it expose? (data someone can read)
- Does it need **authentication**?

Examples:
| What | Tools | Auth |
|------|-------|------|
| Temperature sensor | `get_temperature`, `get_history` | None |
| File server | `list_files`, `read_file`, `search` | PIN |
| Home automation | `lights_on`, `set_thermostat`, `lock_door` | PIN |
| Local LLM | `generate`, `summarize`, `embed` | None |

## Step 2: Pick Your Service Type

Choose from the [service type registry](../spec/appendix-e-service-types.md):

| Your thing | Service type |
|-----------|-------------|
| Sensor/IoT | `iot_device` |
| AI/ML model | `ai_service` |
| Database/API | `data_source` |
| Calculator/converter | `tool_utility` |
| Media player | `media` |
| Info display | `information` |

## Step 3: Implement (Python — Easiest)

```python
from dmed import Card, Tool, Transport, DMEDServer

# Define your card
card = Card(
    instance_id="aabb1122",          # unique 8-char hex
    name="Living Room Sensor",       # what users see
    service_type="iot_device",
    description="Temperature and humidity sensor",
    transports=[Transport("http")],  # URL auto-filled on start
    tools=[
        Tool("get_temperature", "Current temperature in Celsius"),
        Tool("get_humidity", "Current relative humidity %"),
    ]
)

# Implement your tools
import random
def handle(name, args):
    if name == "get_temperature":
        return f"{20 + random.random() * 5:.1f}°C"
    if name == "get_humidity":
        return f"{40 + random.random() * 20:.0f}%"
    raise ValueError(f"Unknown tool: {name}")

# Start — automatically advertises on network
DMEDServer(card, port=8080, tool_handler=handle).start()
```

## Step 3 (Alternative): Implement in C

```c
#define DMED_IMPLEMENTATION
#include "dmed.h"
// See examples/thought_stream.c for complete HTTP server in C
```

## Step 4: Verify Discovery

From another machine on the same network:

```bash
# Linux
avahi-browse -r _dmed._tcp

# macOS
dns-sd -B _dmed._tcp local.

# Or use the DMED Python scanner
python3 -c "from dmed import DMEDScanner; print(DMEDScanner().scan())"
```

## Step 5: Test MCP Interaction

```bash
# Fetch card
curl http://<your-ip>:8080/dmed/card | jq .

# Call a tool
curl -X POST http://<your-ip>:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call",
       "params":{"name":"get_temperature","arguments":{}}}'
```

## Network Requirements

- Both devices on the same subnet (or mDNS relay configured)
- UDP port 5353 open (mDNS multicast)
- TCP port of your server open (e.g., 8080)
- No AP isolation on WiFi (some guest networks block this)

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Not discovered | Check firewall allows UDP 5353 |
| Card fetch fails | Check TCP port is open |
| Works on same machine, not across | Check AP isolation / subnet |
| Duplicate names | Change your `name` — mDNS requires unique names per network |
