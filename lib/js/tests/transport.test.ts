import { describe, it, expect } from 'vitest'
import { selectTransport, NoTransportError } from '../src/transport.js'
import type { DmedCard } from '../src/types.js'

function makeCard(transports: DmedCard['transports']): DmedCard {
  return {
    dmed_version: '0.2.0',
    instance_id: 'test01',
    name: 'Test Device',
    description: '',
    service_type: 'iot_device',
    transports,
    auth: { type: 'none' },
    capabilities: { tools: [] }
  }
}

describe('selectTransport', () => {
  it('selects http transport when available', () => {
    const card = makeCard([
      { type: 'http', url: 'http://192.168.1.1:3100/mcp', priority: 1 }
    ])
    const t = selectTransport(card)
    expect(t.type).toBe('http')
    expect(t.url).toContain('192.168.1.1')
  })

  it('selects lowest priority number first', () => {
    const card = makeCard([
      { type: 'http', url: 'http://fallback:8080/mcp', priority: 2 },
      { type: 'http', url: 'http://preferred:3100/mcp', priority: 1 }
    ])
    const t = selectTransport(card)
    expect(t.url).toContain('preferred')
  })

  it('throws NoTransportError when no http transport', () => {
    const card = makeCard([
      { type: 'ble', service_uuid: 'D14D0001-...', priority: 1 }
    ])
    expect(() => selectTransport(card)).toThrow(NoTransportError)
  })

  it('throws NoTransportError when transports list is empty', () => {
    const card = makeCard([])
    expect(() => selectTransport(card)).toThrow(NoTransportError)
  })

  it('ignores http entry without url', () => {
    const card = makeCard([
      { type: 'http', priority: 1 }
    ])
    expect(() => selectTransport(card)).toThrow(NoTransportError)
  })

  it('throws NoTransportError (not a raw TypeError) when transports is omitted entirely', () => {
    const { transports, ...rest } = makeCard([])
    const card = rest as DmedCard
    expect(() => selectTransport(card)).toThrow(NoTransportError)
  })
})
