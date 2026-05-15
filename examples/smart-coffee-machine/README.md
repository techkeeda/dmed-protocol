# ☕ Smart Coffee Machine — DMED Example

A smart coffee machine that broadcasts itself as an **MCP endpoint** on the local network using the **DMED (Dynamic MCP Endpoint Discovery) Protocol**, allowing any DMED-compatible AI client to find and interact with it.

## What It Does

When running, this MCP endpoint:
1. Exposes tools: `brew_coffee`, `get_status`, `set_temperature`
2. Broadcasts itself via mDNS (`_dmed._tcp`) so clients can discover it
3. Serves its manifest at `http://<local-ip>:3100/dmed-manifest.json`

## Setup

```bash
npm install
npm start
```

## Discovery

Any DMED client on the same network will see:

```
☕ Smart Coffee Machine (iot)
"Control your coffee machine — brew drinks, check status, set temperature"
```

## Interacting

Once a DMED client connects, users can say things like:
- "Make me a large latte"
- "What's the water level?"
- "Set temperature to 90 degrees"

## MCP Client Config (manual)

If you want to connect directly without discovery:

```json
{
  "mcpServers": {
    "coffee-machine": {
      "command": "node",
      "args": ["server.js"],
      "cwd": "/path/to/smart-coffee-machine"
    }
  }
}
```
