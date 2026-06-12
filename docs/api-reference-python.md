# API Reference — Python Library (`dmed`)

Install:
```bash
pip install dmed-protocol          # core (zero deps)
pip install dmed-protocol[all]     # + zeroconf, flask, requests
```

Or copy `lib/python/dmed/__init__.py` into your project.

---

## Core (Zero Dependencies)

### `Beacon`

```python
@dataclass
class Beacon:
    version: int = 1
    flags: int = 0
    service_type: int = 0
    instance_id: int = 0
    tx_power: Optional[int] = None
    name_hash: Optional[int] = None
```

#### `Beacon.encode() → bytes`

Encode to binary wire format (6-9 bytes).

```python
from dmed import Beacon, Flag, ServiceType, crc16

b = Beacon(flags=Flag.MULTI, service_type=ServiceType.IOT_DEVICE,
           instance_id=0xA1B2C3D4, tx_power=-20,
           name_hash=crc16(b"Kitchen Light"))
data = b.encode()  # b'\x14\x01\xa1\xb2\xc3\xd4\xec\x3f\xd8'
```

#### `Beacon.decode(data: bytes) → Beacon`

Decode from binary. Raises `ValueError` if too short.

```python
b = Beacon.decode(data)
print(b.instance_id_hex)  # "a1b2c3d4"
print(b.service_type)     # 1 (IOT_DEVICE)
```

#### `Beacon.instance_id_hex → str`

Property returning 8-char lowercase hex string.

#### `Beacon.to_mdns_txt(name="", path="/mcp", card="/dmed/card") → dict`

Convert to mDNS TXT record properties dict for use with `zeroconf`.

```python
props = b.to_mdns_txt("Kitchen Light")
# {'v': '1', 'id': 'a1b2c3d4', 'st': '01', 'fl': '4', 'nm': 'Kitchen Light', 'path': '/mcp', 'card': '/dmed/card'}
```

---

### `Card`

```python
@dataclass
class Card:
    instance_id: str          # 8-char hex
    name: str                 # Human-readable name
    service_type: str = "unknown"
    description: str = ""
    transports: List[Transport] = []
    auth_type: str = "none"
    tools: List[Tool] = []
    tags: List[str] = []
    metadata: Dict[str, Any] = {}
```

#### `Card.to_dict() → dict`

Convert to a valid DMED Capability Card dictionary.

#### `Card.to_json(indent=None) → str`

Serialize to JSON string.

```python
from dmed import Card, Tool, Transport

card = Card(
    instance_id="cafe0001",
    name="Coffee Kiosk",
    service_type="retail_kiosk",
    transports=[Transport("http", "http://10.0.0.5:9000/mcp", priority=1)],
    tools=[Tool("get_menu", "Get today's menu"), Tool("place_order", "Place an order")]
)
print(card.to_json(indent=2))
```

#### `Card.from_dict(d: dict) → Card`

Parse from dictionary (e.g., from JSON response).

#### `Card.from_json(s: str) → Card`

Parse from JSON string.

```python
card = Card.from_json(response.text)
print(card.name, [t.name for t in card.tools])
```

---

### `Tool`

```python
@dataclass
class Tool:
    name: str                          # Tool identifier (lowercase, underscores)
    description: str = ""              # What it does
    input_schema: Optional[Dict] = None  # JSON Schema for inputSchema (optional)
```

### `Transport`

```python
@dataclass
class Transport:
    type: str          # "http", "https", "ws", "wss", "ble_gatt"
    url: str = ""      # Full URL to MCP endpoint
    priority: int = 10 # Lower = preferred
```

---

### `Flag` (IntFlag)

```python
class Flag(IntFlag):
    AUTH  = 1   # Requires authentication
    TLS   = 2   # Encrypted session
    MULTI = 4   # Multiple tools
    CLOUD = 8   # Internet-backed
```

Combine: `Flag.MULTI | Flag.TLS`

### `ServiceType` (IntEnum)

```python
class ServiceType(IntEnum):
    UNKNOWN = 0x00
    IOT_DEVICE = 0x01
    MEDIA = 0x02
    # ... (see source for full list)
    CUSTOM = 0xFF
```

---

### Utility Functions

#### `crc16(data: bytes) → int`

CRC-16/CCITT. Use for `name_hash`.

```python
from dmed import crc16
hash_val = crc16(b"My Device Name")
```

#### `crc32(data: bytes) → int`

CRC-32. Used internally by `instance_id_from`.

#### `instance_id_from(s: str) → str`

Generate stable 8-char hex instance ID from any string.

```python
from dmed import instance_id_from
iid = instance_id_from("my-device-serial-123")  # "d412f909"
```

---

## High-Level API (Requires Optional Dependencies)

