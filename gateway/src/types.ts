import type { DmedDevice } from '@dmed/discovery'

export interface McpTool {
  name: string
  description: string
  inputSchema: Record<string, unknown>
}

export interface ToolRegistryEntry {
  tool: McpTool
  deviceSlug: string
  action: string
}

export interface ToolRegistry {
  tools: Map<string, ToolRegistryEntry>
  devices: Map<string, DmedDevice>
}

export interface TlsConfig {
  cert: string
  key: string
}

export interface AllowlistConfig {
  devices?: string[]
  actions?: Record<string, string[]>
}

export interface GatewayConfig {
  apiKey: string
  port?: number
  tls?: TlsConfig
  allowlist?: AllowlistConfig
}
