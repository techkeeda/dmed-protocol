import type { Server } from 'http'
import { fileURLToPath } from 'url'
import { dirname, join } from 'path'
import { afterAll, beforeAll, describe, expect, it } from 'vitest'
import request from 'supertest'
import { fetchCard } from '@dmed/discovery'
import { createRegistry, registerDevice } from '../src/tool-mapper.js'
import { createServer } from '../src/server.js'
import type { GatewayConfig, ToolRegistry } from '../src/types.js'

const COFFEE_MACHINE_DIR = join(dirname(fileURLToPath(import.meta.url)), '../../examples/smart-coffee-machine')

// Runs the real smart-coffee-machine Express app in-process (rather than requiring a
// separately-running `npm start`) and registers it directly, bypassing mDNS multicast —
// which is not reliable inside sandboxed/CI environments. Everything downstream of
// discovery (card fetch, tool mapping, dispatch) exercises the real live HTTP server.
describe('Gateway integration against a live coffee machine server', () => {
  const config: GatewayConfig = { apiKey: 'test-key' }
  let coffeeServer: Server
  let coffeePort: number
  let registry: ToolRegistry

  beforeAll(async () => {
    // server.js reads its manifest via a cwd-relative path, so we must chdir into its
    // own directory before importing it, then dynamically import (static imports would
    // already have resolved before this hook runs).
    const originalCwd = process.cwd()
    process.chdir(COFFEE_MACHINE_DIR)
    const { createApp } = await import('../../examples/smart-coffee-machine/server.js')
    process.chdir(originalCwd)

    const { app } = createApp()
    coffeeServer = await new Promise<Server>(resolve => {
      const server = app.listen(0, () => resolve(server))
    })
    const address = coffeeServer.address()
    if (typeof address !== 'object' || address === null) throw new Error('Failed to bind coffee machine server')
    coffeePort = address.port

    const card = await fetchCard('127.0.0.1', coffeePort)
    registry = createRegistry()
    registerDevice(registry, {
      instanceId: card.instance_id,
      name: card.name,
      discoveredVia: 'mdns',
      address: `127.0.0.1:${coffeePort}`,
      httpHost: '127.0.0.1',
      httpPort: coffeePort,
      card
    })
  })

  afterAll(() => {
    coffeeServer.close()
  })

  function mcp(body: Record<string, unknown>) {
    return request(createServer(registry, config)).post('/mcp').set('X-API-Key', config.apiKey).send(body)
  }

  it('initialize returns server name "DMED Gateway"', async () => {
    const res = await mcp({ jsonrpc: '2.0', id: 1, method: 'initialize' })
    expect(res.body.result.serverInfo.name).toBe('DMED Gateway')
  })

  it('tools/list includes coffee_machine__brew_coffee and coffee_machine__get_status', async () => {
    const res = await mcp({ jsonrpc: '2.0', id: 2, method: 'tools/list' })
    const names = res.body.result.tools.map((t: { name: string }) => t.name)
    expect(names).toContain('smart_coffee_machine__brew_coffee')
    expect(names).toContain('smart_coffee_machine__get_status')
  })

  it('tools/call brew_coffee dispatches to the live server and returns a result', async () => {
    const res = await mcp({
      jsonrpc: '2.0',
      id: 3,
      method: 'tools/call',
      params: { name: 'smart_coffee_machine__brew_coffee', arguments: { drink_type: 'latte', size: 'large' } }
    })
    expect(res.body.result.isError).toBe(false)
    expect(res.body.result.content[0].text).toMatch(/latte/i)
  })

  it('tools/call get_status returns machine state', async () => {
    const res = await mcp({
      jsonrpc: '2.0',
      id: 4,
      method: 'tools/call',
      params: { name: 'smart_coffee_machine__get_status', arguments: {} }
    })
    expect(res.body.result.isError).toBe(false)
    const state = JSON.parse(res.body.result.content[0].text)
    expect(state).toHaveProperty('water_level')
  })

  it('rejects requests without a valid API key', async () => {
    const res = await request(createServer(registry, config)).post('/mcp').send({ jsonrpc: '2.0', id: 5, method: 'tools/list' })
    expect(res.status).toBe(401)
  })
})
