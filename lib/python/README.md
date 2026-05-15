# DMED Python Library

Lightweight Python library for DMED protocol — beacon encode/decode, capability cards, server & scanner.

## Install

```bash
pip install dmed-protocol            # core only (zero deps)
pip install dmed-protocol[server]    # + zeroconf, flask
pip install dmed-protocol[client]    # + zeroconf, requests
pip install dmed-protocol[all]       # everything
```

Or just copy `dmed/__init__.py` into your project.

## Usage — Server (5 lines)

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card(instance_id="a1b2c3d4", name="My Light", service_type="iot_device",
            transports=[Transport("http")],
            tools=[Tool("toggle", "Toggle light"), Tool("brightness", "Set 0-100")])

def handle(name, args):
    if name == "toggle": return "toggled"
    if name == "brightness": return f"set to {args.get('level', 50)}%"

DMEDServer(card, port=8080, tool_handler=handle).start()
```

## Usage — Scanner (3 lines)

```python
from dmed import DMEDScanner

servers = DMEDScanner().scan(timeout=5)
for iid, card in servers.items():
    print(f"{card.name}: {[t.name for t in card.tools]}")
```

## Usage — Beacon (no deps)

```python
from dmed import Beacon, ServiceType, Flag, crc16

# Encode
b = Beacon(flags=Flag.MULTI, service_type=ServiceType.IOT_DEVICE,
           instance_id=0xA1B2C3D4, tx_power=-20,
           name_hash=crc16(b"Kitchen Light"))
data = b.encode()  # 9 bytes

# Decode
b2 = Beacon.decode(data)
print(b2.instance_id_hex)  # "a1b2c3d4"
```

## Memory / Size

- Core module: ~6KB source, zero allocations for beacon ops
- No C extensions needed — pure Python
- Works on CPython, PyPy, MicroPython (beacon subset)
