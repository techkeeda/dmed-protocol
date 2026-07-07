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
hardcoded fallback key. `X-API-Key` is compared with a constant-time hash comparison, not
`===`, so a wrong guess can't be narrowed down by response timing.

### Exposing this to the WAN (optional)

Local-only use (LAN, trusted network) needs nothing beyond the above — every discovered
device is exposed. Before exposing the gateway to the internet, add:

```json
{
  "apiKey": "a-long-random-string",
  "port": 4100,
  "tls": {
    "cert": "/path/to/fullchain.pem",
    "key": "/path/to/privkey.pem"
  },
  "allowlist": {
    "devices": ["smart_coffee_machine"],
    "actions": { "smart_coffee_machine": ["get_status"] }
  }
}
```

- **`tls`** — starts the server over HTTPS using the given cert/key instead of plain HTTP.
  For real deployments, a reverse proxy (Caddy, nginx) with automatic cert renewal in front
  of plain HTTP is usually simpler than managing certs here directly — either works.
- **`allowlist`** — omit entirely to expose every discovered device and action (the
  Phase 3 default). Once set, only listed device slugs appear in `tools/list` at all;
  `actions` further narrows a listed device to specific action names (e.g. read-only
  remotely, full control locally). `loadConfig` fails fast if `tls` is set but the cert/key
  files don't exist — misconfiguration is caught at startup, not on the first request.

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

### Discovery: `.well-known/mcp/server-card.json`

Per [SEP-1649](https://github.com/modelcontextprotocol/modelcontextprotocol/issues/1649),
the gateway also serves an unauthenticated MCP Server Card:

```bash
curl http://localhost:4100/.well-known/mcp/server-card.json
```

No `X-API-Key` needed — discovery happens before a client has credentials, and per spec
this document must never contain secrets (it doesn't; it only describes the auth *scheme*,
not the key).

### Connecting an MCP client

This implements the JSON-RPC method surface of MCP over a plain HTTP POST endpoint, not
the full Streamable HTTP transport (session headers, SSE) that desktop MCP clients
normally expect. Any generic HTTP-capable MCP client can talk to it directly (see the
curl examples above). For **Claude Desktop**, which spawns a local stdio process rather
than speaking HTTP directly, use the bundled bridge:

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

(In `~/Library/Application Support/Claude/claude_desktop_config.json` on macOS, or the
equivalent path on your platform.) The bridge reads newline-delimited JSON-RPC from stdin,
forwards each line to `POST /mcp` with the configured API key, and writes the response
back to stdout — Claude Desktop never needs to speak HTTP or know the API key is even
there. Restart Claude Desktop after editing the config.

A native Streamable HTTP implementation (no bridge process needed at all) is a bigger,
separate piece of work — swapping the hand-rolled JSON-RPC handling in `server.ts` for
`@modelcontextprotocol/sdk`'s `StreamableHTTPServerTransport` would get most of it "for
free," but that's tracked separately, not done here.

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
| `src/tool-mapper.ts` | `slugify()`, `toolsFromCard()`, and the in-memory tool/device registry (allowlist-filtered) |
| `src/allowlist.ts` | `isDeviceAllowed()` / `isActionAllowed()` — pure functions over `config.allowlist` |
| `src/proxy.ts` | Resolves an MCP tool name back to a device + action, dispatches via `@dmed/discovery`'s `dispatchAction()` |
| `src/auth.ts` | Loads and validates `config.json`, `X-API-Key` middleware (constant-time comparison) |
| `src/listen.ts` | Picks plain HTTP or HTTPS (`config.tls`) to start the server |
| `src/server.ts` | Express app: MCP JSON-RPC methods + the unauthenticated `.well-known` route |
| `src/server-card.ts` | Builds the SEP-1649 MCP Server Card document |
| `src/constants.ts` | Shared server name/version/protocol version constants |
| `src/index.ts` | Wires discovery → registry → server and starts listening |
| `src/bridge.ts` | `handleLine()` — forwards one line of stdio JSON-RPC to `POST /mcp` |
| `src/stdio-bridge.ts` | CLI entry point Claude Desktop spawns; wraps `bridge.ts` over stdin/stdout |
