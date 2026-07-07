import type { DmedDevice } from '@dmed/discovery'
import type { McpTool, ToolRegistry } from './types.js'

const MAX_SLUG_LENGTH = 32

export function slugify(name: string): string {
  return name
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .slice(0, MAX_SLUG_LENGTH)
}

export function toolsFromCard(device: DmedDevice): McpTool[] {
  const card = device.card
  if (!card) return []
  const slug = slugify(device.name)
  return card.capabilities.tools.map(tool => ({
    name: `${slug}__${tool.name}`,
    description: `${tool.description} (${device.name})`,
    inputSchema: tool.inputSchema ?? { type: 'object', properties: {} }
  }))
}

export function createRegistry(): ToolRegistry {
  return { tools: new Map(), devices: new Map() }
}

export function registerDevice(registry: ToolRegistry, device: DmedDevice): void {
  const slug = slugify(device.name)
  registry.devices.set(slug, device)
  for (const tool of toolsFromCard(device)) {
    const action = tool.name.slice(slug.length + 2)
    registry.tools.set(tool.name, { tool, deviceSlug: slug, action })
  }
}

export function unregisterDevice(registry: ToolRegistry, device: DmedDevice): void {
  const slug = slugify(device.name)
  registry.devices.delete(slug)
  const prefix = `${slug}__`
  for (const name of registry.tools.keys()) {
    if (name.startsWith(prefix)) registry.tools.delete(name)
  }
}
