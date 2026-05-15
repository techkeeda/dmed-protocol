# Tutorial: Build a DMED Client

Scan for DMED servers, discover capabilities, and connect via MCP.

---

## What a Client Does

```
1. SCAN    → Listen for beacons (BLE / mDNS / DNS)
2. RESOLVE → Fetch Capability Card from discovered server
3. DISPLAY → Show user what's available
4. CONNECT → Establish MCP session
5. USE     → Invoke tools, read resources
```

## Python Client (WiFi/mDNS)

### Minimal Scanner

```python
from dmed import DMEDScanner

# Scan for 5 seconds
servers = DMEDScanner().scan(timeout=5)

for iid, card in servers.items():
    print(f"📡 {card.name} [{card.service_type}]")
    print(f"   Tools: {[t.name for t in card.tools]}")
    print(f"   URL:   {card.transports[0].url}")
```

### Full Client with MCP Interaction

```python
import requests
from dmed import DMEDScanner

# 1. Discover
scanner = DMEDScanner()
servers = scanner.scan(timeout=5)

if not servers:
    print("No DMED servers found"); exit()

# 2. Pick a server
card = list(servers.values())[0]
mcp_url = card.transports[0].url
print(f"Connecting to: {card.name}")

# 3. Initialize MCP session
requests.post(mcp_url, json={
    "jsonrpc": "2.0", "id": 0, "method": "initialize",
    "params": {"protocolVersion": "2025-03-26", "capabilities": {},
               "clientInfo": {"name": "My Client", "version": "1.0"}}
})

# 4. List tools
resp = requests.post(mcp_url, json={
    "jsonrpc": "2.0", "id": 1, "method": "tools/list", "params": {}
}).json()
tools = resp["result"]["tools"]
print(f"Available tools: {[t['name'] for t in tools]}")

# 5. Call a tool
result = requests.post(mcp_url, json={
    "jsonrpc": "2.0", "id": 2, "method": "tools/call",
    "params": {"name": tools[0]["name"], "arguments": {}}
}).json()
print(f"Result: {result['result']['content'][0]['text']}")
```

## C Client (Embedded / Low-Level)

See `examples/thought_client.c` for a complete implementation. Key steps:

```c
#define DMED_IMPLEMENTATION
#include "dmed.h"

// 1. Receive beacon bytes (from BLE scan or mDNS)
dmed_beacon_t beacon;
dmed_beacon_decode(raw_bytes, len, &beacon);
printf("Found: %s (id=%08X)\n",
       dmed_service_type_str(beacon.service_type), beacon.instance_id);

// 2. Fetch card via HTTP (use your platform's HTTP client)
// GET http://<host>:<port>/dmed/card

// 3. Parse card JSON (use cJSON, jsmn, or similar)

// 4. POST to MCP endpoint with JSON-RPC
```

## Internet Client (DNS Discovery)

```python
import dns.resolver
import requests
from dmed import Card

# 1. Query DNS
domain = "api.example.com"
answers = dns.resolver.resolve(f"_dmed.{domain}", "TXT")
txt = answers[0].strings[0].decode()

# 2. Parse TXT fields
fields = dict(pair.split("=", 1) for pair in txt.split(";"))

# 3. Fetch card
card_url = fields.get("card", f"https://{domain}/.well-known/dmed/card")
card = Card.from_dict(requests.get(card_url).json())

# 4. Connect via MCP
print(f"Found: {card.name} at {card.transports[0].url}")
```

## Client State Machine

Implement this for robust behavior:

```
IDLE → start_scan() → SCANNING
SCANNING → beacon_found → RESOLVING (fetch card)
RESOLVING → card_ok → RESOLVED (show to user)
RESOLVING → timeout → SCANNING (retry)
RESOLVED → user_connect → CONNECTING
CONNECTING → mcp_ready → CONNECTED
CONNECTING → error → RESOLVED (try next transport)
CONNECTED → lost → SCANNING
```

## Deduplication

Same server may appear on multiple transports. Always deduplicate by `instance_id`:

```python
discovered = {}  # instance_id → card

def on_beacon(instance_id, transport_info):
    if instance_id in discovered:
        # Merge transport into existing entry
        discovered[instance_id].transports.append(transport_info)
    else:
        # New server — fetch card
        card = fetch_card(transport_info)
        discovered[instance_id] = card
```

## UI Best Practices

| Guideline | Reason |
|-----------|--------|
| Show servers within 2s of discovery | Users expect instant feedback |
| Display service_type as icon | Quick visual scanning |
| Show auth requirement before connect | No surprise login prompts |
| Remove servers after 30s silence | Don't show stale entries |
| Sort by proximity (RSSI/subnet) | Most relevant first |
| Allow filtering by service_type | Don't overwhelm in dense environments |

## Error Handling

```python
# Always handle these cases:
try:
    card = fetch_card(url, timeout=5)
except Timeout:
    mark_unavailable(instance_id)
except InvalidJSON:
    mark_incompatible(instance_id)

# Validate card
if card.instance_id != beacon_instance_id:
    discard(card)  # Possible impersonation

# Fallback transports
for transport in sorted(card.transports, key=lambda t: t.priority):
    try:
        session = connect_mcp(transport.url)
        break
    except ConnectionError:
        continue
```
