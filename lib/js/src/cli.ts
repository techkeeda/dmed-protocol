#!/usr/bin/env node
import { runConformance } from './conformance.js'

interface ParsedArgs {
  url?: string
  json: boolean
  callAction?: string
}

export function parseArgs(argv: string[]): ParsedArgs {
  const json = argv.includes('--json')
  const callIdx = argv.indexOf('--call')
  const callAction = callIdx !== -1 ? argv[callIdx + 1] : undefined
  const url = argv.find((arg, i) => !arg.startsWith('--') && argv[i - 1] !== '--call')
  return { url, json, callAction }
}

export function formatReport(report: Awaited<ReturnType<typeof runConformance>>): string {
  const lines = [`DMED Conformance Report — ${report.url}`, '']
  for (const check of report.checks) {
    const mark = check.passed ? '✓' : check.required ? '✗' : '⚠'
    lines.push(`${mark} ${check.name}${check.detail ? ` — ${check.detail}` : ''}`)
  }
  lines.push('', report.passed ? 'PASS' : 'FAIL')
  return lines.join('\n')
}

async function main(): Promise<void> {
  const { url, json, callAction } = parseArgs(process.argv.slice(2))
  if (!url) {
    console.error('Usage: dmed-conform <url> [--call <action>] [--json]')
    process.exit(2)
  }

  const report = await runConformance(url, { callAction })
  console.log(json ? JSON.stringify(report, null, 2) : formatReport(report))
  process.exit(report.passed ? 0 : 1)
}

main().catch(err => {
  console.error(`dmed-conform error: ${err instanceof Error ? err.message : err}`)
  process.exit(2)
})