### `DMEDServer`

Full server: mDNS advertisement + HTTP capability card + MCP endpoint.

**Requires:** `pip install zeroconf flask`

```python
class DMEDServer:
    def __init__(self, card: Card, port: int = 8080, tool_handler=None):
        ...
    def start(self):  # Blocking — runs Flask server
        ...
```

#### Parameters

| Param | Type | Description |
|-------|------|-------------|
| `card` | `Card` | Your server's capability card |
| `port` | `int` | HTTP port (default 8080) |
| `tool_handler` | `Callable[[str, dict], Any]` | Function called with `(tool_name, arguments)` → return value |

#### Example

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("aabbccdd", "Room Sensor", "environmental",
            transports=[Transport("http")],
            tools=[Tool("get_temp", "Get temperature"),
                   Tool("get_humidity", "Get humidity")])

def handle(name, args):
    if name == "get_temp": return "22.5°C"
    if name == "get_humidity": return "45%"

DMEDServer(card, port=9000, tool_handler=handle).start()
```

The server automatically:
- Registers `_dmed._tcp` on the local network via mDNS
- Serves the capability card at `GET /dmed/card`
- Handles MCP protocol at `POST /mcp` (initialize, tools/list, tools/call)

---

### `DMEDScanner`

Scan local network for DMED servers.

**Requires:** `pip install zeroconf requests`

```python
class DMEDScanner:
    def __init__(self): ...
    def scan(self, timeout: float = 5.0) -> Dict[str, Card]: ...
```

#### Example

```python
from dmed import DMEDScanner

scanner = DMEDScanner()
servers = scanner.scan(timeout=5)

for instance_id, card in servers.items():
    print(f"Found: {card.name}")
    print(f"  Type: {card.service_type}")
    print(f"  Tools: {[t.name for t in card.tools]}")
    print(f"  URL: {card.transports[0].url}")
```

---

---

## v0.2: Interaction API

### `DMEDClient`

Connect to and interact with a discovered DMED endpoint.

**Requires:** `pip install requests`

```python
class DMEDClient:
    def __init__(self, base_url: str, auth_token: str = None): ...
    def connect(self) -> Card: ...
    def list_actions(self) -> List[Dict]: ...
    def send_action(self, action: str, params: Dict = None) -> Dict: ...
    def call_tool(self, tool_name: str, arguments: Dict = None) -> Dict: ...
```

#### `DMEDClient.connect() → Card`

Fetch the endpoint's Capability Card.

```python
from dmed import DMEDClient

client = DMEDClient("http://192.168.1.42:8080")
card = client.connect()
print(f"Connected to: {card.name}")
print(f"Tools: {[t.name for t in card.tools]}")
```

#### `DMEDClient.list_actions() → List[Dict]`

List available actions with their parameter schemas.

```python
actions = client.list_actions()
for a in actions:
    print(f"  {a['name']} — {a['description']}")
```

#### `DMEDClient.send_action(action, params) → Dict`

Send a lightweight action/command. This is the primary interaction method.

```python
result = client.send_action("brew_coffee", {"drink_type": "latte", "size": "large"})
# {"status": "ok", "action": "brew_coffee", "result": {"text": "☕ Your large latte is ready."}}
```

#### `DMEDClient.call_tool(tool_name, arguments) → Dict`

Full MCP JSON-RPC `tools/call`. Use `send_action()` for lightweight interaction.

```python
result = client.call_tool("brew_coffee", {"drink_type": "latte", "size": "large"})
# {"content": [{"type": "text", "text": "☕ Your large latte is ready."}]}
```

---

### `DMEDScanner.connect(instance_id) → DMEDClient`

After scanning, connect directly to a discovered endpoint:

```python
from dmed import DMEDScanner

scanner = DMEDScanner()
endpoints = scanner.scan(timeout=5)

# Connect to first found endpoint
first_id = list(endpoints.keys())[0]
client = scanner.connect(first_id)
card = client.connect()
result = client.send_action("get_status")
```

---

### Server: Action Endpoints (auto-registered)

`DMEDServer` in v0.2 automatically exposes:

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/dmed/card` | GET | Capability Card |
| `/dmed/actions` | GET | List actions with param schemas |
| `/dmed/action` | POST | Send lightweight action |
| `/mcp` | POST | Full MCP JSON-RPC |

No extra code needed — if you have a `DMEDServer` with a `tool_handler`, all endpoints work automatically.

---

## Memory & Performance

- **Core module:** ~8KB source, pure Python
- **Beacon encode/decode:** ~1μs per operation
- **No C extensions** — works on CPython, PyPy, MicroPython (beacon subset)
- **Server overhead:** Flask + zeroconf (~20MB RAM total)
