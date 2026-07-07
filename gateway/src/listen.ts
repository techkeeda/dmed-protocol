import { readFileSync } from 'fs'
import { createServer as createHttpsServer } from 'https'
import type { Express } from 'express'
import type { Server } from 'net'
import type { GatewayConfig } from './types.js'

export function startListening(
  app: Express,
  config: GatewayConfig,
  port: number,
  onListening: () => void,
  httpsFactory: typeof createHttpsServer = createHttpsServer
): Server {
  if (config.tls) {
    const options = { cert: readFileSync(config.tls.cert), key: readFileSync(config.tls.key) }
    return httpsFactory(options, app).listen(port, onListening) as unknown as Server
  }
  return app.listen(port, onListening)
}
