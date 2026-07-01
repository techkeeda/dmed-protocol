# DMED VoIP Phone Server (C)

A cross-compilable DMED server that makes a VoIP phone discoverable on the local network. Implements all 10 tools from the `things/communication/voip-phone.json` template.

Zero external dependencies — uses only POSIX sockets. Designed for embedded Linux targets.

## Build

```bash
# Native
make

# Cross-compile for ARM
make CROSS=arm-linux-gnueabihf-

# Cross-compile for MIPS (little-endian)
make CROSS=mipsel-linux-gnu-
```

## Run

```bash
./voip_phone_dmed          # default port 8080
./voip_phone_dmed 3200     # custom port
```

## Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/dmed/card` | DMED capability card |
| GET | `/dmed/actions` | List available actions (v0.2) |
| POST | `/dmed/action` | Invoke action (v0.2 lightweight) |
| POST | `/mcp` | Full MCP JSON-RPC |

## Tools

| Tool | Description |
|------|-------------|
| `make_call` | Dial a phone number |
| `get_missed_calls` | Get missed calls |
| `get_call_history` | Get call history (incoming/outgoing/missed) |
| `get_voicemail` | Get voicemail messages |
| `redial` | Redial last outgoing number |
| `get_active_calls` | Get currently active calls |
| `hangup` | End active call |
| `get_registration_status` | SIP registration status |
| `get_dect_handsets` | List paired DECT handsets |
| `do_not_disturb` | Enable/disable DND |

## Test

```bash
# Get capability card
curl http://localhost:8080/dmed/card | jq .

# Make a call (v0.2 action)
curl -X POST http://localhost:8080/dmed/action \
  -H "Content-Type: application/json" \
  -d '{"action":"make_call","params":{"number":"+14155551234","line":"fxs"}}'

# Get missed calls (MCP JSON-RPC)
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"get_missed_calls","arguments":{"limit":5}}}'
```
