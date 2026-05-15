# Tutorial: Share on the Internet

Make your MCP service discoverable globally via DNS TXT records and HTTPS.

---

## How It Works

```
Your Domain (DNS)                    Any DMED Client
┌─────────────────────┐            ┌─────────────────┐
│ _dmed.example.com    │───DNS─────►│ TXT lookup      │
│ TXT record          │            │                 │
│                     │            │ Finds URL       │
│ /.well-known/dmed/   │◄──HTTPS───│ GET /card       │
│ card                │            │                 │
│                     │            │ Reads card      │
│ /mcp                │◄══HTTPS══►│ MCP session     │
└─────────────────────┘            └─────────────────┘
```

## Step 1: Add DNS TXT Record

Add this to your domain's DNS zone:

```
_dmed.api.example.com. 3600 IN TXT "v=1;id=f1e2d3c4;st=08;fl=3;url=https://api.example.com/mcp;name=My AI Service"
```

**Fields:**
| Key | Required | Example | Description |
|-----|----------|---------|-------------|
| `v` | Yes | `1` | DMED version |
| `id` | Yes | `f1e2d3c4` | Instance ID (8 hex chars) |
| `st` | Yes | `08` | Service type (hex) |
| `fl` | Yes | `3` | Flags (hex): AUTH+TLS |
| `url` | Yes | `https://...` | MCP endpoint URL |
| `name` | Recommended | `My AI Service` | Display name |

### DNS Provider Examples

**Cloudflare:**
```
Type: TXT
Name: _dmed.api
Content: v=1;id=f1e2d3c4;st=08;fl=3;url=https://api.example.com/mcp;name=My AI Service
TTL: 3600
```

**AWS Route 53:**
```json
{
  "Type": "TXT",
  "Name": "_dmed.api.example.com",
  "TTL": 3600,
  "ResourceRecords": [{"Value": "\"v=1;id=f1e2d3c4;st=08;fl=3;url=https://api.example.com/mcp;name=My AI Service\""}]
}
```

## Step 2: Serve Capability Card

Serve a JSON file at `https://api.example.com/.well-known/dmed/card`:

```json
{
  "dmed_version": "0.2.0",
  "instance_id": "f1e2d3c4",
  "name": "My AI Service",
  "description": "Text generation and summarization API",
  "service_type": "ai_service",
  "transports": [
    {"type": "https", "url": "https://api.example.com/mcp", "priority": 1}
  ],
  "auth": {
    "type": "oauth2",
    "oauth2": {
      "authorization_url": "https://auth.example.com/authorize",
      "token_url": "https://auth.example.com/token",
      "scopes": ["read", "write"],
      "client_id": "dmed_public"
    }
  },
  "capabilities": {
    "tools": [
      {"name": "generate_text", "description": "Generate text from a prompt"},
      {"name": "summarize", "description": "Summarize a document"}
    ],
    "resources": [],
    "prompts": []
  }
}
```

**Required HTTP headers:**
```
Content-Type: application/json
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, OPTIONS
```

### Nginx config:

```nginx
location /.well-known/dmed/card {
    default_type application/json;
    add_header Access-Control-Allow-Origin *;
    alias /var/www/dmed-card.json;
}
```

### Express.js:

```javascript
app.get('/.well-known/dmed/card', (req, res) => {
  res.json(card);
});
```

## Step 3: Implement MCP Endpoint

Your MCP endpoint at the advertised URL must handle standard MCP over HTTPS:

```
POST https://api.example.com/mcp
Content-Type: application/json
Authorization: Bearer <token>

{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"summarize","arguments":{"text":"..."}}}
```

Use any MCP SDK or implement the JSON-RPC handlers manually (see examples/).

## Step 4: Verify

```bash
# Check DNS record
dig TXT _dmed.api.example.com

# Fetch card
curl https://api.example.com/.well-known/dmed/card | jq .

# Test MCP (after auth)
curl -X POST https://api.example.com/mcp \
  -H "Content-Type: application/json" \
  -H "Authorization: Bearer YOUR_TOKEN" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

## Requirements

- Valid TLS certificate (Let's Encrypt is fine)
- DNS zone access to add TXT records
- HTTPS server for card + MCP endpoint
- CORS headers on card endpoint (for browser clients)

## Multiple Services Per Domain

```
_dmed.chat.example.com.   TXT "v=1;id=aaa11111;st=08;url=https://chat.example.com/mcp;name=Chat"
_dmed.images.example.com. TXT "v=1;id=bbb22222;st=08;url=https://images.example.com/mcp;name=Images"
_dmed.code.example.com.   TXT "v=1;id=ccc33333;st=08;url=https://code.example.com/mcp;name=Code"
```

## Security Checklist

- [ ] TLS 1.2+ on all endpoints
- [ ] Valid certificate from trusted CA
- [ ] OAuth2 or API key for tool invocation
- [ ] Rate limiting on card endpoint (60 req/min/IP)
- [ ] No secrets in DNS TXT record or card
- [ ] CORS restricted to known origins (production)
