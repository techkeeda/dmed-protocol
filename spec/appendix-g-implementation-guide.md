# Appendix G: Reference Implementation Guide

This appendix provides guidance for building DMED-conformant implementations. It includes recommended libraries, minimal code structure, and a quickstart for both servers and clients.

---

## G.1 Minimal DMED Server (Python — WiFi/mDNS)

This is the simplest possible DMED server. It advertises via mDNS and serves a Capability Card and MCP endpoint over HTTP.

### Dependencies

```
pip install zeroconf flask
```

### Code: `dmed_server.py`

```python
#!/usr/bin/env python3
"""Minimal DMED Server — mDNS + HTTP"""

import json
import socket
import threading
from flask import Flask, jsonify, request
from zeroconf import Zeroconf, ServiceInfo

# === Configuration ===
SERVER_NAME = "My DMED Device"
INSTANCE_ID = "a1b2c3d4"
SERVICE_TYPE = "0a"  # tool_utility
FLAGS = "4"  # MULTI flag set
PORT = 8080
MCP_PATH = "/mcp"
CARD_PATH = "/dmed/card"

# === Capability Card ===
CAPABILITY_CARD = {
    "dmed_version": "0.2.0",
    "instance_id": INSTANCE_ID,
    "name": SERVER_NAME,
    "description": "A simple DMED-enabled tool server",
    "service_type": "tool_utility",
    "transports": [
        {
            "type": "http",
            "url": f"http://{{host}}:{PORT}{MCP_PATH}",
            "priority": 1
        }
    ],
    "auth": {"type": "none"},
    "capabilities": {
        "tools": [
            {
                "name": "echo",
                "description": "Echo back the input text",
                "input_schema_summary": "text: string"
            },
            {
                "name": "add",
                "description": "Add two numbers",
                "input_schema_summary": "a: number, b: number"
            }
        ],
        "resources": [],
        "prompts": []
    }
}

# === MCP Tool Implementations ===
def handle_tool_call(name, arguments):
    if name == "echo":
        return {"type": "text", "text": arguments.get("text", "")}
    elif name == "add":
        result = arguments.get("a", 0) + arguments.get("b", 0)
        return {"type": "text", "text": str(result)}
    else:
        raise ValueError(f"Unknown tool: {name}")

# === Flask App ===
app = Flask(__name__)

@app.route(CARD_PATH, methods=["GET"])
def serve_card():
    # Fill in actual host IP
    host = request.host.split(":")[0]
    card = json.loads(json.dumps(CAPABILITY_CARD))
    card["transports"][0]["url"] = f"http://{host}:{PORT}{MCP_PATH}"
    return jsonify(card)

@app.route(MCP_PATH, methods=["POST"])
def mcp_endpoint():
    """Minimal MCP Streamable HTTP handler"""
    req = request.get_json()
    method = req.get("method")
    req_id = req.get("id")

    if method == "initialize":
        return jsonify({
            "jsonrpc": "2.0",
            "id": req_id,
            "result": {
                "protocolVersion": "2025-03-26",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": SERVER_NAME, "version": "1.0.0"}
            }
        })
    elif method == "notifications/initialized":
        return "", 204
    elif method == "tools/list":
        tools = [
            {
                "name": "echo",
                "description": "Echo back the input text",
                "inputSchema": {
                    "type": "object",
                    "properties": {"text": {"type": "string"}},
                    "required": ["text"]
                }
            },
            {
                "name": "add",
                "description": "Add two numbers",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "a": {"type": "number"},
                        "b": {"type": "number"}
                    },
                    "required": ["a", "b"]
                }
            }
        ]
        return jsonify({"jsonrpc": "2.0", "id": req_id, "result": {"tools": tools}})
    elif method == "tools/call":
        name = req["params"]["name"]
        args = req["params"].get("arguments", {})
        try:
            content = handle_tool_call(name, args)
            return jsonify({
                "jsonrpc": "2.0", "id": req_id,
                "result": {"content": [content]}
            })
        except ValueError as e:
            return jsonify({
                "jsonrpc": "2.0", "id": req_id,
                "error": {"code": -32601, "message": str(e)}
            })
    else:
        return jsonify({
            "jsonrpc": "2.0", "id": req_id,
            "error": {"code": -32601, "message": f"Method not found: {method}"}
        })

# === mDNS Registration ===
def get_local_ip():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    finally:
        s.close()

def register_mdns():
    ip = get_local_ip()
    info = ServiceInfo(
        type_="_mcp-dmed._tcp.local.",
        name=f"{SERVER_NAME}._mcp-dmed._tcp.local.",
        addresses=[socket.inet_aton(ip)],
        port=PORT,
        properties={
            "v": "1",
            "id": INSTANCE_ID,
            "st": SERVICE_TYPE,
            "fl": FLAGS,
            "nm": SERVER_NAME,
            "path": MCP_PATH,
            "card": CARD_PATH,
        },
        server=f"{INSTANCE_ID}.local.",
    )
    zc = Zeroconf()
    zc.register_service(info)
    print(f"[DMED] Registered mDNS: {SERVER_NAME} at {ip}:{PORT}")
    return zc

# === Main ===
if __name__ == "__main__":
    zc = register_mdns()
    try:
        print(f"[DMED] Server running on port {PORT}")
        print(f"[DMED] Card: http://localhost:{PORT}{CARD_PATH}")
        print(f"[DMED] MCP:  http://localhost:{PORT}{MCP_PATH}")
        app.run(host="0.0.0.0", port=PORT)
    finally:
        zc.unregister_all_services()
        zc.close()
```

