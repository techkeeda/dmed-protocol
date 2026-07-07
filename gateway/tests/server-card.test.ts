import { describe, expect, it } from 'vitest'
import request from 'supertest'
import { createServer } from '../src/server.js'
import { createRegistry } from '../src/tool-mapper.js'
import type { GatewayConfig } from '../src/types.js'

describe('GET /.well-known/mcp/server-card.json', () => {
  const config: GatewayConfig = { apiKey: 'secret-key' }

  it('is reachable without an API key', async () => {
    const res = await request(createServer(createRegistry(), config)).get('/.well-known/mcp/server-card.json')
    expect(res.status).toBe(200)
  })

  it('includes the required SEP-1649 fields', async () => {
    const res = await request(createServer(createRegistry(), config)).get('/.well-known/mcp/server-card.json')
    expect(res.body).toMatchObject({
      $schema: expect.any(String),
      version: expect.any(String),
      protocolVersion: expect.any(String),
      serverInfo: { name: 'DMED Gateway', version: expect.any(String) },
      transport: { type: 'http', endpoint: expect.stringContaining('/mcp') },
      capabilities: { tools: {} }
    })
  })

  it('never leaks the configured API key', async () => {
    const res = await request(createServer(createRegistry(), config)).get('/.well-known/mcp/server-card.json')
    expect(JSON.stringify(res.body)).not.toContain(config.apiKey)
  })

  it('builds the transport endpoint from the request host', async () => {
    const res = await request(createServer(createRegistry(), config))
      .get('/.well-known/mcp/server-card.json')
      .set('Host', 'gateway.example.com')
    expect(res.body.transport.endpoint).toBe('http://gateway.example.com/mcp')
  })
})
