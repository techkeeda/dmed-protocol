# DMED Gateway

A local process that discovers DMED devices on the network (via mDNS / `_dmed._tcp`) and
exposes them as MCP tools over a single HTTP JSON-RPC endpoint. Point any MCP-over-HTTP
client at it and it forwards `tools/call` to the right device's `/dmed/action` endpoint.

Each discovered device's actions become tools named `{device_slug}__{action_name}`, e.g.
a "Smart Coffee Machine" advertising `brew_coffee` becomes the tool
`smart_coffee_machine__brew_coffee`.

## Setup

```bash
cd gateway
npm install       # also links the local @dmed/discovery package from ../lib/js
npm run build     # compiles src/ -> dist/
cp config.json.example config.json
```

Edit `config.json` and set a real `apiKey`:

```json
{
  "apiKey": "a-long-random-string",
  "port": 4100
}
```

The gateway refuses to start if `config.json` is missing or has no `apiKey` — there is no
hardcoded fallback key.

## Running

```bash
npm start
```

This starts mDNS discovery and the HTTP server. As DMED devices come online they're logged:

```
[Gateway] DMED Gateway listening on :4100
[Gateway] MCP endpoint: POST http://localhost:4100/mcp
[Gateway] + Smart Coffee Machine (3 tools registered)
```

## Using it

All requests are JSON-RPC 2.0 POSTs to `/mcp`, authenticated with an `X-API-Key` header.

```bash
curl -X POST http://localhost:4100/mcp \
  -H "Content-Type: application/json" \
  -H "X-API-Key: a-long-random-string" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'

curl -X POST http://localhost:4100/mcp \
  -H "Content-Type: application/json" \
  -H "X-API-Key: a-long-random-string" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"smart_coffee_machine__brew_coffee","arguments":{"drink_type":"latte","size":"large"}}}'
```

Supported methods: `initialize`, `notifications/initialized`, `tools/list`, `tools/call`.

### Connecting an MCP client

This implements the JSON-RPC method surface of MCP over a plain HTTP POST endpoint. Most
desktop MCP clients (including Claude Desktop) expect a stdio-transport server or the full
Streamable HTTP transport (session headers, SSE upgrade). This gateway does not yet
implement session handling or SSE, so wiring it into Claude Desktop directly requires a
small stdio↔HTTP bridge in front of `/mcp` (or use it directly via `curl`/any HTTP-capable
MCP client for now). Full Streamable HTTP / Claude Desktop compatibility is tracked for a
later pass.

## Development

```bash
npm test          # unit tests (tool-mapper, proxy, auth) + integration test against a
                   # live in-process instance of examples/smart-coffee-machine
npm run test:watch
```

The integration test in `tests/gateway.test.ts` starts the real coffee machine Express app
in-process (bypassing mDNS, which isn't reliable in CI/sandboxes) and drives real HTTP
card-fetch and action-dispatch calls through the gateway.

## Architecture

| File | Responsibility |
|---|---|
| `src/discovery.ts` | Browses `_dmed._tcp` via `bonjour-service`, fetches each device's card, feeds `@dmed/discovery`'s `DmedDiscovery` base class |
| `src/tool-mapper.ts` | `slugify()`, `toolsFromCard()`, and the in-memory tool/device registry |
| `src/proxy.ts` | Resolves an MCP tool name back to a device + action, dispatches via `@dmed/discovery`'s `dispatchAction()` |
| `src/auth.ts` | Loads `config.json`, `X-API-Key` middleware |
| `src/server.ts` | Express app implementing the MCP JSON-RPC methods |
| `src/index.ts` | Wires discovery → registry → server and starts listening |
