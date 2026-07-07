export interface BridgeOptions {
  gatewayUrl: string
  apiKey: string
  fetchImpl?: typeof fetch
}

interface JsonRpcMessage {
  jsonrpc?: string
  id?: unknown
  method?: string
  params?: unknown
}

// Claude Desktop speaks MCP over stdio as newline-delimited JSON-RPC. This takes one such
// line, forwards it to the gateway's HTTP endpoint, and returns the line to write back to
// stdout (or undefined when nothing should be written — a notification, a blank input
// line, or a 204 from the gateway).
export async function handleLine(rawLine: string, opts: BridgeOptions): Promise<string | undefined> {
  const trimmed = rawLine.trim()
  if (!trimmed) return undefined

  let message: JsonRpcMessage
  try {
    message = JSON.parse(trimmed)
  } catch {
    // Not a JSON-RPC message at all, and with no id there's nothing to correlate a reply to.
    return undefined
  }

  const fetchImpl = opts.fetchImpl ?? fetch
  try {
    const res = await fetchImpl(`${opts.gatewayUrl}/mcp`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', 'X-API-Key': opts.apiKey },
      body: JSON.stringify(message)
    })
    if (res.status === 204) return undefined
    return await res.text()
  } catch (err) {
    if (message.id === undefined) return undefined
    return JSON.stringify({
      jsonrpc: '2.0',
      id: message.id,
      error: { code: -32000, message: `Gateway unreachable: ${err instanceof Error ? err.message : String(err)}` }
    })
  }
}
