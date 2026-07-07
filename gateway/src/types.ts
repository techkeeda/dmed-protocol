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

export interface GatewayConfig {
  apiKey: string
  port?: number
}
