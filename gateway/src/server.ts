import express, { type Express, type Request, type Response } from 'express'
import { apiKeyAuth } from './auth.js'
import { DeviceNotFoundError, ToolNotFoundError, dispatchToolCall, resolveToolCall } from './proxy.js'
import type { GatewayConfig, ToolRegistry } from './types.js'

export const SERVER_NAME = 'DMED Gateway'
export const SERVER_VERSION = '0.1.0'

interface JsonRpcRequest {
  jsonrpc?: string
  id?: unknown
  method?: string
  params?: { name?: string; arguments?: Record<string, unknown> }
}

export function createServer(registry: ToolRegistry, config: GatewayConfig): Express {
  const app = express()
  app.use(express.json())
  app.use(apiKeyAuth(config))

  app.post('/mcp', async (req: Request, res: Response) => {
    const { id, method, params } = (req.body ?? {}) as JsonRpcRequest

    if (method === 'initialize') {
      res.json({
        jsonrpc: '2.0',
        id,
        result: {
          protocolVersion: '2025-03-26',
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