### Run

```bash
python3 dmed_server.py
```

### Test

```bash
# Fetch capability card
curl http://localhost:8080/dmed/card | jq .

# Call a tool via MCP
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"add","arguments":{"a":5,"b":3}}}'
```

---

## G.2 Minimal DMED Client (Python — mDNS Scanner)

### Dependencies

```
pip install zeroconf requests
```

### Code: `dmed_client.py`

```python
#!/usr/bin/env python3
"""Minimal DMED Client — mDNS Scanner"""

import json
import socket
import time
import requests
from zeroconf import Zeroconf, ServiceBrowser, ServiceListener

class DMEDScanner(ServiceListener):
    def __init__(self):
        self.servers = {}  # instance_id -> server info

    def add_service(self, zc, service_type, name):
        info = zc.get_service_info(service_type, name)
        if not info:
            return

        host = socket.inet_ntoa(info.addresses[0])
        port = info.port
        props = {k.decode(): v.decode() for k, v in info.properties.items()}

        instance_id = props.get("id", "unknown")
        card_path = props.get("card", "/dmed/card")
        server_name = props.get("nm", name)

        # Fetch capability card
        card_url = f"http://{host}:{port}{card_path}"
        try:
            resp = requests.get(card_url, timeout=5)
            card = resp.json()
        except Exception as e:
            print(f"  [!] Failed to fetch card: {e}")
            return

        self.servers[instance_id] = {
            "name": server_name,
            "host": host,
            "port": port,
            "card": card,
            "mcp_url": card["transports"][0]["url"]
        }

        print(f"\n[+] Discovered: {server_name}")
        print(f"    ID: {instance_id}")
        print(f"    Type: {card.get('service_type', 'unknown')}")
        print(f"    Tools: {[t['name'] for t in card.get('capabilities', {}).get('tools', [])]}")
        print(f"    MCP URL: {card['transports'][0]['url']}")

    def remove_service(self, zc, service_type, name):
        print(f"\n[-] Lost: {name}")

    def update_service(self, zc, service_type, name):
        pass

def call_tool(mcp_url, tool_name, arguments):
    """Call an MCP tool on a discovered server."""
    # Initialize
    requests.post(mcp_url, json={
        "jsonrpc": "2.0", "id": 0, "method": "initialize",
        "params": {
            "protocolVersion": "2025-03-26",
            "capabilities": {},
            "clientInfo": {"name": "DMED CLI Client", "version": "0.2.0"}
        }
    })

    # Call tool
    resp = requests.post(mcp_url, json={
        "jsonrpc": "2.0", "id": 1, "method": "tools/call",
        "params": {"name": tool_name, "arguments": arguments}
    })
    return resp.json()

if __name__ == "__main__":
    print("[DMED Client] Scanning for DMED servers on local network...")
    print("[DMED Client] Press Ctrl+C to stop\n")

    scanner = DMEDScanner()
    zc = Zeroconf()
    browser = ServiceBrowser(zc, "_mcp-dmed._tcp.local.", scanner)

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        zc.close()

    # Interactive tool calling
    if scanner.servers:
        print("\n\n=== Discovered Servers ===")
        servers = list(scanner.servers.values())
        for i, s in enumerate(servers):
            print(f"  [{i}] {s['name']} — {s['mcp_url']}")

        choice = int(input("\nSelect server [0]: ") or "0")
        server = servers[choice]

        tools = server["card"]["capabilities"]["tools"]
        print(f"\nTools on {server['name']}:")
        for i, t in enumerate(tools):
            print(f"  [{i}] {t['name']} — {t.get('description', '')}")

        tool_idx = int(input("\nSelect tool [0]: ") or "0")
        tool = tools[tool_idx]

        args_str = input(f"Arguments for '{tool['name']}' (JSON): ")
        args = json.loads(args_str) if args_str else {}

        result = call_tool(server["mcp_url"], tool["name"], args)
        print(f"\nResult: {json.dumps(result, indent=2)}")
```

