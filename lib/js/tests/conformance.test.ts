import { createServer } from 'http'
import type { Server } from 'http'
import { afterAll, beforeAll, describe, expect, it } from 'vitest'
import { runConformance } from '../src/conformance.js'

const GOOD_CARD = {
  dmed_version: '0.2.0',
  instance_id: 'c0ffee01',
  name: 'Fixture Device',
  description: 'A fixture DMED device',
  service_type: 'iot_device',
  transports: [{ type: 'http', url: 'http://localhost:9901/mcp', priority: 1 }],
  auth: { type: 'none' },
  capabilities: {
    tools: [
      { name: 'do_thing', description: 'Does a thing' },
      { name: 'get_status', description: 'Gets status' }
    ]
  }
}

function readBody(req: import('http').IncomingMessage): Promise<string> {
  return new Promise(resolve => {
    let data = ''
    req.on('data', chunk => (data += chunk))
    req.on('end', () => resolve(data))
  })
}

function startFixtureServer(handler: (req: import('http').IncomingMessage, res: import('http').ServerResponse) => Promise<void>): Promise<{ server: Server; port: number }> {
  return new Promise(resolve => {
    const server = createServer((req, res) => {
      handler(req, res).catch(() => {
        res.statusCode = 500
        res.end()
      })
    })
    server.listen(0, () => {
      const address = server.address()
      const port = typeof address === 'object' && address ? address.port : 0
      resolve({ server, port })
    })
  })
}

function json(res: import('http').ServerResponse, status: number, body: unknown): void {
  res.statusCode = status
  res.setHeader('Content-Type', 'application/json')
  res.end(JSON.stringify(body))
}

describe('runConformance against a well-formed device', () => {
  let server: Server
  let baseUrl: string

  beforeAll(async () => {
    const started = await startFixtureServer(async (req, res) => {
      if (req.method === 'GET' && req.url === '/dmed/card') {
        return json(res, 200, GOOD_CARD)
      }
      if (req.method === 'GET' && req.url === '/dmed/actions') {
        return json(res, 200, { actions: GOOD_CARD.capabilities.tools })
      }
      if (req.method === 'POST' && req.url === '/dmed/action') {
        const body = JSON.parse((await readBody(req)) || '{}')
        if (!body.action) return json(res, 400, { status: 'error', message: 'Missing action' })
        const known = GOOD_CARD.capabilities.tools.some(t => t.name === body.action)
        if (!known) return json(res, 404, { status: 'error', message: 'Unknown action' })
        return json(res, 200, { status: 'ok', action: body.action, result: { text: 'done' } })
      }
      res.statusCode = 404
      res.end()
    })
    server = started.server
    baseUrl = `http://localhost:${started.port}`
  })

  afterAll(() => {
    server.close()
  })

  it('passes all required checks', async () => {
    const report = await runConformance(baseUrl)
    expect(report.passed).toBe(true)
    expect(report.checks.filter(c => c.required && !c.passed)).toEqual([])
  })

  it('reports capabilities.tools non-empty as passed', async () => {
    const report = await runConformance(baseUrl)
    const check = report.checks.find(c => c.name.includes('non-empty array'))
    expect(check?.passed).toBe(true)
  })

  it('with --call runs a live dispatch and reports it', async () => {
    const report = await runConformance(baseUrl, { callAction: 'get_status' })
    const check = report.checks.find(c => c.name.includes('returns 200 with a status field'))
    expect(check?.passed).toBe(true)
  })
})

describe('runConformance against a broken device', () => {
  let server: Server
  let baseUrl: string

  beforeAll(async () => {
    const started = await startFixtureServer(async (req, res) => {
      if (req.method === 'GET' && req.url === '/dmed/card') {
        // Missing instance_id — fails card validation
        const broken = { ...GOOD_CARD } as Record<string, unknown>
        delete broken.instance_id
        return json(res, 200, broken)
      }
      res.statusCode = 404
      res.end()
    })
    server = started.server
    baseUrl = `http://localhost:${started.port}`
  })

  afterAll(() => {
    server.close()
  })

  it('fails overall and reports the specific validation failure', async () => {
    const report = await runConformance(baseUrl)
    expect(report.passed).toBe(false)
    const check = report.checks.find(c => c.name.includes('valid DMED card'))
    expect(check?.passed).toBe(false)
    expect(check?.detail).toMatch(/instance_id/)
  })
})

describe('runConformance against a device with a mismatched action count', () => {
  let server: Server
  let baseUrl: string

  beforeAll(async () => {
    const started = await startFixtureServer(async (req, res) => {
      if (req.method === 'GET' && req.url === '/dmed/card') {
        return json(res, 200, GOOD_CARD)
      }
      if (req.method === 'GET' && req.url === '/dmed/actions') {
        return json(res, 200, { actions: [GOOD_CARD.capabilities.tools[0]] }) // only 1 of 2
      }
      if (req.method === 'POST' && req.url === '/dmed/action') {
        const body = JSON.parse((await readBody(req)) || '{}')
        if (!body.action) return json(res, 400, {})
        return json(res, 404, {})
      }
      res.statusCode = 404
      res.end()
    })
    server = started.server
    baseUrl = `http://localhost:${started.port}`
  })

  afterAll(() => {
    server.close()
  })

  it('flags the action-count mismatch and fails overall', async () => {
    const report = await runConformance(baseUrl)
    expect(report.passed).toBe(false)
    const check = report.checks.find(c => c.name.includes('action count matches'))
    expect(check?.passed).toBe(false)
    expect(check?.detail).toBe('actions: 1, tools: 2')
  })
})
