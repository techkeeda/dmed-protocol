import { describe, expect, it, vi } from 'vitest'
import { handleLine } from '../src/bridge.js'

function fakeFetch(status: number, body: string) {
  return vi.fn().mockResolvedValue({
    status,
    text: async () => body
  }) as unknown as typeof fetch
}

const opts = { gatewayUrl: 'http://localhost:4100', apiKey: 'test-key' }

describe('handleLine', () => {
  it('returns undefined for a blank line', async () => {
    const fetchImpl = fakeFetch(200, '{}')
    expect(await handleLine('   ', { ...opts, fetchImpl })).toBeUndefined()
    expect(fetchImpl).not.toHaveBeenCalled()
  })

  it('returns undefined for unparseable JSON with nothing to correlate a reply to', async () => {
    const fetchImpl = fakeFetch(200, '{}')
    expect(await handleLine('not json', { ...opts, fetchImpl })).toBeUndefined()
    expect(fetchImpl).not.toHaveBeenCalled()
  })

  it('forwards a JSON-RPC request to the gateway with the API key header', async () => {
    const fetchImpl = fakeFetch(200, '{"jsonrpc":"2.0","id":1,"result":{}}')
    await handleLine('{"jsonrpc":"2.0","id":1,"method":"tools/list"}', { ...opts, fetchImpl })
    expect(fetchImpl).toHaveBeenCalledWith(
      'http://localhost:4100/mcp',
      expect.objectContaining({
        method: 'POST',
        headers: expect.objectContaining({ 'X-API-Key': 'test-key' }),
        body: '{"jsonrpc":"2.0","id":1,"method":"tools/list"}'
      })
    )
  })

  it('returns the gateway response body as the output line', async () => {
    const fetchImpl = fakeFetch(200, '{"jsonrpc":"2.0","id":1,"result":{"tools":[]}}')
    const result = await handleLine('{"jsonrpc":"2.0","id":1,"method":"tools/list"}', { ...opts, fetchImpl })
    expect(result).toBe('{"jsonrpc":"2.0","id":1,"result":{"tools":[]}}')
  })

  it('returns undefined when the gateway responds 204 (a notification)', async () => {
    const fetchImpl = fakeFetch(204, '')
    const result = await handleLine('{"jsonrpc":"2.0","method":"notifications/initialized"}', { ...opts, fetchImpl })
    expect(result).toBeUndefined()
  })

  it('returns a JSON-RPC error with the original id when the gateway is unreachable', async () => {
    const fetchImpl = vi.fn().mockRejectedValue(new Error('ECONNREFUSED')) as unknown as typeof fetch
    const result = await handleLine('{"jsonrpc":"2.0","id":42,"method":"tools/list"}', { ...opts, fetchImpl })
    expect(JSON.parse(result ?? '{}')).toMatchObject({
      jsonrpc: '2.0',
      id: 42,
      error: { message: expect.stringContaining('ECONNREFUSED') }
    })
  })

  it('swallows an unreachable gateway for a notification (no id to reply to)', async () => {
    const fetchImpl = vi.fn().mockRejectedValue(new Error('ECONNREFUSED')) as unknown as typeof fetch
    const result = await handleLine('{"jsonrpc":"2.0","method":"notifications/initialized"}', { ...opts, fetchImpl })
    expect(result).toBeUndefined()
  })
})
