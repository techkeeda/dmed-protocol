import type { DmedCard, DmedDevice, TransportEntry, ActionResponse } from './types.js'

export class NoTransportError extends Error {}
export class DispatchError extends Error {}

export function selectTransport(card: DmedCard): TransportEntry {
  const http = (card.transports ?? [])
    .filter(t => t.type === 'http' && t.url)
    .sort((a, b) => a.priority - b.priority)[0]
  if (!http) throw new NoTransportError(`No HTTP transport in card for ${card.name}`)
  return http
}

export async function fetchCard(host: string, port: number): Promise<DmedCard> {
  const res = await fetch(`http://${host}:${port}/dmed/card`)
  if (!res.ok) throw new DispatchError(`GET /dmed/card failed: ${res.status}`)
  return res.json()
}

export async function dispatchAction(
  device: DmedDevice,
  action: string,
  params: Record<string, unknown> = {}
): Promise<ActionResponse> {
  if (!device.httpHost || !device.httpPort) {
    throw new DispatchError('Device has no HTTP transport')
  }
  const res = await fetch(`http://${device.httpHost}:${device.httpPort}/dmed/action`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action, params })
  })
  if (!res.ok) throw new DispatchError(`POST /dmed/action failed: ${res.status}`)
  return res.json()
}
