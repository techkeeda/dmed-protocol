import express, { type Express, type Request, type Response } from 'express'
import { apiKeyAuth } from './auth.js'
import { PROTOCOL_VERSION, SERVER_NAME, SERVER_VERSION } from './constants.js'
import { DeviceNotFoundError, ToolNotFoundError, dispatchToolCall, resolveToolCall } from './proxy.js'
import { buildServerCard } from './server-card.js'
import type { GatewayConfig, ToolRegistry } from './types.js'

interface JsonRpcRequest {
  jsonrpc?: string
  id?: unknown
  method?: string
  params?: { name?: string; arguments?: Record<string, unknown> }
}

export function createServer(registry: ToolRegistry, config: GatewayConfig): Express {
  const app = express()
  app.use(express.json())

  // Unauthenticated by design (SEP-1649): discovery happens before a client has
  // credentials, and the spec explicitly forbids putting secrets in this document.
  app.get('/.well-known/mcp/server-card.json', (req: Request, res: Response) => {
    const endpoint = `${req.protocol}://${req.get('host')}/mcp`
    res.json(buildServerCard(endpoint))
  })

  app.use(apiKeyAuth(config))

  app.post('/mcp', async (req: Request, res: Response) => {
    const { id, method, params } = (req.body ?? {}) as JsonRpcRequest

    if (method === 'initialize') {
      res.json({
        jsonrpc: '2.0',
        id,
        result: {
          protocolVersion: PROTOCOL_VERSION,
          capabilities: { tools: {} },
          serverInfo: { name: SERVER_NAME, version: SERVER_VERSION }
        }
      })
      return
    }

    if (method === 'notifications/initialized') {
      res.status(204).end()
      return
    }

    if (method === 'tools/list') {
      res.json({
        jsonrpc: '2.0',
        id,
        result: { tools: Array.from(registry.tools.values()).map(entry => entry.tool) }
      })
      return
    }

    if (method === 'tools/call') {
      const toolName = params?.name ?? ''
      try {
        const { device, action } = resolveToolCall(toolName, registry)
        const result = await dispatchToolCall(device, action, params?.arguments ?? {})
        res.json({
          jsonrpc: '2.0',
          id,
          result: { content: [{ type: 'text', text: result.text }], isError: result.isError }
        })
      } catch (err) {
        if (err instanceof ToolNotFoundError || err instanceof DeviceNotFoundError) {
          res.json({ jsonrpc: '2.0', id, error: { code: -32602, message: err.message } })
        } else {
          res.json({
            jsonrpc: '2.0',
            id,
            error: { code: -32603, message: err instanceof Error ? err.message : 'Internal error' }
          })
        }
      }
      return
    }

    res.json({ jsonrpc: '2.0', id, error: { code: -32601, message: `Unknown method: ${method}` } })
  })

  return app
}
