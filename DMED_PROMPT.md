# DMED (Dynamic MCP Endpoint Discovery) — AI Prompt Library

## Instructions for AI Tools

You are helping a user make their service, device, or application discoverable as an **MCP endpoint** via the **DMED (Dynamic MCP Endpoint Discovery) Protocol**. DMED allows MCP endpoints to broadcast their capabilities over local network transports (WiFi/mDNS, Bluetooth, Ethernet) so that any DMED-compatible client app can automatically find and interact with them via AI.

---

## Your Task

When a user describes what they want to make discoverable, you must:

1. **Create an MCP endpoint** that exposes the described functionality as tools
2. **Add DMED discovery broadcasting** so clients can find it automatically
3. **Generate a DMED manifest** describing the endpoint's capabilities

---

## DMED Protocol Specification

### DMED Manifest (dmed-manifest.json)

Every DMED-enabled MCP endpoint must have a manifest:

```json
{
  "dmed_version": "1.0",
  "endpoint": {
    "id": "<unique-endpoint-id>",
    "name": "<human-readable-name>",
    "description": "<what-this-endpoint-does>",
    "icon": "<optional-icon-url-or-emoji>",
    "category": "<category: iot | api | database | media | utility | commerce | custom>"
  },
  "capabilities": {
    "tools": [
      {
        "name": "<tool-name>",
        "description": "<what-it-does>",
        "inputSchema": { }
      }
    ],
    "resources": [],
    "prompts": []
  },
  "transport": {
    "supported": ["mdns", "bluetooth", "manual"],
    "mdns": {
      "service_type": "_dmed._tcp",
      "port": 3000
    },
    "bluetooth": {
      "service_uuid": "<generated-uuid>"
    }
  },
  "auth": {
    "type": "none | token | oauth2",
    "details": {}
  }
}
```

### Discovery Broadcasting

#### mDNS/DNS-SD (WiFi/Ethernet)

Broadcast using service type `_dmed._tcp` with TXT records:

| TXT Record     | Value                           |
|----------------|---------------------------------|
| `dmed_version` | `1.0`                           |
| `name`         | Endpoint display name           |
| `description`  | Short description               |
| `category`     | Endpoint category               |
| `manifest_url` | URL to full dmed-manifest.json  |

#### Bluetooth (BLE)

Advertise a BLE service with:
- Service UUID: `0000ED00-0000-1000-8000-00805F9B34FB` (base, replace ED00 with unique suffix)
- Characteristic: manifest JSON (chunked if needed)

---

## Code Generation Template (Node.js)

When generating code, use this structure:

```
project/
├── dmed-manifest.json
├── server.js          # MCP endpoint + DMED broadcasting
├── package.json
└── README.md
```

### package.json dependencies:

```json
{
  "dependencies": {
    "@modelcontextprotocol/sdk": "^1.0.0",
    "bonjour-service": "^1.2.0"
  }
}
```

### server.js template:

```javascript
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import Bonjour from "bonjour-service";
import { readFileSync } from "fs";
import { networkInterfaces } from "os";

// Load manifest
const manifest = JSON.parse(readFileSync("./dmed-manifest.json", "utf-8"));

// --- MCP Endpoint Setup ---
const server = new McpServer({
  name: manifest.endpoint.name,
  version: "1.0.0",
});

// TOOLS GO HERE — generate based on user's requirements
// server.tool("tool_name", "description", { ...schema }, async (params) => { ... });

// --- DMED Discovery Broadcasting ---
function getLocalIP() {
  const interfaces = networkInterfaces();
  for (const iface of Object.values(interfaces)) {
    for (const addr of iface) {
      if (addr.family === "IPv4" && !addr.internal) return addr.address;
    }
  }
  return "127.0.0.1";
}

const bonjour = new Bonjour();
const localIP = getLocalIP();

bonjour.publish({
  name: manifest.endpoint.name,
  type: "dmed",
  port: manifest.transport.mdns.port,
  txt: {
    dmed_version: manifest.dmed_version,
    name: manifest.endpoint.name,
    description: manifest.endpoint.description,
    category: manifest.endpoint.category,
    manifest_url: `http://${localIP}:${manifest.transport.mdns.port}/dmed-manifest.json`,
  },
});

console.error(`[DMED] Broadcasting MCP endpoint: ${manifest.endpoint.name}`);

// --- Start MCP transport ---
const transport = new StdioServerTransport();
await server.connect(transport);
```

---

## How to Use This Prompt

Tell the AI:

> "I want to make [THING] discoverable as an MCP endpoint via DMED. It should expose [CAPABILITIES]. It runs on [PLATFORM]."

### Examples:

- "I want to make my smart coffee machine discoverable as an MCP endpoint. It should expose: brew coffee, check water level, set temperature. It runs on a Raspberry Pi."
- "I want to make my local PostgreSQL database discoverable as an MCP endpoint. It should expose: query tables, list schemas, describe columns."
- "I want to make my home security camera discoverable as an MCP endpoint. It should expose: get live snapshot, list recordings, get motion events."

---

## Rules for the AI

1. Always generate a complete `dmed-manifest.json` first
2. Generate a working MCP endpoint with all tools implemented
3. Include mDNS broadcasting by default
4. Add Bluetooth BLE advertising if the user mentions mobile/Bluetooth
5. Use the user's language/platform preference (Node.js default, Python if requested)
6. Include a README with setup instructions
7. Keep the implementation minimal and functional
8. Use secure defaults (token auth if the endpoint handles sensitive data)

---

## Python Alternative

If the user prefers Python:

```python
from mcp.server import Server
from zeroconf import ServiceInfo, Zeroconf
import socket, json

manifest = json.load(open("dmed-manifest.json"))

# MCP Endpoint
server = Server(manifest["endpoint"]["name"])

# DMED Broadcasting
zc = Zeroconf()
info = ServiceInfo(
    "_dmed._tcp.local.",
    f"{manifest['endpoint']['name']}._dmed._tcp.local.",
    addresses=[socket.inet_aton(socket.gethostbyname(socket.gethostname()))],
    port=manifest["transport"]["mdns"]["port"],
    properties={
        "dmed_version": manifest["dmed_version"],
        "name": manifest["endpoint"]["name"],
        "description": manifest["endpoint"]["description"],
        "category": manifest["endpoint"]["category"],
    },
)
zc.register_service(info)

# Register tools based on user requirements
# @server.tool()
# async def tool_name(params): ...
```

---

## Version

DMED Protocol Version: 1.0 (Draft)
Author: Nilesh
License: Open specification
