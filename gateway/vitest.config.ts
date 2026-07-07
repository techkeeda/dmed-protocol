import { defineConfig } from 'vitest/config'

export default defineConfig({
  test: {
    environment: 'node',
    // gateway.test.ts needs process.chdir() to import the coffee-machine example
    // (which reads its manifest via a cwd-relative path) — unsupported in worker threads.
    pool: 'forks'
  }
})
