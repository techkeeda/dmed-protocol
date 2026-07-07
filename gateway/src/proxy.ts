import { dispatchAction } from '@dmed/discovery'
import type { ActionResponse, DmedDevice } from '@dmed/discovery'
import type { ToolRegistry } from './types.js'

export class ToolNotFoundError extends Error {}
export class DeviceNotFoundError extends Error {}

export function resolveToolCall(
  toolName: string,
  registry: ToolRegistry
): { device: DmedDevice; action: string } {
  const entry = registry.tools.get(toolName)
  if (!entry) throw new ToolNotFoundError(`Unknown tool: ${toolName}`)
  const device = registry.devices.get(entry.deviceSlug)
  if (!device) throw new DeviceNotFoundError(`Device offline for tool: ${toolName}`)
  return { device, action: entry.action }
}

export interface DispatchResult {
  isError: boolean
  text: string
}

export async function dispatchToolCall(
  device: DmedDevice,
  action: string,
  params: Record<string, unknown> = {},
  dispatch: typeof dispatchAction = dispatchAction
): Promise<DispatchResult> {
  let resp: ActionResponse
  try {
    resp = await dispatch(device, action, params)
  } catch (err) {
    return { isError: true, text: err instanceof Error ? err.message : 'Device unreachable' }
  }
  if (resp.status === 'ok') {
    return { isError: false, text: resp.result?.text ?? 'Done' }
  }
  return { isError: true, text: resp.message ?? 'Unknown error' }
}
