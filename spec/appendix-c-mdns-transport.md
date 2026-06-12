# Appendix C: mDNS/DNS-SD Transport Detail (WiFi & Ethernet)

## C.1 Service Type

DMED servers on local networks MUST register the following DNS-SD service type:

```
_dmed._tcp
```

This service type indicates an MCP server discoverable via the DMED protocol.

## C.2 DNS-SD Registration

### SRV Record

```
<instance_name>._dmed._tcp.local. 120 IN SRV 0 0 <port> <hostname>.local.
```

- `<instance_name>`: Human-readable name (RFC 6763 compliant, max 63 bytes UTF-8)
- `<port>`: TCP port where the HTTP server listens
- `<hostname>`: mDNS hostname of the device

### TXT Record

```
<instance_name>._dmed._tcp.local. 4500 IN TXT
  "v=1"
  "id=a1b2c3d4"
  "st=01"
  "fl=0"
  "nm=Kitchen Light"
  "path=/mcp"
  "card=/dmed/card"
```

### TXT Record Key Definitions

| Key | Required | Max Length | Description |
|-----|----------|-----------|-------------|
| `v` | REQUIRED | 2 chars | DMED protocol version (integer) |
| `id` | REQUIRED | 8 chars | Instance ID (lowercase hex) |
| `st` | REQUIRED | 2 chars | Service type code (lowercase hex) |
| `fl` | REQUIRED | 1 char | Flags nibble (lowercase hex) |
| `nm` | RECOMMENDED | 63 bytes | Human-readable name (UTF-8) |
| `path` | REQUIRED | 128 chars | URL path to MCP endpoint |
| `card` | REQUIRED | 128 chars | URL path to Capability Card |
| `tp` | OPTIONAL | 4 chars | TX power (signed decimal, for WiFi RSSI calibration) |

## C.3 Browsing Procedure (Client)

Clients discover DMED servers by browsing for the service type:

```
1. Send mDNS query: _dmed._tcp.local. PTR ?
2. Receive PTR responses listing instance names
3. For each instance, resolve SRV + TXT records
4. Extract host, port, and TXT key-value pairs
5. Construct Capability Card URL: http://<host>:<port><card_path>
6. Fetch Capability Card via HTTP GET
```

### Pseudocode

```python
import zeroconf

class DMEDListener:
    def add_service(self, zc, service_type, name):
        info = zc.get_service_info(service_type, name)
        host = socket.inet_ntoa(info.addresses[0])
        port = info.port
        txt = info.properties  # dict of TXT key-value pairs

        instance_id = txt[b'id'].decode()
        card_path = txt[b'card'].decode()
        mcp_path = txt[b'path'].decode()

        # Fetch capability card
        card_url = f"http://{host}:{port}{card_path}"
        card = http_get_json(card_url)

        # Store discovered server
        discovered_servers[instance_id] = {
            'card': card,
            'mcp_url': f"http://{host}:{port}{mcp_path}",
            'transport': 'mdns'
        }

zc = zeroconf.Zeroconf()
browser = zeroconf.ServiceBrowser(zc, "_dmed._tcp.local.", DMEDListener())
```

## C.4 Registration Procedure (Server)

Servers advertise by registering the service:

```python
import zeroconf

info = zeroconf.ServiceInfo(
    type_="_dmed._tcp.local.",
    name="Kitchen Light._dmed._tcp.local.",
    addresses=[socket.inet_aton("192.168.1.42")],
    port=8080,
    properties={
        'v': '1',
        'id': 'a1b2c3d4',
        'st': '01',
        'fl': '0',
        'nm': 'Kitchen Light',
        'path': '/mcp',
        'card': '/dmed/card',
    },
    server="kitchen-light.local.",
)

zc = zeroconf.Zeroconf()
zc.register_service(info)
```

## C.5 HTTP Endpoints (Server)

The server MUST serve these HTTP endpoints on the advertised port:

### GET `<card_path>` (e.g., `/dmed/card`)

**Response:**
```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Cache-Control: max-age=300
ETag: "abc123"

{
  "dmed_version": "0.2.0",
  "instance_id": "a1b2c3d4",
  "name": "Kitchen Light",
  ...
}
```

**Conditional request support:**
```http
GET /dmed/card HTTP/1.1
If-None-Match: "abc123"

HTTP/1.1 304 Not Modified
```

### POST `<mcp_path>` (e.g., `/mcp`)

Standard MCP Streamable HTTP endpoint per MCP specification.

## C.6 IPv4 and IPv6

- Servers MUST support IPv4.
- Servers SHOULD support IPv6 link-local addresses.
- Clients MUST handle both A and AAAA records in mDNS responses.
- When both are available, clients SHOULD prefer IPv6 link-local for same-subnet communication.

## C.7 Goodbye Packets

When a server shuts down gracefully, it MUST send mDNS goodbye packets (records with TTL=0) to notify clients:

```
Kitchen Light._dmed._tcp.local. 0 IN SRV ...
Kitchen Light._dmed._tcp.local. 0 IN TXT ...
```

This causes clients to immediately remove the server from their discovered list.

## C.8 Conflict Resolution

If two servers on the same network have the same instance name, standard mDNS conflict resolution (RFC 6762 Section 9) applies. The server that loses the conflict MUST choose a new instance name (typically by appending a number).

The `instance_id` (in TXT record) is NOT affected by name conflicts — it remains stable.

## C.9 Network Considerations

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| mDNS multicast address (IPv4) | 224.0.0.251 | RFC 6762 |
| mDNS multicast address (IPv6) | ff02::fb | RFC 6762 |
| mDNS port | 5353 | RFC 6762 |
| TXT record TTL | 4500 seconds | RFC 6763 recommendation |
| SRV record TTL | 120 seconds | Short for dynamic environments |
| Reconfirmation interval | 80% of TTL | RFC 6762 cache maintenance |

## C.10 Firewall Requirements

For DMED mDNS discovery to work, the following MUST be permitted:

- **UDP port 5353** (mDNS) — multicast send and receive
- **TCP port `<advertised_port>`** — HTTP for card retrieval and MCP session
- Multicast group membership for 224.0.0.251 / ff02::fb
