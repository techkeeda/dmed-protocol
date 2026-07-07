# DMED Cookbook

Copy-paste recipes for making anything discoverable via DMED.

---

## 1. Temperature Sensor

```python
from dmed import Card, Tool, Transport, DMEDServer
import random

card = Card("temp0001", "Room Sensor", "environmental",
    transports=[Transport("http")],
    tools=[Tool("get_temperature", "Current temperature in Celsius")])

DMEDServer(card, 8080, lambda n, a: f"{20+random.random()*5:.1f}°C").start()
```

## 2. File Sharing

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("file0001", "Shared Notes", "information",
    transports=[Transport("http")],
    tools=[Tool("read", "Read file content")])

DMEDServer(card, 8080, lambda n, a: open("/tmp/notes.txt").read()).start()
```

## 3. Local LLM

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("llm00001", "Local Llama", "ai_service",
    transports=[Transport("http")],
    tools=[Tool("generate", "Generate text from prompt"),
           Tool("summarize", "Summarize text")])

def handle(name, args):
    # Replace with actual model call
    if name == "generate": return f"[Response to: {args.get('prompt','')}]"
    if name == "summarize": return f"[Summary of {len(args.get('text',''))} chars]"

DMEDServer(card, 8080, handle).start()
```

## 4. Smart Lock

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("lock0001", "Front Door", "security",
    transports=[Transport("http")], auth_type="pin",
    tools=[Tool("lock", "Lock the door"), Tool("unlock", "Unlock the door"),
           Tool("status", "Get lock status")])

state = {"locked": True}
def handle(name, args):
    if name == "lock": state["locked"] = True; return "Locked"
    if name == "unlock": state["locked"] = False; return "Unlocked"
    if name == "status": return f"{'Locked' if state['locked'] else 'Unlocked'}"

DMEDServer(card, 8080, handle).start()
```

## 5. Any CLI Tool as MCP Endpoint

```python
import subprocess
from dmed import Card, Tool, Transport, DMEDServer

card = Card("cli00001", "System Info", "computing",
    transports=[Transport("http")],
    tools=[Tool("uptime", "System uptime"), Tool("disk", "Disk usage"),
           Tool("memory", "Memory usage")])

def handle(name, args):
    cmds = {"uptime": "uptime", "disk": "df -h /", "memory": "free -h"}
    return subprocess.check_output(cmds[name], shell=True).decode().strip()

DMEDServer(card, 8080, handle).start()
```

## 6. Temperature Sensor (JavaScript/TypeScript)

`lib/js` is a discovery/dispatch library, not a server-authoring helper like Python's
`DMEDServer` — there's no one-liner class yet. Until there is (see
[CONTRIBUTING.md](../CONTRIBUTING.md) if you'd like to add one), a device in JS is just
`express` + `bonjour-service` serving the three DMED endpoints directly, same shape as
[`examples/smart-coffee-machine/server.js`](../examples/smart-coffee-machine/server.js):

```js
import express from 'express'
import bonjourService from 'bonjour-service'
const Bonjour = bonjourService.default || bonjourService

const card = {
  dmed_version: '0.2.0',
  instance_id: 'temp0001',
  name: 'Room Sensor',
  service_type: 'environmental',
  transports: [{ type: 'http', url: 'http://localhost:8080/mcp', priority: 1 }],
  auth: { type: 'none' },
  capabilities: { tools: [{ name: 'get_temperature', description: 'Current temperature in Celsius' }] }
}

function handle(name) {
  if (name === 'get_temperature') return `${(20 + Math.random() * 5).toFixed(1)}°C`
}

const app = express()
app.get('/dmed/card', (req, res) => res.json(card))
app.get('/dmed/actions', (req, res) =>
  res.json({ actions: card.capabilities.tools.map(t => ({ name: t.name, description: t.description })) })
)
app.post('/dmed/action', express.json(), (req, res) => {
  const { action } = req.body ?? {}
  if (!action) return res.status(400).json({ status: 'error', message: 'Missing "action" field' })
  if (!card.capabilities.tools.some(t => t.name === action)) {
    return res.status(404).json({ status: 'error', message: `Unknown action: ${action}` })
  }
  res.json({ status: 'ok', action, result: { text: handle(action) } })
})
app.listen(8080)

new Bonjour().publish({ name: card.name, type: 'dmed', port: 8080, txt: { v: '1', id: card.instance_id } })
```

Once it's running, self-check it with the conformance CLI before calling it done — see
[Gateway Recipes](#gateway-recipes) below.

## Pattern: Wrap Anything in 4 Lines

```python
from dmed import Card, Tool, Transport, DMEDServer

card = Card("<id>", "<name>", "<type>", transports=[Transport("http")],
            tools=[Tool("<tool_name>", "<description>")])

DMEDServer(card, 8080, lambda name, args: YOUR_FUNCTION(args)).start()
```

---

## Gateway Recipes

Recipes for [`gateway/`](../gateway/) — the piece that bridges your local devices to
remote MCP clients. See [`gateway/README.md`](../gateway/README.md) for full setup.

### Check a device is conformant before wiring it up

Run this against any device — yours or someone else's — before assuming the gateway will
pick it up correctly:

```bash
node lib/js/dist/cli.js http://<device-ip>:<port>
```

### Expose one device, read-only, to the WAN

Everything discovered is exposed by default (fine for LAN-only use). Before putting the
gateway on the internet, narrow it down in `gateway/config.json`:

```json
{
  "apiKey": "a-long-random-string",
  "allowlist": {
    "devices": ["smart_coffee_machine"],
    "actions": { "smart_coffee_machine": ["get_status"] }
  }
}
```

`brew_coffee` and `set_temperature` stay fully usable on the LAN — the allowlist only
governs what the gateway itself exposes through `/mcp`.

### Connect Claude Desktop

Claude Desktop spawns a local stdio process rather than speaking HTTP directly. Point it
at the bundled bridge in your `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "dmed-gateway": {
      "command": "node",
      "args": ["/absolute/path/to/gateway/dist/stdio-bridge.js"],
      "env": {
        "DMED_GATEWAY_URL": "http://localhost:4100",
        "DMED_GATEWAY_API_KEY": "a-long-random-string"
      }
    }
  }
}
```

Restart Claude Desktop after saving. It should list your devices' actions as tools
without ever needing to speak HTTP or see the underlying API key.
