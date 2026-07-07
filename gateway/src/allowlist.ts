import type { AllowlistConfig } from './types.js'

export function isDeviceAllowed(allowlist: AllowlistConfig | undefined, deviceSlug: string): boolean {
  if (!allowlist?.devices) return true
  return allowlist.devices.includes(deviceSlug)
}

export function isActionAllowed(allowlist: AllowlistConfig | undefined, deviceSlug: string, action: string): boolean {
  if (!isDeviceAllowed(allowlist, deviceSlug)) return false
  const actions = allowlist?.actions?.[deviceSlug]
  if (!actions) return true
  return actions.includes(action)
}
