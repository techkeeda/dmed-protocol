# 🔍 DMED Client Scanner

Discovers MCP endpoints broadcasting on your local network via the **DMED (Dynamic MCP Endpoint Discovery) Protocol**.

## Usage

### Quick Scan (passive)

```bash
npm install
npm run scan
```

Output:
```
🔍 DMED Scanner — Searching for MCP endpoints...
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

✅ Found MCP endpoint: Smart Coffee Machine
   📝 Control your coffee machine — brew drinks, check status, set temperature
   📂 Category: iot
   🌐 Host: 192.168.1.42:3100
   📄 Manifest: http://192.168.1.42:3100/dmed-manifest.json
   🔖 DMED Version: 1.0
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### Interactive Client

```bash
node interactive-client.js
```

Scans the network, lists found MCP endpoints, and lets you select one to view its capabilities.

## How It Works

1. Listens for mDNS broadcasts on service type `_dmed._tcp`
2. Reads TXT records for endpoint metadata (name, service type, card URL)
3. Optionally fetches the full capability card to list available tools

## Testing Locally

1. Start the coffee machine example in one terminal:
   ```bash
   cd ../smart-coffee-machine && npm install && npm start
   ```

2. Run the scanner in another terminal:
   ```bash
   npm run scan
   ```

You should see the coffee machine MCP endpoint appear within a few seconds.