### Run

```bash
# Terminal 1: Start server
python3 dmed_server.py

# Terminal 2: Start client
python3 dmed_client.py
```

---

## G.3 Minimal DMED Server (ESP32 — BLE + WiFi, Arduino/C++)

### Sketch outline for ESP32 (PlatformIO/Arduino):

```cpp
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// DMED UUIDs
#define DMED_BEACON_SERVICE_UUID "d14d0000-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_SERVICE_UUID   "d14d0001-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_DATA_UUID      "d14d0002-1000-4000-8000-00805f9b34fb"
#define DMED_CARD_LENGTH_UUID    "d14d0003-1000-4000-8000-00805f9b34fb"

// Configuration
#define INSTANCE_ID 0xA1B2C3D4
#define SERVICE_TYPE 0x01  // iot_device
#define FLAGS 0x04         // MULTI
#define SERVER_NAME "Kitchen Light"
#define HTTP_PORT 8080

WebServer httpServer(HTTP_PORT);
String capabilityCardJson;

void setupBLE() {
    BLEDevice::init(SERVER_NAME);

    // Advertising
    BLEAdvertising *adv = BLEDevice::getAdvertising();

    // Build beacon payload
    uint8_t beacon[7] = {
        (1 << 4) | FLAGS,   // version=1, flags
        SERVICE_TYPE,        // service_type
        (INSTANCE_ID >> 24) & 0xFF,
        (INSTANCE_ID >> 16) & 0xFF,
        (INSTANCE_ID >> 8) & 0xFF,
        INSTANCE_ID & 0xFF,
        0xEC                 // tx_power = -20 dBm
    };

    // Set as service data
    BLEAdvertisementData advData;
    advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
    advData.setCompleteServices(BLEUUID(DMED_BEACON_SERVICE_UUID));
    // Add beacon as manufacturer data (simplified)
    std::string beaconStr((char*)beacon, sizeof(beacon));
    advData.setManufacturerData(beaconStr);
    adv->setAdvertisementData(advData);

    // Scan response with name
    BLEAdvertisementData scanResp;
    scanResp.setName(SERVER_NAME);
    adv->setScanResponseData(scanResp);

    // GATT service for capability card
    BLEServer *server = BLEDevice::createServer();
    BLEService *cardService = server->createService(DMED_CARD_SERVICE_UUID);

    // Card length characteristic
    BLECharacteristic *lengthChar = cardService->createCharacteristic(
        DMED_CARD_LENGTH_UUID, BLECharacteristic::PROPERTY_READ);
    uint32_t cardLen = capabilityCardJson.length();
    lengthChar->setValue((uint8_t*)&cardLen, 4);

    // Card data characteristic
    BLECharacteristic *dataChar = cardService->createCharacteristic(
        DMED_CARD_DATA_UUID, BLECharacteristic::PROPERTY_READ);
    dataChar->setValue(capabilityCardJson.c_str());

    cardService->start();
    adv->start();
}

void setupMDNS() {
    MDNS.begin("kitchen-light");
    MDNS.addService("_mcp-dmed", "_tcp", HTTP_PORT);
    MDNS.addServiceTxt("_mcp-dmed", "_tcp", "v", "1");
    MDNS.addServiceTxt("_mcp-dmed", "_tcp", "id", "a1b2c3d4");
    MDNS.addServiceTxt("_mcp-dmed", "_tcp", "st", "01");
    MDNS.addServiceTxt("_mcp-dmed", "_tcp", "fl", "4");
    MDNS.addServiceTxt("_mcp-dmed", "_tcp", "nm", SERVER_NAME);
    MDNS.addServiceTxt("_mcp-dmed", "_tcp", "path", "/mcp");
    MDNS.addServiceTxt("_mcp-dmed", "_tcp", "card", "/dmed/card");
}

void setupHTTP() {
    httpServer.on("/dmed/card", HTTP_GET, []() {
        httpServer.send(200, "application/json", capabilityCardJson);
    });

    httpServer.on("/mcp", HTTP_POST, []() {
        // Parse MCP JSON-RPC request and handle tools
        // (implementation similar to Python example)
        String body = httpServer.arg("plain");
        // ... handle MCP methods ...
    });

    httpServer.begin();
}

void setup() {
    // Build capability card JSON
    // ... (use ArduinoJson to construct the card) ...

    setupBLE();
    setupMDNS();
    setupHTTP();
}

void loop() {
    httpServer.handleClient();
}
```

