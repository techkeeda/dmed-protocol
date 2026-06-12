# DMED (Dynamic MCP Endpoint Discovery) — AI Prompt Library

## Instructions for AI Tools

You are helping a user make their service, device, or application discoverable as an **MCP endpoint** via the **DMED (Dynamic MCP Endpoint Discovery) Protocol v0.2**. DMED allows MCP endpoints to broadcast their capabilities over local network transports (WiFi/mDNS, Bluetooth, Ethernet) so that any DMED-compatible client app can automatically discover, connect, and interact with them via AI.

---

## Your Task

When a user describes what they want to make discoverable, you must:

1. **Create an MCP endpoint** that exposes the described functionality as tools
2. **Add DMED discovery broadcasting** so clients can find it automatically
3. **Generate a DMED manifest** describing the endpoint's capabilities
4. **Expose the interaction API** (`/dmed/action`, `/dmed/actions`) for lightweight command execution

---

## DMED Protocol Specification (v0.2)

### Protocol Lifecycle

```
Discover → Connect → Interact → Disconnect
```

1. **Discover** — Endpoint broadcasts via mDNS (`_dmed._tcp`) or BLE
2. **Connect** — Client fetches `GET /dmed/card` to learn capabilities
3. **Interact** — Client sends actions via `POST /dmed/action` (lightweight) or `POST /mcp` (full MCP)
4. **Disconnect** — Client stops sending requests (stateless, no handshake needed)

### DMED Capability Card (served at `/dmed/card`)

Every DMED-enabled MCP endpoint must serve a capability card:

```json
{
  "dmed_version": "0.2.0",
  "instance_id": "<8-char-hex-id>",
  "name": "<human-readable-name>",
  "description": "<what-this-endpoint-does>",
  "service_type": "<category: iot_device | ai_service | data_source | media | tool_utility | retail_kiosk | information | custom>",
  "transports": [
    {
      "type": "http",
      "url": "http://<host>:<port>/mcp",
      "priority": 1
    }
  ],
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
  "auth": {
    "type": "none | pin | api_key | oauth2"
  },
  "tags": ["<keyword1>", "<keyword2>"],
  "metadata": {}
}
```

### HTTP API (Required for v0.2 compliance)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/dmed/card` | Fetch endpoint capabilities |
| GET | `/dmed/actions` | List available actions with param schemas |
| POST | `/dmed/action` | Send a lightweight action/command |
| POST | `/mcp` | Full MCP JSON-RPC endpoint |

### Action Request/Response Format

**Request** (`POST /dmed/action`):
```json
{
  "action": "<tool-name>",
  "params": { "<key>": "<value>" }
}
```

**Success Response:**
```json
{
  "status": "ok",
  "action": "<tool-name>",
  "result": { "text": "<result-text>" }
}
```

**Error Response:**
```json
{
  "status": "error",
  "action": "<tool-name>",
  "message": "<error-description>"
}
```

### Discovery Broadcasting

#### mDNS/DNS-SD (WiFi/Ethernet)

Broadcast using service type `_dmed._tcp` with TXT records:

| TXT Record     | Value                           |
|----------------|---------------------------------|
| `dmed_version` | `0.2.0`                         |
| `name`         | Endpoint display name           |
| `description`  | Short description               |
| `category`     | Endpoint category               |
| `card`         | URL path to capability card     |

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
├── server.js          # MCP endpoint + DMED broadcasting + action API
├── package.json
└── README.md
```

### package.json dependencies:

```json
{
  "dependencies": {
    "@modelcontextprotocol/sdk": "^1.0.0",
    "bonjour-service": "^1.2.0",
    "express": "^4.21.0"
  }
}
```

### server.js template:

```javascript
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import Bonjour from "bonjour-service";
import express from "express";
import { readFileSync } from "fs";
import { networkInterfaces } from "os";

// Load manifest
const manifest = JSON.parse(readFileSync("./dmed-manifest.json", "utf-8"));

// --- MCP Endpoint Setup ---
const server = new McpServer({
  name: card.name,
  version: "1.0.0",
});

// TOOLS GO HERE — generate based on user's requirements
// server.tool("tool_name", "description", { ...schema }, async (params) => { ... });

// --- Tool handler map (for action API) ---
const toolHandlers = {};
// toolHandlers["tool_name"] = async (params) => { return "result"; };

// --- HTTP API (DMED v0.2) ---
function getLocalIP() {
  const interfaces = networkInterfaces();
  for (const iface of Object.values(interfaces)) {
    for (const addr of iface) {
      if (addr.family === "IPv4" && !addr.internal) return addr.address;
    }
  }
  return "127.0.0.1";
}

const localIP = getLocalIP();
const port = PORT;
const app = express();
app.use(express.json());

// GET /dmed/card — Capability Card
app.get("/dmed/card", (req, res) => res.json(manifest));

// GET /dmed/actions — List available actions
app.get("/dmed/actions", (req, res) => {
  res.json({
    actions: manifest.capabilities.tools.map(t => ({
      name: t.name, description: t.description, params: t.inputSchema
    }))
  });
});

// POST /dmed/action — Lightweight action invocation
app.post("/dmed/action", async (req, res) => {
  const { action, params } = req.body;
  if (!action) return res.status(400).json({ status: "error", message: "Missing 'action' field" });
  const handler = toolHandlers[action];
  if (!handler) return res.status(404).json({ status: "error", action, message: `Unknown action: ${action}` });
  try {
    const result = await handler(params || {});
    res.json({ status: "ok", action, result: { text: String(result) } });
  } catch (e) {
    res.status(500).json({ status: "error", action, message: e.message });
  }
});

app.listen(port, () => {
  console.error(`[DMED] Endpoints at http://${localIP}:${port}`);
});

// --- DMED Discovery Broadcasting ---
const bonjour = new Bonjour();
bonjour.publish({
  name: card.name,
  type: "dmed",
  port,
  txt: {
    dmed_version: manifest.dmed_version,
    name: card.name,
    description: card.description,
    category: card.service_type,
    card: `/dmed/card`,
  },
});

console.error(`[DMED] Broadcasting MCP endpoint: ${card.name}`);

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

1. Always generate a complete `dmed-manifest.json` first (with `dmed_version: "0.2.0"`)
2. Generate a working MCP endpoint with all tools implemented
3. Expose all 4 HTTP endpoints: `/dmed/card`, `/dmed/actions`, `/dmed/action`, `/mcp`
4. Include mDNS broadcasting by default
5. Add Bluetooth BLE advertising if the user mentions mobile/Bluetooth
6. Use the user's language/platform preference (Node.js default, Python if requested)
7. Include a README with setup instructions
8. Keep the implementation minimal and functional
9. Use secure defaults (token auth if the endpoint handles sensitive data)

---

## Python Alternative

If the user prefers Python:

```python
from dmed import Card, Tool, Transport, DMEDServer

# Define endpoint
card = Card(
    instance_id="<generated-id>",
    name="<endpoint-name>",
    service_type="<category>",
    description="<description>",
    transports=[Transport("http")],
    tools=[
        Tool("<tool-name>", "<description>", {"type": "object", "properties": {...}}),
    ]
)

# Implement tool logic
def handle(name, args):
    if name == "<tool-name>":
        return "<result>"

# Start — auto-registers mDNS + exposes /dmed/card, /dmed/actions, /dmed/action, /mcp
DMEDServer(card, port=8080, tool_handler=handle).start()
```

---

## Version

DMED Protocol Version: 0.2 (Draft)
Author: Nilesh Valmik Ladhe
License: Open specification
