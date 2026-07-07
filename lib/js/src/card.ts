import type { DmedCard } from './types.js'

export class CardValidationError extends Error {}

const REQUIRED_FIELDS = ['dmed_version', 'instance_id', 'name', 'capabilities'] as const

export function parseCard(json: string): DmedCard {
  let data: unknown
  try {
    data = JSON.parse(json)
  } catch {
    throw new CardValidationError('Invalid JSON')
  }
  return validateCard(data)
}

export function validateCard(data: unknown): DmedCard {
  if (typeof data !== 'object' || data === null) {
    throw new CardValidationError('Card must be a JSON object')
  }
  const obj = data as Record<string, unknown>
  for (const field of REQUIRED_FIELDS) {
    if (!(field in obj)) {
      throw new CardValidationError(`Missing required field: ${field}`)
    }
  }
  if (!Array.isArray((obj.capabilities as any)?.tools)) {
    throw new CardValidationError('capabilities.tools must be an array')
  }
  return data as DmedCard
}