---

## G.4 Recommended Libraries by Platform

### Server Libraries

| Platform | BLE | mDNS | HTTP | JSON |
|----------|-----|------|------|------|
| Python | `bleak` | `zeroconf` | `flask` / `fastapi` | `json` (stdlib) |
| Node.js | `@abandonware/noble` | `bonjour-service` | `express` | `JSON` (native) |
| ESP32 (C++) | ESP-IDF BLE | `ESPmDNS` | `WebServer` | `ArduinoJson` |
| Rust | `btleplug` | `mdns-sd` | `axum` / `actix-web` | `serde_json` |
| Go | `tinygo bluetooth` | `github.com/hashicorp/mdns` | `net/http` | `encoding/json` |
| Swift (iOS) | `CoreBluetooth` | `NWBrowser` (Network.framework) | `URLSession` | `JSONDecoder` |
| Kotlin (Android) | `android.bluetooth.le` | `android.net.nsd` | `OkHttp` | `kotlinx.serialization` |

### Client Libraries

| Platform | BLE Scan | mDNS Browse | HTTP | MCP Client |
|----------|----------|-------------|------|-----------|
| Swift (iOS) | `CoreBluetooth` | `NWBrowser` | `URLSession` | Custom / `mcp-swift-sdk` |
| Kotlin (Android) | `android.bluetooth.le` | `NsdManager` | `OkHttp` | Custom / `mcp-kotlin-sdk` |
| Python | `bleak` | `zeroconf` | `requests` / `httpx` | `mcp` (official SDK) |
| TypeScript | `noble` | `bonjour-service` | `fetch` | `@modelcontextprotocol/sdk` |

---

## G.5 Conformance Checklist

### Server Conformance (Minimal — Level 1)

- [ ] Broadcasts beacon on at least one transport (BLE or mDNS)
- [ ] Beacon contains: version=1, valid flags, service_type, instance_id
- [ ] Serves Capability Card as valid JSON at advertised endpoint
- [ ] Card contains all REQUIRED fields (dmed_version, instance_id, name, service_type, transports, capabilities)
- [ ] Card instance_id matches beacon instance_id
- [ ] MCP endpoint responds to `initialize` method
- [ ] MCP endpoint responds to `tools/list` method
- [ ] MCP endpoint responds to `tools/call` for all advertised tools
- [ ] Returns appropriate HTTP status codes (200, 304, 429, 503)

### Client Conformance (Minimal — Level 3)

- [ ] Scans at least one transport (BLE or mDNS)
- [ ] Correctly parses beacon fields
- [ ] Fetches and parses Capability Card
- [ ] Validates card instance_id matches beacon
- [ ] Displays server name and service_type to user
- [ ] Establishes MCP session using transport from card
- [ ] Handles `initialize` → `tools/list` → `tools/call` flow
- [ ] Handles resolution errors gracefully (timeout, invalid JSON, etc.)
- [ ] Removes servers that stop advertising (within 30s for BLE, 75s for mDNS)

---

## G.6 Testing Your Implementation

### Manual Testing

```bash
# 1. Verify mDNS advertisement is visible
dns-sd -B _mcp-dmed._tcp local.
# or on Linux:
avahi-browse -r _mcp-dmed._tcp

# 2. Verify capability card
curl http://<server-ip>:<port>/dmed/card | python3 -m json.tool

# 3. Verify MCP initialize
curl -X POST http://<server-ip>:<port>/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}'

# 4. Verify tools/list
curl -X POST http://<server-ip>:<port>/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}'

# 5. Verify tool call
curl -X POST http://<server-ip>:<port>/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"echo","arguments":{"text":"hello"}}}'
```

### Automated Testing

A conformance test suite should verify:
1. Beacon encoding matches test vectors (Appendix F.5)
2. Card validates against JSON Schema (Appendix A)
3. MCP session follows protocol correctly
4. Error cases are handled (invalid JSON, unknown methods, auth failures)
