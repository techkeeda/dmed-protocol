# Appendix I: DMED-MCP Gateway (Tier 3)

## I.1 Overview

The DMED-MCP Gateway bridges local physical devices to the internet by:

1. Running on a local machine with access to the physical network and BLE
2. Discovering all DMED devices continuously using the Discovery Framework (Appendix H)
3. Translating each discovered device's actions into MCP tools
4. Serving a standard MCP server endpoint that remote AI agents can connect to
5. Proxying MCP tool calls to the physical device and returning results

The gateway requires no per-device configuration. When a new DMED device appears on the local network, the gateway automatically exposes its actions as MCP tools within seconds.

```
Remote Claude / AI Agent
     │
     │  MCP (HTTPS + API key auth)
     ↓
┌─────────────────────────────────────────────────────┐
│  DMED-MCP Gateway                                   │
│  ┌─────────────────┐   ┌──────────────────────────┐ │
│  │ Discovery (Tier2)│   │ MCP Server               │ │
│  │ BLE + mDNS scan  │──▶│ tools/list               │ │
│  └─────────────────┘   │ tools/call → dispatch     │ │
└────────────────────────┴──────────────────────────┘─┘
     │
     ├── BLE → [Embedded device]
     ├── mDNS → [Coffee machine]
     └── Ethernet → [Industrial sensor]
```

---

## I.2 MCP Tool Mapping

Each DMED device action is exposed as one MCP tool. The mapping is deterministic and requires no configuration.

### I.2.1 Tool Name

```
{device_slug}__{action_name}
```

- `device_slug`: device name, lowercased, non-alphanumeric characters replaced with `_`, truncated to 32 characters
- `__` (double underscore): separator between device and action
- `action_name`: action name verbatim from the Capability Card (already lowercase per spec)

**Examples:**

| DMED Device | DMED Action | MCP Tool Name |
|---|---|---|
| `Smart Coffee Machine` | `brew_coffee` | `smart_coffee_machine__brew_coffee` |
| `VoIP Phone #1` | `make_call` | `voip_phone_1__make_call` |
| `Kitchen Light` | `toggle` | `kitchen_light__toggle` |
| `Thermostat (Living Room)` | `set_temperature` | `thermostat_living_room__set_temperature` |

### I.2.2 Tool Description

```
{action.description} ({device.name})
```

Example: `"Brew a coffee with specified type and size (Smart Coffee Machine)"`

### I.2.3 Tool Input Schema

The `inputSchema` is copied verbatim from the DMED Capability Card action definition. No transformation is applied.

### I.2.4 Tool Result

The MCP tool result is the `result.text` string from the DMED action response:

```
DMED: { "status": "ok", "action": "brew_coffee", "result": { "text": "☕ Your latte is ready!" } }
 MCP: { "content": [{ "type": "text", "text": "☕ Your latte is ready!" }] }
```

On DMED error:
```
DMED: { "status": "error", "action": "brew_coffee", "message": "Water level too low" }
 MCP: isError: true, content: [{ "type": "text", "text": "Water level too low" }]
```

---

## I.3 Gateway API Surface

### I.3.1 MCP Endpoints (served to remote AI agents)

| Method | Path | Description |
|---|---|---|
| `GET` | `/mcp` | MCP server info (name, version, capabilities) |
| `POST` | `/mcp` | MCP JSON-RPC endpoint (`tools/list`, `tools/call`, etc.) |

### I.3.2 Management Endpoints (local access only)

| Method | Path | Description |
|---|---|---|
| `GET` | `/dmed/devices` | List all currently discovered local DMED devices |
| `POST` | `/dmed/refresh` | Trigger a fresh scan immediately |
| `GET` | `/dmed/allowlist` | Get current device/action allowlist |
| `POST` | `/dmed/allowlist` | Update device/action allowlist |

Management endpoints MUST NOT be accessible over WAN. They are for local administration only.

---

## I.4 Device Allowlist

Not all local DMED devices should be WAN-accessible. The gateway MUST support a device allowlist configuration.

### I.4.1 Configuration Format

```json
{
  "allowlist": [
    {
      "instance_id": "c0ffee01",
      "name": "Smart Coffee Machine",
      "actions": ["brew_coffee", "get_status"]
    },
    {
      "instance_id": "*",
      "actions": ["get_status"]
    }
  ]
}
```

- `instance_id: "*"` matches all devices
- `actions: ["*"]` allows all actions on the matched device
- Devices not in the allowlist are NOT exposed as MCP tools
- Actions not in the allowlist are NOT callable remotely

### I.4.2 Default Behaviour

If no allowlist is configured, the gateway MUST NOT expose any devices (fail-closed). An explicit allowlist is required before any device becomes WAN-accessible.

---

## I.5 Security

### I.5.1 Authentication

All requests to the MCP endpoint (`/mcp`) MUST be authenticated with an API key:

```http
POST /mcp HTTP/1.1
X-API-Key: dmed-gw-key-abc123
Content-Type: application/json
```

The gateway MUST return HTTP 401 for any request without a valid API key.

API keys are configured in the gateway config file and MUST NOT be hardcoded.

### I.5.2 Transport Security

