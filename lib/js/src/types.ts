export interface TransportEntry {
  type: 'http' | 'ble' | string
  url?: string
  service_uuid?: string
  priority: number
}

export interface Tool {
  name: string
  description: string
  inputSchema?: Record<string, unknown>
}

export interface DmedCard {
  dmed_version: string
  instance_id: string
  name: string
  description: string
  service_type: string
  transports?: TransportEntry[]
  auth?: { type: string }
  capabilities: { tools: Tool[] }
  metadata?: Record<string, string>
}

export interface DmedDevice {
  instanceId: string
  name: string
  discoveredVia: 'ble' | 'mdns'
  address: string
  httpHost?: string
  httpPort?: number
  card?: DmedCard
}

export interface ActionResponse {
  status: 'ok' | 'error'
  action: string
  result?: { text: string }
  message?: string
}

export type DeviceEventHandler = (device: DmedDevice) => void
