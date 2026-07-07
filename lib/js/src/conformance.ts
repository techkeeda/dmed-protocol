import { CardValidationError, parseCard } from './card.js'
import type { DmedCard } from './types.js'

export interface ConformanceCheck {
  name: string
  passed: boolean
  required: boolean
  detail?: string
}

export interface ConformanceReport {
  url: string
  checks: ConformanceCheck[]
  passed: boolean
}

export interface ConformanceOptions {
  callAction?: string
}

async function postAction(base: string, body: unknown): Promise<Response> {
  return fetch(`${base}/dmed/action`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body)
  })
}

export async function runConformance(baseUrl: string, opts: ConformanceOptions = {}): Promise<ConformanceReport> {
  const base = baseUrl.replace(/\/+$/, '')
  const checks: ConformanceCheck[] = []
  let card: DmedCard | undefined

  const cardRes = await fetch(`${base}/dmed/card`)
  const cardBody = await cardRes.text()
  checks.push({ name: 'GET /dmed/card returns 200', passed: cardRes.status === 200, required: true })

  if (cardRes.status === 200) {
    try {
      card = parseCard(cardBody)
      checks.push({ name: '/dmed/card response is a valid DMED card', passed: true, required: true })
    } catch (err) {
      checks.push({
        name: '/dmed/card response is a valid DMED card',
        passed: false,
        required: true,
        detail: err instanceof CardValidationError ? err.message : String(err)
      })
    }
  }

  if (!card) {
    return { url: base, checks, passed: false }
  }

  const toolCount = card.capabilities.tools.length
  checks.push({
    name: 'capabilities.tools is a non-empty array',
    passed: toolCount > 0,
    required: false,
    detail: toolCount === 0 ? 'Card declares zero tools' : undefined
  })

  const toolsWellFormed = card.capabilities.tools.every(
    t => typeof t.name === 'string' && typeof t.description === 'string'
  )
  checks.push({ name: 'every tool has a name and a description', passed: toolsWellFormed, required: true })

  if (card.transports) {
    const transportsWellFormed = card.transports.every(
      t =>
        typeof t.type === 'string' &&
        typeof t.priority === 'number' &&
        (t.type !== 'http' || typeof t.url === 'string')
    )
    checks.push({ name: 'transports entries are well-formed', passed: transportsWellFormed, required: true })
  }

  const actionsRes = await fetch(`${base}/dmed/actions`)
  checks.push({ name: 'GET /dmed/actions returns 200', passed: actionsRes.status === 200, required: true })

  if (actionsRes.status === 200) {
    try {
      const parsed = JSON.parse(await actionsRes.text())
      const actionCount = Array.isArray(parsed.actions) ? parsed.actions.length : -1
      checks.push({
        name: '/dmed/actions action count matches capabilities.tools',
        passed: actionCount === toolCount,
        required: true,
        detail: actionCount !== toolCount ? `actions: ${actionCount}, tools: ${toolCount}` : undefined
      })
    } catch {
      checks.push({
        name: '/dmed/actions action count matches capabilities.tools',
        passed: false,
        required: true,
        detail: 'Response was not valid JSON with an "actions" array'
      })
    }
  }

  const missingActionRes = await postAction(base, {})
  checks.push({
    name: 'POST /dmed/action with a missing "action" field returns 400',
    passed: missingActionRes.status === 400,
    required: true
  })

  const unknownActionRes = await postAction(base, { action: '__dmed_conform_unknown_action__' })
  checks.push({
    name: 'POST /dmed/action with an unknown action returns 404',
    passed: unknownActionRes.status === 404,
    required: true
  })

  if (opts.callAction) {
    const callRes = await postAction(base, { action: opts.callAction, params: {} })
    let statusField: unknown
    try {
      statusField = JSON.parse(await callRes.text()).status
    } catch {
      statusField = undefined
    }
    checks.push({
      name: `POST /dmed/action { action: "${opts.callAction}" } returns 200 with a status field`,
      passed: callRes.status === 200 && (statusField === 'ok' || statusField === 'error'),
      required: false
    })
  }

  return { url: base, checks, passed: checks.filter(c => c.required).every(c => c.passed) }
}