- The MCP endpoint MUST use HTTPS (TLS 1.2 or later) for WAN access
- The management endpoints MUST only be accessible on `localhost` or the local network
- The gateway MUST NOT log action parameters or results to persistent storage without explicit configuration

### I.5.3 Trust Boundary

```
                    ┌─────────────────────┐
Trusted (no auth) → │ Local DMED devices  │
                    └─────────────────────┘
                             ↑
                    ┌─────────────────────┐
Auth required     → │ DMED-MCP Gateway    │ ← Allowlist enforced
                    └─────────────────────┘
                             ↑
Auth + TLS        → │ Remote AI agents    │
```

Local DMED is trusted (same physical space). WAN access requires API key + TLS + allowlist.

---

## I.6 Device Lifecycle

### I.6.1 Device Discovery

When a new DMED device is discovered by the Discovery Framework:
1. Gateway checks allowlist — if not permitted, device is ignored
2. Gateway fetches the Capability Card
3. Gateway generates MCP tool definitions from the card (I.2)
4. Tools become available in the next `tools/list` response

### I.6.2 Device Loss

When a device is lost (BLE out of range, mDNS goodbye, network timeout):
1. Its MCP tools are removed from `tools/list`
2. Any in-flight `tools/call` returns an error: `"Device offline: {device.name}"`
3. Gateway continues serving other devices normally

### I.6.3 Device Reconnect

When a lost device reappears:
1. Its MCP tools are re-added to `tools/list`
2. No action required from the remote AI agent

---

## I.7 MCP tools/list Response Example

```json
{
  "tools": [
    {
      "name": "smart_coffee_machine__brew_coffee",
      "description": "Brew a coffee with specified type and size (Smart Coffee Machine)",
      "inputSchema": {
        "type": "object",
        "properties": {
          "drink_type": { "type": "string", "enum": ["espresso", "americano", "latte", "cappuccino"] },
          "size":       { "type": "string", "enum": ["small", "medium", "large"] }
        },
        "required": ["drink_type", "size"]
      }
    },
    {
      "name": "smart_coffee_machine__get_status",
      "description": "Get current machine status (Smart Coffee Machine)",
      "inputSchema": { "type": "object", "properties": {} }
    },
    {
      "name": "kitchen_light__toggle",
      "description": "Toggle the light on or off (Kitchen Light)",
      "inputSchema": { "type": "object", "properties": {} }
    }
  ]
}
```

---

## I.8 End-to-End Flow

```
1. Remote Claude connects: POST /mcp (initialize)
2. Claude calls: tools/list
3. Gateway returns: all tools from all allowlisted, online DMED devices
4. Claude decides to brew coffee
5. Claude calls: tools/call {name: "smart_coffee_machine__brew_coffee", arguments: {drink_type: "latte", size: "large"}}
6. Gateway looks up: smart_coffee_machine → instance_id "c0ffee01" → mDNS transport
7. Gateway dispatches: POST http://192.168.1.42:3100/dmed/action {action: "brew_coffee", params: {drink_type: "latte", size: "large"}}
8. Coffee machine responds: {status: "ok", result: {text: "☕ Your large latte is ready!"}}
9. Gateway returns to Claude: {content: [{type: "text", text: "☕ Your large latte is ready!"}]}
10. Claude: "Your large latte is brewing ☕"
```

---

## I.9 Conformance Requirements

A **DMED v0.2 compliant MCP Gateway** MUST:

1. Use the Discovery Framework (Appendix H) or equivalent for local device discovery
2. Map DMED actions to MCP tools using the naming rules in I.2.1
3. Proxy MCP `tools/call` to DMED `POST /dmed/action` and translate the response (I.2.4)
4. Require API key authentication for all MCP endpoint access
5. Enforce a device allowlist (fail-closed — no devices exposed without explicit config)
6. Restrict management endpoints to local network only
7. Remove MCP tools when their device goes offline (I.6.2)
8. Add MCP tools automatically when new devices are discovered (I.6.1)

A **DMED v0.2 compliant MCP Gateway** SHOULD:

1. Use HTTPS with a valid certificate for the MCP endpoint
2. Support `tools/list` change notification via MCP SSE when device list changes
3. Log all remote `tools/call` invocations (action name, timestamp) for audit purposes
4. Provide a local web UI for allowlist management

---

## I.10 Reference Implementation

See `gateway/` in the repository for the reference Node.js/TypeScript implementation.

**Minimum deployment (Raspberry Pi):**
- RAM: 256 MB
- Storage: 1 GB
- Network: WiFi + Bluetooth 4.0+
- OS: Raspberry Pi OS Lite (64-bit)

**Quick start:**
```bash
git clone https://github.com/techkeeda/dmed-protocol
cd dmed-protocol/gateway
cp config.json.example config.json
# Edit config.json: set api_key and allowlist
npm install && npm start
```

Then add to Claude Desktop `~/.config/claude/claude_desktop_config.json`:
```json
{
  "mcpServers": {
    "dmed-home": {
      "url": "https://your-gateway-host/mcp",
      "headers": { "X-API-Key": "your-api-key" }
    }
  }
}
```
