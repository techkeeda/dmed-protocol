import { existsSync, readFileSync } from 'fs'
import type { NextFunction, Request, Response } from 'express'
import type { GatewayConfig } from './types.js'

export class ConfigError extends Error {}

export function loadConfig(path: string): GatewayConfig {
  if (!existsSync(path)) {
    throw new ConfigError(
      `Config file not found: ${path}. Copy config.json.example to config.json and set an apiKey.`
    )
  }
  const parsed = JSON.parse(readFileSync(path, 'utf-8'))
  if (!parsed.apiKey || typeof parsed.apiKey !== 'string') {
    throw new ConfigError(`Config file ${path} must define a string "apiKey"`)
  }
  return parsed as GatewayConfig
}

export function apiKeyAuth(config: GatewayConfig) {
  return (req: Request, res: Response, next: NextFunction): void => {
    const key = req.header('X-API-Key')
    if (key !== config.apiKey) {
      res.status(401).json({ error: 'Unauthorized' })
      return
    }
    next()
  }
}
