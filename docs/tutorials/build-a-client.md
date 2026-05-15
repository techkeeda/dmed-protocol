# Tutorial: Build a DMED Client

Scan for DMED servers, discover capabilities, and connect via MCP.

---

## What a Client Does

```
1. SCAN    → Listen for beacons (BLE / mDNS / DNS)
2. RESOLVE → Fetch Capability Card from discovered endpoint
3. DISPLAY → Show user what's available
4. INTERACT → Send lightweight actions (v0.2) or establish full MCP session
```

## Python Client (WiFi/mDNS)

### Minimal Scanner + Interaction (v0.2)

```python
from dmed import DMEDScanner, DMEDClient

# 1. Discover
scanner = DMEDScanner()
endpoints = scanner.scan(timeout=5)

for iid, card in endpoints.items():
    print(f"📡 {card.name} [{card.service_type}]")
    print(f"   Tools: {[t.name for t in card.tools]}")

# 2. Connect to first endpoint
first_id = list(endpoints.keys())[0]
client = scanner.connect(first_id)
card = client.connect()

# 3. List actions
actions = client.list_actions()
print(f"Available: {[a['name'] for a in actions]}")

# 4. Send action (lightweight — no MCP session needed)
result = client.send_action(actions[0]["name"], {})
print(f"Result: {result}")
```

### Full MCP Client (Advanced)

```python
from dmed import DMEDScanner, DMEDClient

scanner = DMEDScanner()
endpoints = scanner.scan(timeout=5)

if not endpoints:
    print("No DMED endpoints found"); exit()

# Connect
card = list(endpoints.values())[0]
client = DMEDClient(card.transports[0].url.rsplit("/",1)[0])
client.connect()

# Use full MCP tools/call
result = client.call_tool("add", {"a": 5, "b": 3})
print(f"Result: {result}")
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
