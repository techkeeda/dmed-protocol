# Appendix D: Internet Transport Detail (DNS + HTTPS + Gateway)

## D.0 Two WAN Scenarios

There are two distinct ways a DMED device can be accessible over the internet:

### Scenario A — Direct Internet Endpoint
A device or service is directly hosted on the public internet with its own domain name. Discovery uses DNS TXT records. Capability Cards are served over HTTPS. This is appropriate for cloud-hosted AI services, SaaS tools, and public APIs.

```
Remote Client
     │ DNS TXT lookup
     ↓
_dmed.api.example.com → card URL → capabilities → MCP session
```

Sections D.1–D.9 cover this scenario.

### Scenario B — Local Devices Bridged via DMED-MCP Gateway
Physical devices on a local network (BLE, mDNS, Ethernet) are not directly internet-accessible. A **DMED-MCP Gateway** runs on a local machine, discovers DMED devices using the Discovery Framework (Appendix H), and exposes them as MCP tools to remote AI agents.

```
Remote Claude / AI Agent
     │ MCP (HTTPS)
     ↓
DMED-MCP Gateway  ← runs on local machine / home server / Pi
     │
     ├── BLE → [Embedded device]
     ├── mDNS → [Coffee machine]
     └── Ethernet → [Industrial sensor]
```

This is the recommended approach for making local physical devices WAN-accessible. See Appendix I for the full gateway specification.

### Choosing the Right Scenario

| | Scenario A (Direct) | Scenario B (Gateway) |
|---|---|---|
| **Device location** | Public internet | Local network / BLE |
| **Device type** | Cloud service, SaaS | Physical device, embedded |
| **Discovery** | DNS TXT record | Gateway auto-discovers |
| **Access control** | Device handles auth | Gateway enforces allowlist |
| **Setup** | DNS record + HTTPS cert | Install gateway, configure allowlist |

### Alignment with MCP Server Cards

Anthropic's official MCP roadmap includes `.well-known/mcp.json` Server Cards for internet-scale discovery of MCP tools. DMED Scenario A is complementary — DMED adds structured capability cards, service type classification, and multi-transport metadata on top of the `.well-known` pattern. Implementations MAY serve both `/.well-known/mcp.json` (MCP standard) and `/.well-known/dmed/card` (DMED) from the same server.

---

## D.1 Overview (Scenario A — Direct Internet Endpoint)

The Internet transport binding enables discovery of DMED servers hosted on the public internet. This is used for cloud-hosted AI services, public APIs, and remote MCP endpoints.

Discovery uses DNS TXT records. Capability Cards are served over HTTPS.

## D.2 DNS TXT Record

### Record Location

```
_dmed.<domain> IN TXT "<key-value pairs>"
```

Example:
```
_dmed.api.example.com. 3600 IN TXT "v=1;id=f1e2d3c4;st=08;fl=3;url=https://api.example.com/mcp;name=Example AI Service"
```

### Field Format

Fields are semicolon-separated key=value pairs within a single TXT record string:

| Key | Required | Description |
|-----|----------|-------------|
| `v` | REQUIRED | DMED version (integer) |
| `id` | REQUIRED | Instance ID (8-char hex) |
| `st` | REQUIRED | Service type (2-char hex) |
| `fl` | REQUIRED | Flags (1-char hex) |
| `url` | REQUIRED | Full HTTPS URL to MCP endpoint |
| `name` | RECOMMENDED | Human-readable name |
| `card` | OPTIONAL | Full HTTPS URL to Capability Card (defaults to `https://<domain>/.well-known/dmed/card`) |

### Multiple Servers Per Domain

A domain MAY publish multiple `_dmed` records for different subdomains:

```
_dmed.chat.example.com.   3600 IN TXT "v=1;id=aaaa1111;st=08;url=https://chat.example.com/mcp;name=Chat AI"
_dmed.images.example.com. 3600 IN TXT "v=1;id=bbbb2222;st=08;url=https://images.example.com/mcp;name=Image Gen"
```

## D.3 Well-Known Capability Card Endpoint

### URL

```
https://<domain>/.well-known/dmed/card
```

If the `card` field is present in the DNS TXT record, use that URL instead.

### Request

