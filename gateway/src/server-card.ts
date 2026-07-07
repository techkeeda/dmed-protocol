import { PROTOCOL_VERSION, SERVER_NAME, SERVER_VERSION } from './constants.js'

// Shape per SEP-1649 (MCP Server Cards — HTTP Server Discovery via .well-known).
// Served unauthenticated at GET /.well-known/mcp/server-card.json — the whole point
// is pre-connection discovery, so this must never include the API key or any secret.
export interface McpServerCard {
  $schema: string
  version: string
  protocolVersion: string
  serverInfo: { name: string; version: string }
  transport: { type: 'http'; endpoint: string }
  capabilities: { tools: Record<string, never> }
  description: string
  authentication: { type: string; in: string; name: string }
  tools: 'dynamic'
}

export function buildServerCard(mcpEndpoint: string): McpServerCard {
  return {
    $schema: 'https://modelcontextprotocol.io/schemas/server-card/1.0.json',
    version: '1.0',
    protocolVersion: PROTOCOL_VERSION,
    serverInfo: { name: SERVER_NAME, version: SERVER_VERSION },
    transport: { type: 'http', endpoint: mcpEndpoint },
    capabilities: { tools: {} },
    description: 'Bridges local DMED devices (BLE, mDNS, Ethernet) to MCP tools.',
    authentication: { type: 'apiKey', in: 'header', name: 'X-API-Key' },
    tools: 'dynamic'
  }
}