```http
GET /.well-known/dmed/card HTTP/1.1
Host: api.example.com
Accept: application/json
```

### Response

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Cache-Control: public, max-age=3600
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, OPTIONS
Access-Control-Allow-Headers: Accept

{
  "dmed_version": "0.2.0",
  "instance_id": "f1e2d3c4",
  "name": "Example AI Service",
  "service_type": "ai_service",
  "transports": [
    {
      "type": "https",
      "url": "https://api.example.com/mcp",
      "priority": 1,
      "bandwidth": "high",
      "latency_ms": 100
    }
  ],
  "auth": {
    "type": "oauth2",
    "oauth2": {
      "authorization_url": "https://auth.example.com/authorize",
      "token_url": "https://auth.example.com/token",
      "scopes": ["tools:read", "tools:execute"],
      "client_id": "dmed_public_client"
    }
  },
  "capabilities": {
    "tools": [
      {"name": "generate_text", "description": "Generate text from a prompt"},
      {"name": "summarize", "description": "Summarize a document"},
      {"name": "translate", "description": "Translate text between languages"}
    ],
    "resources": [],
    "prompts": [
      {"name": "creative_writing", "description": "Prompt template for creative writing"}
    ]
  }
}
```

## D.4 TLS Requirements

1. Capability Card endpoints MUST use HTTPS (TLS 1.2 or later).
2. MCP endpoints advertised in Internet transport MUST use HTTPS or WSS.
3. Servers MUST present a valid certificate trusted by public CAs.
4. Clients MUST validate server certificates.
5. Clients MUST NOT proceed if certificate validation fails.

## D.5 CORS Headers

Since browser-based DMED clients may fetch Capability Cards cross-origin, servers MUST include CORS headers:

```http
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, OPTIONS
Access-Control-Allow-Headers: Accept, Authorization
```

For the MCP endpoint itself, CORS headers depend on the MCP transport specification.

## D.6 DNS Discovery Procedure (Client)

```
1. Client has a domain name (from user input, QR code, NFC tag, or directory)
2. Query DNS: _dmed.<domain> TXT
3. Parse TXT record fields
4. If 'card' field present: fetch that URL
   Else: fetch https://<domain>/.well-known/dmed/card
5. Parse Capability Card
6. Establish MCP session at the advertised URL
```

### Pseudocode

```python
import dns.resolver

def discover_internet_dmed(domain: str) -> dict:
    # Step 1: DNS TXT lookup
    try:
        answers = dns.resolver.resolve(f"_dmed.{domain}", "TXT")
    except dns.resolver.NXDOMAIN:
        return None

    # Step 2: Parse TXT record
    txt_data = answers[0].strings[0].decode()
    fields = {}
    for pair in txt_data.split(";"):
        key, _, value = pair.strip().partition("=")
        fields[key] = value

    # Step 3: Fetch Capability Card
    card_url = fields.get("card", f"https://{domain}/.well-known/dmed/card")
    card = https_get_json(card_url)

    # Step 4: Validate
    assert card["instance_id"] == fields["id"]

    return card
```

## D.7 DNS TTL and Caching

| Record | Recommended TTL | Rationale |
|--------|----------------|-----------|
| `_dmed.<domain>` TXT | 3600 seconds (1 hour) | Balance between freshness and DNS load |
| Capability Card (HTTP) | Cache-Control: max-age=3600 | Match DNS TTL |

Clients SHOULD respect DNS TTL for re-query intervals.
Clients SHOULD respect HTTP Cache-Control headers for card re-fetch intervals.

## D.8 Discovery via Well-Known URI (Alternative)

For domains that cannot modify DNS records, an alternative discovery path:

```
GET https://<domain>/.well-known/dmed/discovery HTTP/1.1
```

Response:
```json
{
  "servers": [
    {
      "instance_id": "f1e2d3c4",
      "name": "Example AI Service",
      "card_url": "https://api.example.com/.well-known/dmed/card"
    }
  ]
}
```

This is OPTIONAL and complementary to DNS-based discovery.

## D.9 Rate Limiting

Internet-facing DMED servers SHOULD implement rate limiting on the card endpoint:

- Recommended: 60 requests per minute per IP
- Response when limited: HTTP 429 with `Retry-After` header
- Clients MUST respect `Retry-After` headers
