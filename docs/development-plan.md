# DMED Detailed Development Plan

---

## How to Read This

Each task has an ID (`P0.1`, `P1.2.3`), lists exact files to touch, exact changes to make, and exact tests to write. Work top-to-bottom within each phase — tasks inside a phase are ordered by dependency.

---

# Phase 0 — Foundation

**Goal:** Clean repo, all test runners wired, nothing blocked.

---

## P0.1 — Delete IDE artifact

**File:** `voip-phone-dmed-session.json` (project root)
**Change:** Delete it. It is a Kiro IDE session export, not project content.

```bash
git rm voip-phone-dmed-session.json
```

No tests needed.

---

## P0.2 — Commit all untracked work

Stage and commit the five untracked directories in a single commit:

```bash
git add \
  examples/smart-coffee-machine/ \
  examples/voip-phone-c/ \
  examples/bleclient-dmed/ \
  examples/bleclient-dmed-working/ \
  android/
git commit -m "chore: add untracked examples and android app"
```

No tests needed.

---

## P0.3 — Coffee machine: add test runner

**Goal:** `npm test` works in `examples/smart-coffee-machine/`.

### P0.3.1 — Refactor server.js for testability

The Express app and MCP stdio transport must be separable so tests can import the app without starting stdio.

**File:** `examples/smart-coffee-machine/server.js`

Extract the Express app into a named export. The file currently ends with `await server.connect(transport)` — guard that behind a main-module check.

**Changes:**
1. Move all Express route definitions into a function `createApp(machineState)` that returns `{app, state}`.
2. Export `createApp`.
3. Wrap the `app.listen(...)`, `bonjour.publish(...)`, and `await server.connect(transport)` block in:
   ```js
   // Only run when executed directly, not when imported by tests
   if (process.argv[1] === fileURLToPath(import.meta.url)) {
     // ... existing startup code ...
   }
   ```
4. Add `import { fileURLToPath } from 'url'` at the top.

### P0.3.2 — Add vitest and supertest

**File:** `examples/smart-coffee-machine/package.json`

Add to `devDependencies` and `scripts`:

```json
{
  "scripts": {
    "start": "node server.js",
    "test": "vitest run",
    "test:watch": "vitest"
  },
  "devDependencies": {
    "vitest": "^1.6.0",
    "supertest": "^7.0.0"
  }
}
```

### P0.3.3 — Create placeholder test file

**New file:** `examples/smart-coffee-machine/server.test.js`

```js
import { describe, it, expect } from 'vitest'
import request from 'supertest'
import { createApp } from './server.js'

describe('placeholder', () => {
  it('test runner works', () => {
    expect(true).toBe(true)
  })
})
```

**Verify:** `cd examples/smart-coffee-machine && npm install && npm test` — passes.

---

## P0.4 — Android: add test infrastructure

**Goal:** `./gradlew test` works in `android/dmed-scanner-working/`.

### P0.4.1 — Add test dependencies to build.gradle.kts

**File:** `android/dmed-scanner-working/app/build.gradle.kts`

Add to `dependencies` block:

```kotlin
// Test dependencies
testImplementation("junit:junit:4.13.2")
testImplementation("org.mockito.kotlin:mockito-kotlin:5.2.1")
testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.7.3")
testImplementation("com.google.code.gson:gson:2.10.1")
```

### P0.4.2 — Add OkHttp (needed for AI brain in Phase 1)

Add to `dependencies` block now so Phase 1 doesn't need a separate Gradle sync:

```kotlin
implementation("com.squareup.okhttp3:okhttp:4.12.0")
```

### P0.4.3 — Enable BuildConfig (needed for API key in Phase 1)

In the `android { buildFeatures { ... } }` block, add:

```kotlin
buildConfig = true
```

In `android { defaultConfig { ... } }`, add:

```kotlin
buildConfigField(
    "String", "CLAUDE_API_KEY",
    "\"${project.properties.getOrDefault("CLAUDE_API_KEY", "")}\""
)
```

### P0.4.4 — Create test source directory

```
android/dmed-scanner-working/app/src/test/java/com/dmed/scanner/
```

Create a placeholder test:

**New file:** `android/dmed-scanner-working/app/src/test/java/com/dmed/scanner/PlaceholderTest.kt`

```kotlin
package com.dmed.scanner

import org.junit.Test
import org.junit.Assert.assertTrue

class PlaceholderTest {
    @Test
    fun testRunnerWorks() {
        assertTrue(true)
    }
}
```

### P0.4.5 — Add INTERNET permission to manifest

**File:** `android/dmed-scanner-working/app/src/main/AndroidManifest.xml`

Add after the existing `<uses-permission>` lines:

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.CHANGE_NETWORK_STATE" />
```

`INTERNET` is required for Claude API calls and HTTP card reading.
`CHANGE_NETWORK_STATE` allows the app to query network interface details for mDNS.

**Verify:** `cd android/dmed-scanner-working && ./gradlew test` — passes.

### Phase 0 Definition of Done

- [ ] `voip-phone-dmed-session.json` deleted and committed
- [ ] All untracked examples and android committed
- [ ] `npm test` in `examples/smart-coffee-machine/` → green
- [ ] `./gradlew test` in `android/dmed-scanner-working/` → green
- [ ] No compiler errors in Android project

---

# Phase 1 — Demo

**Goal:** Phone discovers coffee machine on WiFi/Ethernet/BLE, natural language controls it end-to-end.

Ordered by dependency: models → network layer → discovery → brain → UI → tests.

---

## P1.1 — Update Models

**File:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/data/Models.kt`

### Changes

**1. Add `TransportType` enum** — tracks how a device was found:

```kotlin
enum class TransportType { BLE, MDNS }
```

**2. Add `discoveredVia`, `httpHost`, `httpPort` to `DmedEndpoint`:**

```kotlin
data class DmedEndpoint(
    val name: String,
    val address: String,
    val icon: String = "📡",
    val description: String = "",
    val instanceId: String = "",
    val serviceType: Int = 0,
    val discoveredVia: TransportType = TransportType.BLE,
    val httpHost: String? = null,
    val httpPort: Int = 8080,
    var card: DmedCard? = null
)
```

**3. Add `TransportEntry` and update `DmedCard`** to match the actual manifest format (`transports` is an array, not a nested object):

```kotlin
data class TransportEntry(
    val type: String,
    val url: String? = null,
    val service_uuid: String? = null,
    val priority: Int = 1
)

data class DmedCard(
    val dmed_version: String,
    val instance_id: String,
    val name: String,
    val description: String,
    val service_type: String,
    val capabilities: Capabilities,
    val metadata: Map<String, String>?,
    val transports: List<TransportEntry>? = null,
    val auth: Map<String, String>? = null
)
```

**4. Add AI conversation types** at the bottom of the file:

```kotlin
data class ToolCall(
    val id: String,
    val name: String,
    val input: Map<String, Any>
)

data class AiResponse(
    val message: String,
    val toolCall: ToolCall? = null
)
```

**Remove** the old `Transport` and `HttpTransport` data classes — replaced by `TransportEntry`.

---

## P1.2 — HTTP Card Reader

**New file:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/network/HttpCardReader.kt`

Used when a device is discovered via mDNS (WiFi/Ethernet) — fetches the capability card over HTTP instead of BLE GATT.

```kotlin
package com.dmed.scanner.network

import com.dmed.scanner.data.DmedCard
import com.google.gson.Gson
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.util.concurrent.TimeUnit

class HttpCardReader {

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .build()
    private val gson = Gson()

    var debugError: String = ""
        private set

    suspend fun readCard(host: String, port: Int): DmedCard? = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url("http://$host:$port/dmed/card")
                .get()
                .build()
            val response = client.newCall(request).execute()
            if (!response.isSuccessful) {
                debugError = "HTTP ${response.code}"
                return@withContext null
            }
            val body = response.body?.string() ?: run {
                debugError = "Empty response body"
                return@withContext null
            }
            gson.fromJson(body, DmedCard::class.java)
        } catch (e: Exception) {
            debugError = e.message ?: "Unknown error"
            null
        }
    }
}
```

---

## P1.3 — HTTP Action Client

**New file:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/network/HttpActionClient.kt`

Sends actions to mDNS/HTTP devices via `POST /dmed/action`.

```kotlin
package com.dmed.scanner.network

import com.dmed.scanner.data.ActionResponse
import com.google.gson.Gson
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit

class HttpActionClient {

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .build()
    private val gson = Gson()
    private val json = "application/json".toMediaType()

    suspend fun sendAction(
        host: String,
        port: Int,
        action: String,
        params: Map<String, Any> = emptyMap()
    ): ActionResponse? = withContext(Dispatchers.IO) {
        try {
            val body = gson.toJson(mapOf("action" to action, "params" to params))
            val request = Request.Builder()
                .url("http://$host:$port/dmed/action")
                .post(body.toRequestBody(json))
                .build()
            val response = client.newCall(request).execute()
            val responseBody = response.body?.string() ?: return@withContext null
            gson.fromJson(responseBody, ActionResponse::class.java)
        } catch (e: Exception) {
            null
        }
    }
}
```

---

## P1.4 — Fix BLE DMED-only filtering

**File:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/discovery/DmedDiscovery.kt`

### Change 1: Wire `isDmedDevice()` into `onScanResult`

Current `onScanResult` (line 32) accepts every device. Replace the body:

```kotlin
override fun onScanResult(callbackType: Int, result: ScanResult) {
    val record = result.scanRecord ?: return

    // Only accept DMED devices
    if (!isDmedDevice(record)) return

    val device = result.device
    val name = record.deviceName ?: device.name ?: return  // require a name

    val serviceData = record.getServiceData(ParcelUuid(DMED_BEACON_SERVICE_UUID))
    var serviceType = 0
    var instanceId = ""
    if (serviceData != null && serviceData.size >= 6) {
        serviceType = serviceData[1].toInt() and 0xFF
        instanceId = String.format("%02X%02X%02X%02X",
            serviceData[2], serviceData[3], serviceData[4], serviceData[5])
    }

    val endpoint = DmedEndpoint(
        name = name,
        address = device.address,
        icon = "📡",
        description = "BLE DMED Device",
        instanceId = instanceId,
        serviceType = serviceType,
        discoveredVia = TransportType.BLE    // new field
    )

    val current = _endpoints.value.toMutableList()
    if (current.none { it.address == endpoint.address }) {
        current.add(endpoint)
        _endpoints.value = current
    }
}
```

Add import: `import com.dmed.scanner.data.TransportType`

---

## P1.5 — Add mDNS discovery (NSD)

**File:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/discovery/DmedDiscovery.kt`

### Change 1: Add NSD imports

```kotlin
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
```

### Change 2: Add NsdManager field

After the `bluetoothManager` line, add:

```kotlin
private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
private var discoveryListener: NsdManager.DiscoveryListener? = null
```

### Change 3: Add `startMdnsScan()` and `stopMdnsScan()`

Add these two functions to the class:

```kotlin
private fun startMdnsScan() {
    val listener = object : NsdManager.DiscoveryListener {

        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
            _isScanning.value = false
        }

        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {}

        override fun onDiscoveryStarted(serviceType: String) {}

        override fun onDiscoveryStopped(serviceType: String) {}

        override fun onServiceFound(service: NsdServiceInfo) {
            nsdManager.resolveService(service, object : NsdManager.ResolveListener {

                override fun onResolveFailed(info: NsdServiceInfo, errorCode: Int) {}

                override fun onServiceResolved(info: NsdServiceInfo) {
                    val host = info.host?.hostAddress ?: return
                    val port = info.port
                    val name = info.serviceName

                    // Parse instance_id from TXT record if present
                    val txtAttrs = info.attributes
                    val instanceId = txtAttrs["id"]?.let { String(it) } ?: ""
                    val icon = when (txtAttrs["st"]?.let { String(it) }) {
                        "01" -> "💡"
                        "02" -> "☕"
                        "03" -> "📞"
                        else -> "📡"
                    }

                    val endpoint = DmedEndpoint(
                        name = name,
                        address = "$host:$port",
                        icon = icon,
                        description = "mDNS DMED Device",
                        instanceId = instanceId,
                        discoveredVia = TransportType.MDNS,
                        httpHost = host,
                        httpPort = port
                    )

                    val current = _endpoints.value.toMutableList()
                    // Deduplicate: if same instanceId already from BLE, skip
                    val alreadyPresent = instanceId.isNotEmpty() &&
                        current.any { it.instanceId == instanceId }
                    if (!alreadyPresent && current.none { it.address == endpoint.address }) {
                        current.add(endpoint)
                        _endpoints.value = current
                    }
                }
            })
        }

        override fun onServiceLost(service: NsdServiceInfo) {
            val current = _endpoints.value.toMutableList()
            current.removeAll { it.name == service.serviceName && it.discoveredVia == TransportType.MDNS }
            _endpoints.value = current
        }
    }

    discoveryListener = listener
    try {
        nsdManager.discoverServices("_dmed._tcp", NsdManager.PROTOCOL_DNS_SD, listener)
    } catch (_: Exception) {}
}

private fun stopMdnsScan() {
    discoveryListener?.let {
        try { nsdManager.stopServiceDiscovery(it) } catch (_: Exception) {}
        discoveryListener = null
    }
}
```

### Change 4: Update `startScan()` and `stopScan()` to call both

```kotlin
fun startScan() {
    if (_isScanning.value) return
    _endpoints.value = emptyList()
    _isScanning.value = true

    // BLE scan
    val s = scanner
    if (s != null) {
        s.startScan(scanCallback)
    }

    // mDNS scan (WiFi + Ethernet)
    startMdnsScan()
}

fun stopScan() {
    stopMdnsScan()
    try { scanner?.stopScan(scanCallback) } catch (_: Exception) {}
    _isScanning.value = false
}
```

---

## P1.6 — Route card reading and action dispatch by transport

**File:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/ui/MainViewModel.kt`

### Change 1: Add `HttpCardReader` and `HttpActionClient`

```kotlin
private val httpCardReader = HttpCardReader()
private val httpActionClient = HttpActionClient()
```

Add imports:
```kotlin
import com.dmed.scanner.data.TransportType
import com.dmed.scanner.network.HttpCardReader
import com.dmed.scanner.network.HttpActionClient
```

### Change 2: Update `selectEndpoint()` to route by transport

```kotlin
fun selectEndpoint(endpoint: DmedEndpoint) {
    viewModelScope.launch {
        _selectedEndpoint.value = endpoint
        _messages.value = listOf(ChatMessage("🔗 Connecting to read capability card...", false))

        val card = when (endpoint.discoveredVia) {
            TransportType.BLE -> bleCardReader.readCard(endpoint.address)
            TransportType.MDNS -> httpCardReader.readCard(endpoint.httpHost!!, endpoint.httpPort)
        }

        if (card != null) {
            endpoint.card = card
            _actions.value = card.capabilities.tools
            _messages.value = listOf(
                ChatMessage(
                    "Connected to ${card.name}\n${card.description}\n\n" +
                    "Transport: ${endpoint.discoveredVia.name}\n\n" +
                    "Available actions:\n" +
                    card.capabilities.tools.joinToString("\n") { "• ${it.name} — ${it.description}" },
                    false
                )
            )
        } else {
            val err = when (endpoint.discoveredVia) {
                TransportType.BLE -> bleCardReader.debugError.ifEmpty { "no details" }
                TransportType.MDNS -> httpCardReader.debugError.ifEmpty { "no details" }
            }
            _messages.value = listOf(ChatMessage("❌ Could not read capability card\n\n$err", false))
        }
    }
}
```

### Change 3: Add private `dispatch()` helper

```kotlin
private suspend fun dispatch(
    endpoint: DmedEndpoint,
    action: String,
    params: Map<String, Any>
): ActionResponse? {
    return when (endpoint.discoveredVia) {
        TransportType.BLE ->
            bleActionClient.sendAction(endpoint.address, action, params)
        TransportType.MDNS ->
            httpActionClient.sendAction(endpoint.httpHost!!, endpoint.httpPort, action, params)
    }
}
```

### Change 4: Update `sendAction()` to use `dispatch()`

```kotlin
fun sendAction(actionName: String, params: Map<String, Any> = emptyMap()) {
    val endpoint = _selectedEndpoint.value ?: return
    val displayText = if (params.isEmpty()) actionName
        else "$actionName(${params.entries.joinToString { "${it.key}=${it.value}" }})"

    val current = _messages.value.toMutableList()
    current.add(ChatMessage(displayText, true))
    _messages.value = current

    viewModelScope.launch {
        val resp = dispatch(endpoint, actionName, params)
        val reply = when {
            resp == null -> "❌ No response from device"
            resp.status == "ok" -> resp.result?.text ?: "✅ Done"
            else -> "⚠️ ${resp.message ?: "Error"}"
        }
        val updated = _messages.value.toMutableList()
        updated.add(ChatMessage(reply, false))
        _messages.value = updated
    }
}
```

---

## P1.7 — AI Brain

**New file:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/ai/AiBrain.kt`

This is the Claude API integration. It receives the device capability card + conversation history + user input, and returns either a text message or a tool call.

```kotlin
package com.dmed.scanner.ai

import com.dmed.scanner.data.AiResponse
import com.dmed.scanner.data.DmedCard
import com.dmed.scanner.data.ToolCall
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit

class AiBrain(private val apiKey: String) {

    private val client = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(30, TimeUnit.SECONDS)
        .build()
    private val gson = Gson()
    private val jsonMedia = "application/json".toMediaType()

    fun buildSystemPrompt(card: DmedCard): String = """
You are a natural language interface for a physical device: ${card.name}.
Device description: ${card.description}

When the user asks you to do something the device supports, call the appropriate tool.
After a tool call succeeds, confirm naturally in one sentence. 
If the user asks a question about device state, call the relevant tool and report the result.
If asked what you can do, list the available actions briefly.
Keep all responses short and conversational.
    """.trimIndent()

    data class ApiMessage(val role: String, val content: Any)

    suspend fun chat(
        card: DmedCard,
        history: List<ApiMessage>,
        userInput: String
    ): AiResponse = withContext(Dispatchers.IO) {

        val tools = card.capabilities.tools.map { tool ->
            mapOf(
                "name" to tool.name,
                "description" to tool.description,
                "input_schema" to (tool.inputSchema
                    ?: mapOf("type" to "object", "properties" to emptyMap<String, Any>()))
            )
        }

        val messages = history + ApiMessage("user", userInput)

        val requestBody = mapOf(
            "model" to "claude-haiku-4-5-20251001",
            "max_tokens" to 1024,
            "system" to buildSystemPrompt(card),
            "tools" to tools,
            "messages" to messages
        )

        val request = Request.Builder()
            .url("https://api.anthropic.com/v1/messages")
            .addHeader("x-api-key", apiKey)
            .addHeader("anthropic-version", "2023-06-01")
            .addHeader("content-type", "application/json")
            .post(gson.toJson(requestBody).toRequestBody(jsonMedia))
            .build()

        val response = client.newCall(request).execute()
        val responseBody = response.body?.string()
            ?: return@withContext AiResponse("❌ Empty response from Claude")

        if (!response.isSuccessful) {
            return@withContext AiResponse("❌ Claude API error ${response.code}: ${responseBody.take(200)}")
        }

        val mapType = object : TypeToken<Map<String, Any>>() {}.type
        val parsed: Map<String, Any> = gson.fromJson(responseBody, mapType)
        val contentBlocks = parsed["content"] as? List<*>
            ?: return@withContext AiResponse("❌ Unexpected response format")

        var textMessage = ""
        var toolCall: ToolCall? = null

        for (block in contentBlocks) {
            val b = block as? Map<*, *> ?: continue
            when (b["type"]) {
                "text" -> textMessage = b["text"] as? String ?: ""
                "tool_use" -> {
                    val inputMap = b["input"] as? Map<*, *> ?: emptyMap<String, Any>()
                    @Suppress("UNCHECKED_CAST")
                    toolCall = ToolCall(
                        id = b["id"] as? String ?: "",
                        name = b["name"] as? String ?: "",
                        input = inputMap as Map<String, Any>
                    )
                }
            }
        }

        AiResponse(message = textMessage, toolCall = toolCall)
    }

    suspend fun narrateResult(
        card: DmedCard,
        history: List<ApiMessage>,
        userInput: String,
        toolCall: ToolCall,
        toolResult: String
    ): String = withContext(Dispatchers.IO) {

        // Full Claude tool-use flow: user → tool_use → tool_result → narration
        val messagesWithToolResult = history + listOf(
            ApiMessage("user", userInput),
            ApiMessage("assistant", listOf(
                mapOf("type" to "tool_use", "id" to toolCall.id, "name" to toolCall.name, "input" to toolCall.input)
            )),
            ApiMessage("user", listOf(
                mapOf("type" to "tool_result", "tool_use_id" to toolCall.id, "content" to toolResult)
            ))
        )

        val requestBody = mapOf(
            "model" to "claude-haiku-4-5-20251001",
            "max_tokens" to 512,
            "system" to buildSystemPrompt(card),
            "messages" to messagesWithToolResult
        )

        val request = Request.Builder()
            .url("https://api.anthropic.com/v1/messages")
            .addHeader("x-api-key", apiKey)
            .addHeader("anthropic-version", "2023-06-01")
            .addHeader("content-type", "application/json")
            .post(gson.toJson(requestBody).toRequestBody(jsonMedia))
            .build()

        val response = client.newCall(request).execute()
        val responseBody = response.body?.string() ?: return@withContext toolResult

        if (!response.isSuccessful) return@withContext toolResult

        val mapType = object : TypeToken<Map<String, Any>>() {}.type
        val parsed: Map<String, Any> = gson.fromJson(responseBody, mapType)
        val contentBlocks = parsed["content"] as? List<*> ?: return@withContext toolResult

        for (block in contentBlocks) {
            val b = block as? Map<*, *> ?: continue
            if (b["type"] == "text") return@withContext b["text"] as? String ?: toolResult
        }

        toolResult
    }
}
```

---

## P1.8 — Wire AI Brain into MainViewModel

**File:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/ui/MainViewModel.kt`

### Change 1: Add `AiBrain` field

```kotlin
import com.dmed.scanner.BuildConfig
import com.dmed.scanner.ai.AiBrain

private val aiBrain = AiBrain(BuildConfig.CLAUDE_API_KEY)
private val aiHistory = mutableListOf<AiBrain.ApiMessage>()
```

### Change 2: Clear `aiHistory` in `clearSelection()`

```kotlin
fun clearSelection() {
    _selectedEndpoint.value = null
    _messages.value = emptyList()
    _actions.value = emptyList()
    aiHistory.clear()
}
```

### Change 3: Add `processMessage()` — replaces raw `sendAction()` for the chat input

Keep `sendAction()` for the quick-action chips (direct dispatch, no brain).
Add `processMessage()` for the free-text input:

```kotlin
fun processMessage(userInput: String) {
    val endpoint = _selectedEndpoint.value ?: return
    val card = endpoint.card ?: return

    val current = _messages.value.toMutableList()
    current.add(ChatMessage(userInput, true))
    current.add(ChatMessage("…", false))  // thinking indicator
    _messages.value = current

    viewModelScope.launch {
        val response = try {
            aiBrain.chat(card, aiHistory.toList(), userInput)
        } catch (e: Exception) {
            AiResponse("❌ ${e.message ?: "Claude error"}")
        }

        if (response.toolCall != null) {
            // Dispatch the action
            val actionResp = dispatch(endpoint, response.toolCall.name, response.toolCall.input)
            val rawResult = when {
                actionResp == null -> "No response from device"
                actionResp.status == "ok" -> actionResp.result?.text ?: "Done"
                else -> actionResp.message ?: "Error"
            }

            // Get Claude to narrate the result
            val narration = try {
                aiBrain.narrateResult(card, aiHistory.toList(), userInput, response.toolCall, rawResult)
            } catch (_: Exception) {
                rawResult
            }

            // Update history
            aiHistory.add(AiBrain.ApiMessage("user", userInput))
            aiHistory.add(AiBrain.ApiMessage("assistant", narration))

            // Replace thinking indicator with narration
            val updated = _messages.value.toMutableList()
            updated[updated.lastIndex] = ChatMessage(narration, false)
            _messages.value = updated

        } else {
            // Pure text response
            aiHistory.add(AiBrain.ApiMessage("user", userInput))
            aiHistory.add(AiBrain.ApiMessage("assistant", response.message))

            val updated = _messages.value.toMutableList()
            updated[updated.lastIndex] = ChatMessage(response.message, false)
            _messages.value = updated
        }
    }
}
```

Import needed:
```kotlin
import com.dmed.scanner.data.AiResponse
```

---

## P1.9 — Update ChatScreen to use processMessage

**File:** `android/dmed-scanner-working/app/src/main/java/com/dmed/scanner/ui/ChatScreen.kt`

### Change 1: Update send button to call `processMessage`

Find the `IconButton` `onClick` handler (currently calls `viewModel.sendAction(inputText.trim())`).
Change it to:

```kotlin
onClick = {
    if (inputText.isNotBlank()) {
        viewModel.processMessage(inputText.trim())
        inputText = ""
    }
}
```

### Change 2: Update placeholder text

```kotlin
placeholder = { Text("Ask in natural language...") }
```

### Change 3: Style the "thinking" indicator

In `ChatBubble`, when `message.text == "…"`, show a `CircularProgressIndicator` instead of text:

```kotlin
@Composable
fun ChatBubble(message: ChatMessage) {
    val bgColor = if (message.isUser) MaterialTheme.colorScheme.primaryContainer
                  else MaterialTheme.colorScheme.surfaceVariant
    val alignment = if (message.isUser) Arrangement.End else Arrangement.Start

    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = alignment) {
        Box(
            modifier = Modifier
                .widthIn(max = 300.dp)
                .background(bgColor, RoundedCornerShape(12.dp))
                .padding(12.dp)
        ) {
            if (message.text == "…" && !message.isUser) {
                CircularProgressIndicator(
                    modifier = Modifier.size(20.dp),
                    strokeWidth = 2.dp
                )
            } else {
                androidx.compose.foundation.text.selection.SelectionContainer {
                    Text(message.text)
                }
            }
        }
    }
}
```

Add import: `import androidx.compose.ui.unit.dp` and `import androidx.compose.material3.CircularProgressIndicator`

---

## P1.10 — Add API key to local.properties

**File:** `android/dmed-scanner-working/local.properties`

Add:
```
CLAUDE_API_KEY=your-key-here
```

Add `local.properties` to `.gitignore` if not already present (it usually is by default).

---

## P1.11 — Coffee machine server tests

**File:** `examples/smart-coffee-machine/server.test.js`

Replace the placeholder with the full test suite:

```js
import { describe, it, expect, beforeEach } from 'vitest'
import request from 'supertest'
import { createApp } from './server.js'

describe('GET /dmed/card', () => {
  const { app } = createApp()

  it('returns 200', async () => {
    const res = await request(app).get('/dmed/card')
    expect(res.status).toBe(200)
  })

  it('has required DMED fields', async () => {
    const res = await request(app).get('/dmed/card')
    expect(res.body).toMatchObject({
      dmed_version: expect.any(String),
      instance_id: expect.any(String),
      name: expect.any(String),
      transports: expect.any(Array),
    })
  })

  it('capabilities includes brew_coffee, get_status, set_temperature', async () => {
    const res = await request(app).get('/dmed/card')
    const names = res.body.capabilities.tools.map(t => t.name)
    expect(names).toContain('brew_coffee')
    expect(names).toContain('get_status')
    expect(names).toContain('set_temperature')
  })
})

describe('GET /dmed/actions', () => {
  const { app } = createApp()

  it('returns 200 with actions array', async () => {
    const res = await request(app).get('/dmed/actions')
    expect(res.status).toBe(200)
    expect(res.body.actions).toBeInstanceOf(Array)
    expect(res.body.actions.length).toBeGreaterThan(0)
  })

  it('action count matches capability card', async () => {
    const cardRes = await request(app).get('/dmed/card')
    const actionsRes = await request(app).get('/dmed/actions')
    expect(actionsRes.body.actions.length).toBe(cardRes.body.capabilities.tools.length)
  })
})

describe('POST /dmed/action — brew_coffee', () => {
  let app, state

  beforeEach(() => {
    const result = createApp()
    app = result.app
    state = result.state
  })

  it('valid params → 200 with ok status', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'large' } })
    expect(res.status).toBe(200)
    expect(res.body.status).toBe('ok')
  })

  it('result text mentions drink_type and size', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'espresso', size: 'small' } })
    expect(res.body.result.text).toMatch(/espresso/i)
    expect(res.body.result.text).toMatch(/small/i)
  })

  it('water level decreases after brew', async () => {
    const before = state.water_level
    await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'large' } })
    expect(state.water_level).toBeLessThan(before)
  })

  it('brew when water < 10 returns error result', async () => {
    state.water_level = 5
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'brew_coffee', params: { drink_type: 'latte', size: 'small' } })
    expect(res.body.result.text).toMatch(/water/i)
  })

  it('missing action field → 400', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ params: {} })
    expect(res.status).toBe(400)
  })

  it('unknown action → 404', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'make_tea' })
    expect(res.status).toBe(404)
  })
})

describe('POST /dmed/action — get_status', () => {
  const { app, state } = createApp()

  it('returns water_level, bean_level, temperature, is_brewing', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'get_status' })
    const body = JSON.parse(res.body.result.text)
    expect(body).toHaveProperty('water_level')
    expect(body).toHaveProperty('bean_level')
    expect(body).toHaveProperty('temperature')
    expect(body).toHaveProperty('is_brewing')
  })
})

describe('POST /dmed/action — set_temperature', () => {
  let app, state

  beforeEach(() => {
    const result = createApp()
    app = result.app
    state = result.state
  })

  it('valid temperature → ok and state updated', async () => {
    const res = await request(app)
      .post('/dmed/action')
      .send({ action: 'set_temperature', params: { temperature: 93 } })
    expect(res.body.status).toBe('ok')
    expect(state.temperature).toBe(93)
  })
})
```

---

## P1.12 — Android unit tests

### P1.12.1 — DmedDiscoveryTest

**New file:** `android/dmed-scanner-working/app/src/test/java/com/dmed/scanner/discovery/DmedDiscoveryTest.kt`

```kotlin
package com.dmed.scanner.discovery

import com.dmed.scanner.data.DmedEndpoint
import com.dmed.scanner.data.TransportType
import org.junit.Test
import org.junit.Assert.*

class DmedDiscoveryTest {

    // isDmedDevice is private — test it indirectly via the DMED UUID byte pattern
    // We test the UUID constant and the raw-byte logic by constructing known byte arrays

    @Test
    fun dmedBeaconUuidIsCorrect() {
        val uuid = DmedDiscovery.DMED_BEACON_SERVICE_UUID
        assertEquals("d14d0000-1000-4000-8000-00805f9b34fb", uuid.toString())
    }

    @Test
    fun dmedCardUuidIsCorrect() {
        val uuid = DmedDiscovery.DMED_CARD_SERVICE_UUID
        assertEquals("d14d0001-1000-4000-8000-00805f9b34fb", uuid.toString())
    }

    @Test
    fun endpointCreatedWithBleTransportType() {
        val endpoint = DmedEndpoint(
            name = "Test Device",
            address = "AA:BB:CC:DD:EE:FF",
            discoveredVia = TransportType.BLE
        )
        assertEquals(TransportType.BLE, endpoint.discoveredVia)
        assertNull(endpoint.httpHost)
    }

    @Test
    fun endpointCreatedWithMdnsTransportType() {
        val endpoint = DmedEndpoint(
            name = "Coffee Machine",
            address = "192.168.1.42:3100",
            discoveredVia = TransportType.MDNS,
            httpHost = "192.168.1.42",
            httpPort = 3100
        )
        assertEquals(TransportType.MDNS, endpoint.discoveredVia)
        assertEquals("192.168.1.42", endpoint.httpHost)
        assertEquals(3100, endpoint.httpPort)
    }

    @Test
    fun deduplicationByInstanceId() {
        // Same instanceId from BLE and mDNS → should not both appear
        val bleEndpoint = DmedEndpoint(
            name = "Device", address = "AA:BB:CC:DD:EE:FF",
            instanceId = "c0ffee01", discoveredVia = TransportType.BLE
        )
        val mdnsEndpoint = DmedEndpoint(
            name = "Device", address = "192.168.1.42:3100",
            instanceId = "c0ffee01", discoveredVia = TransportType.MDNS
        )

        val list = mutableListOf(bleEndpoint)
        val alreadyPresent = mdnsEndpoint.instanceId.isNotEmpty() &&
            list.any { it.instanceId == mdnsEndpoint.instanceId }
        assertTrue("Same instanceId should be detected as duplicate", alreadyPresent)
    }
}
```

### P1.12.2 — AiBrainTest

**New file:** `android/dmed-scanner-working/app/src/test/java/com/dmed/scanner/ai/AiBrainTest.kt`

```kotlin
package com.dmed.scanner.ai

import com.dmed.scanner.data.*
import org.junit.Test
import org.junit.Assert.*

class AiBrainTest {

    private val brain = AiBrain("test-key")

    private fun makeCard(tools: List<Tool> = emptyList()) = DmedCard(
        dmed_version = "0.2.0",
        instance_id = "test01",
        name = "Smart Coffee Machine",
        description = "Brews coffee on demand",
        service_type = "iot_device",
        capabilities = Capabilities(tools),
        metadata = null
    )

    @Test
    fun systemPromptContainsDeviceName() {
        val card = makeCard()
        val prompt = brain.buildSystemPrompt(card)
        assertTrue("Prompt should contain device name", prompt.contains("Smart Coffee Machine"))
    }

    @Test
    fun systemPromptContainsDeviceDescription() {
        val card = makeCard()
        val prompt = brain.buildSystemPrompt(card)
        assertTrue("Prompt should contain description", prompt.contains("Brews coffee on demand"))
    }

    @Test
    fun toolCallDataClassHoldsCorrectValues() {
        val call = ToolCall(
            id = "call_abc",
            name = "brew_coffee",
            input = mapOf("drink_type" to "latte", "size" to "large")
        )
        assertEquals("call_abc", call.id)
        assertEquals("brew_coffee", call.name)
        assertEquals("latte", call.input["drink_type"])
        assertEquals("large", call.input["size"])
    }

    @Test
    fun aiResponseWithNoToolCall() {
        val resp = AiResponse(message = "I can brew espresso, latte, and americano.")
        assertEquals("I can brew espresso, latte, and americano.", resp.message)
        assertNull(resp.toolCall)
    }

    @Test
    fun aiResponseWithToolCall() {
        val resp = AiResponse(
            message = "Brewing your latte now.",
            toolCall = ToolCall("id1", "brew_coffee", mapOf("drink_type" to "latte", "size" to "large"))
        )
        assertNotNull(resp.toolCall)
        assertEquals("brew_coffee", resp.toolCall!!.name)
    }
}
```

### P1.12.3 — MainViewModelTest

**New file:** `android/dmed-scanner-working/app/src/test/java/com/dmed/scanner/ui/MainViewModelTest.kt`

```kotlin
package com.dmed.scanner.ui

import com.dmed.scanner.data.*
import org.junit.Test
import org.junit.Assert.*

class MainViewModelTest {

    private fun makeEndpoint(via: TransportType = TransportType.MDNS) = DmedEndpoint(
        name = "Coffee Machine",
        address = "192.168.1.42:3100",
        discoveredVia = via,
        httpHost = "192.168.1.42",
        httpPort = 3100
    )

    private fun makeCard() = DmedCard(
        dmed_version = "0.2.0",
        instance_id = "c0ffee01",
        name = "Smart Coffee Machine",
        description = "Brews coffee",
        service_type = "iot_device",
        capabilities = Capabilities(listOf(
            Tool("brew_coffee", "Brew coffee", null),
            Tool("get_status", "Get status", null)
        )),
        metadata = null
    )

    @Test
    fun chatMessageDataClass() {
        val msg = ChatMessage("Hello", isUser = true)
        assertEquals("Hello", msg.text)
        assertTrue(msg.isUser)
    }

    @Test
    fun dmedEndpointDefaultsToBlue() {
        val ep = DmedEndpoint(name = "Test", address = "AA:BB:CC:DD:EE:FF")
        assertEquals(TransportType.BLE, ep.discoveredVia)
    }

    @Test
    fun toolListFromCard() {
        val card = makeCard()
        val tools = card.capabilities.tools
        assertEquals(2, tools.size)
        assertEquals("brew_coffee", tools[0].name)
        assertEquals("get_status", tools[1].name)
    }

    @Test
    fun cardAssignedToEndpoint() {
        val ep = makeEndpoint()
        val card = makeCard()
        ep.card = card
        assertNotNull(ep.card)
        assertEquals("Smart Coffee Machine", ep.card!!.name)
    }

    @Test
    fun actionResponseParsing() {
        val resp = ActionResponse(
            status = "ok",
            action = "brew_coffee",
            result = ActionResult("☕ Your large latte is ready!")
        )
        assertEquals("ok", resp.status)
        assertEquals("☕ Your large latte is ready!", resp.result?.text)
    }

    @Test
    fun actionResponseError() {
        val resp = ActionResponse(
            status = "error",
            action = "brew_coffee",
            message = "Water level too low"
        )
        assertEquals("error", resp.status)
        assertEquals("Water level too low", resp.message)
    }
}
```

### P1.12.4 — HttpActionClientTest

**New file:** `android/dmed-scanner-working/app/src/test/java/com/dmed/scanner/network/HttpActionClientTest.kt`

```kotlin
package com.dmed.scanner.network

import com.dmed.scanner.data.ActionResponse
import com.dmed.scanner.data.ActionResult
import com.google.gson.Gson
import org.junit.Test
import org.junit.Assert.*

class HttpActionClientTest {

    private val gson = Gson()

    @Test
    fun actionResponseDeserialization_success() {
        val json = """{"status":"ok","action":"brew_coffee","result":{"text":"☕ Your latte is ready!"}}"""
        val resp = gson.fromJson(json, ActionResponse::class.java)
        assertEquals("ok", resp.status)
        assertEquals("brew_coffee", resp.action)
        assertEquals("☕ Your latte is ready!", resp.result?.text)
    }

    @Test
    fun actionResponseDeserialization_error() {
        val json = """{"status":"error","action":"brew_coffee","message":"Water level too low"}"""
        val resp = gson.fromJson(json, ActionResponse::class.java)
        assertEquals("error", resp.status)
        assertEquals("Water level too low", resp.message)
        assertNull(resp.result)
    }

    @Test
    fun actionResponseDeserialization_missingResult() {
        val json = """{"status":"ok","action":"get_status"}"""
        val resp = gson.fromJson(json, ActionResponse::class.java)
        assertEquals("ok", resp.status)
        assertNull(resp.result)
    }
}
```

---

## P1.13 — End-to-end demo checklist

**New file:** `docs/demo-checklist.md`

```markdown
# DMED Demo Checklist

## Setup
- [ ] `cd examples/smart-coffee-machine && npm start`
- [ ] Confirm: `[DMED] ✓ Broadcasting "Smart Coffee Machine" on _dmed._tcp port 3100`
- [ ] Confirm: `http://<IP>:3100/dmed/card` returns JSON in browser
- [ ] Phone and laptop on same WiFi network

## Scanner
- [ ] Open DMED Scanner app on Android
- [ ] Tap "Scan for DMED BLE Devices"
- [ ] "Smart Coffee Machine ☕" appears within 10 seconds (via mDNS)
- [ ] Tap the card — chat screen opens
- [ ] Capability card loads: shows name, description, available actions

## Natural language interaction
- [ ] Type: "what can you do?" — Claude lists actions
- [ ] Type: "make me a large latte" — brew_coffee dispatched, confirmation shown
- [ ] Type: "how much water is left?" — get_status called, Claude narrates result
- [ ] Type: "set temperature to 94" — set_temperature dispatched

## Edge cases
- [ ] Type nonsense: "what's the weather?" — Claude explains it can't do that
- [ ] Stop coffee machine server — next action shows error message, no crash
- [ ] Restart server — rescan, reconnect, works again
```

---

### Phase 1 Definition of Done

- [ ] Android scanner shows only DMED devices (BLE and mDNS), no generic BLE noise
- [ ] Coffee machine discovered via mDNS on same WiFi
- [ ] Capability card read over HTTP — shows name, description, actions
- [ ] Natural language chat dispatches correct actions with correct params
- [ ] Claude narrates results conversationally
- [ ] `npm test` in `examples/smart-coffee-machine/` → all green
- [ ] `./gradlew test` in Android → all green
- [ ] Demo checklist runs clean

---

# Phase 2 — Discovery Framework SDK

**Goal:** A reusable, well-tested `lib/js/` TypeScript library that any client (Node.js, browser, or serves as the canonical implementation) can use for DMED discovery and action dispatch.

---

## P2.1 — Scaffold lib/js

**New directory:** `lib/js/`

```
lib/js/
  src/
    index.ts          — public API exports
    discovery.ts      — DmedDiscovery class
    card.ts           — DmedCard parsing and validation
    transport.ts      — transport selector and HTTP dispatch
    types.ts          — all TypeScript types
  tests/
    card.test.ts
    transport.test.ts
    discovery.test.ts
  package.json
  tsconfig.json
  vitest.config.ts
  README.md
```

### P2.1.1 — package.json

```json
{
  "name": "@dmed/discovery",
  "version": "0.3.0",
  "type": "module",
  "main": "dist/index.js",
  "types": "dist/index.d.ts",
  "scripts": {
    "build": "tsc",
    "test": "vitest run",
    "test:watch": "vitest"
  },
  "devDependencies": {
    "typescript": "^5.3.0",
    "vitest": "^1.6.0",
    "@types/node": "^20.0.0"
  },
  "dependencies": {
    "zeroconf-service": "^1.0.0"
  }
}
```

### P2.1.2 — types.ts

All TypeScript types matching the DMED spec exactly:

```typescript
export interface TransportEntry {
  type: 'http' | 'ble' | string
  url?: string
  service_uuid?: string
  priority: number
}

export interface Tool {
  name: string
  description: string
  inputSchema?: Record<string, unknown>
}

export interface DmedCard {
  dmed_version: string
  instance_id: string
  name: string
  description: string
  service_type: string
  transports: TransportEntry[]
  auth: { type: string }
  capabilities: { tools: Tool[] }
  metadata?: Record<string, string>
}

export interface DmedDevice {
  instanceId: string
  name: string
  discoveredVia: 'ble' | 'mdns'
  address: string        // BLE MAC or "host:port"
  httpHost?: string
  httpPort?: number
  card?: DmedCard
}

export interface ActionResponse {
  status: 'ok' | 'error'
  action: string
  result?: { text: string }
  message?: string
}

export type DeviceEventHandler = (device: DmedDevice) => void
```

### P2.1.3 — card.ts

Card parsing and validation:

```typescript
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
```

### P2.1.4 — transport.ts

Transport selection and HTTP action dispatch:

```typescript
import type { DmedCard, DmedDevice, TransportEntry, ActionResponse } from './types.js'

export class NoTransportError extends Error {}
export class DispatchError extends Error {}

export function selectTransport(card: DmedCard): TransportEntry {
  const http = card.transports
    .filter(t => t.type === 'http' && t.url)
    .sort((a, b) => a.priority - b.priority)[0]
  if (!http) throw new NoTransportError(`No HTTP transport in card for ${card.name}`)
  return http
}

export async function fetchCard(host: string, port: number): Promise<DmedCard> {
  const res = await fetch(`http://${host}:${port}/dmed/card`)
  if (!res.ok) throw new DispatchError(`GET /dmed/card failed: ${res.status}`)
  return res.json()
}

export async function dispatchAction(
  device: DmedDevice,
  action: string,
  params: Record<string, unknown> = {}
): Promise<ActionResponse> {
  if (!device.httpHost || !device.httpPort) {
    throw new DispatchError('Device has no HTTP transport')
  }
  const res = await fetch(`http://${device.httpHost}:${device.httpPort}/dmed/action`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ action, params })
  })
  if (!res.ok) throw new DispatchError(`POST /dmed/action failed: ${res.status}`)
  return res.json()
}
```

### P2.1.5 — discovery.ts

Multi-transport DmedDiscovery class:

```typescript
import type { DmedDevice, DeviceEventHandler } from './types.js'
import { fetchCard } from './transport.js'

export class DmedDiscovery {
  private deviceHandlers: DeviceEventHandler[] = []
  private lostHandlers: DeviceEventHandler[] = []
  private devices = new Map<string, DmedDevice>()

  on(event: 'device' | 'lost', handler: DeviceEventHandler): this {
    if (event === 'device') this.deviceHandlers.push(handler)
    if (event === 'lost') this.lostHandlers.push(handler)
    return this
  }

  async connect(device: DmedDevice): Promise<DmedDevice> {
    if (!device.httpHost || !device.httpPort) {
      throw new Error('connect() requires httpHost and httpPort')
    }
    const card = await fetchCard(device.httpHost, device.httpPort)
    return { ...device, card }
  }

  protected addDevice(device: DmedDevice): void {
    const key = device.instanceId || device.address
    if (!this.devices.has(key)) {
      this.devices.set(key, device)
      this.deviceHandlers.forEach(h => h(device))
    }
  }

  protected removeDevice(key: string): void {
    const device = this.devices.get(key)
    if (device) {
      this.devices.delete(key)
      this.lostHandlers.forEach(h => h(device))
    }
  }

  getDevices(): DmedDevice[] {
    return Array.from(this.devices.values())
  }
}
```

### P2.1.6 — index.ts

```typescript
export { DmedDiscovery } from './discovery.js'
export { parseCard, validateCard, CardValidationError } from './card.js'
export { selectTransport, fetchCard, dispatchAction, NoTransportError, DispatchError } from './transport.js'
export type { DmedCard, DmedDevice, Tool, TransportEntry, ActionResponse } from './types.js'
```

---

## P2.2 — lib/js tests

### P2.2.1 — card.test.ts

**New file:** `lib/js/tests/card.test.ts`

```typescript
import { describe, it, expect } from 'vitest'
import { parseCard, validateCard, CardValidationError } from '../src/card.js'

const VALID_CARD_JSON = JSON.stringify({
  dmed_version: '0.2.0',
  instance_id: 'c0ffee01',
  name: 'Smart Coffee Machine',
  description: 'Brews coffee',
  service_type: 'iot_device',
  transports: [{ type: 'http', url: 'http://192.168.1.42:3100/mcp', priority: 1 }],
  auth: { type: 'none' },
  capabilities: {
    tools: [
      { name: 'brew_coffee', description: 'Brew coffee' },
      { name: 'get_status', description: 'Get status' }
    ]
  }
})

describe('parseCard', () => {
  it('parses valid card JSON', () => {
    const card = parseCard(VALID_CARD_JSON)
    expect(card.name).toBe('Smart Coffee Machine')
    expect(card.capabilities.tools).toHaveLength(2)
  })

  it('throws on invalid JSON', () => {
    expect(() => parseCard('{not json}')).toThrow(CardValidationError)
  })

  it('throws on non-object JSON', () => {
    expect(() => parseCard('"just a string"')).toThrow(CardValidationError)
  })

  it('throws on missing dmed_version', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    delete card.dmed_version
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('throws on missing instance_id', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    delete card.instance_id
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('throws on missing capabilities', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    delete card.capabilities
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('throws when capabilities.tools is not an array', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    card.capabilities.tools = 'not an array'
    expect(() => validateCard(card)).toThrow(CardValidationError)
  })

  it('tolerates unknown extra fields (forward compat)', () => {
    const card = JSON.parse(VALID_CARD_JSON)
    card.future_field = 'ignored'
    expect(() => validateCard(card)).not.toThrow()
  })
})
```

### P2.2.2 — transport.test.ts

**New file:** `lib/js/tests/transport.test.ts`

```typescript
import { describe, it, expect, vi } from 'vitest'
import { selectTransport, NoTransportError } from '../src/transport.js'
import type { DmedCard } from '../src/types.js'

function makeCard(transports: DmedCard['transports']): DmedCard {
  return {
    dmed_version: '0.2.0',
    instance_id: 'test01',
    name: 'Test Device',
    description: '',
    service_type: 'iot_device',
    transports,
    auth: { type: 'none' },
    capabilities: { tools: [] }
  }
}

describe('selectTransport', () => {
  it('selects http transport when available', () => {
    const card = makeCard([
      { type: 'http', url: 'http://192.168.1.1:3100/mcp', priority: 1 }
    ])
    const t = selectTransport(card)
    expect(t.type).toBe('http')
    expect(t.url).toContain('192.168.1.1')
  })

  it('selects lowest priority number first', () => {
    const card = makeCard([
      { type: 'http', url: 'http://fallback:8080/mcp', priority: 2 },
      { type: 'http', url: 'http://preferred:3100/mcp', priority: 1 }
    ])
    const t = selectTransport(card)
    expect(t.url).toContain('preferred')
  })

  it('throws NoTransportError when no http transport', () => {
    const card = makeCard([
      { type: 'ble', service_uuid: 'D14D0001-...', priority: 1 }
    ])
    expect(() => selectTransport(card)).toThrow(NoTransportError)
  })

  it('throws NoTransportError when transports list is empty', () => {
    const card = makeCard([])
    expect(() => selectTransport(card)).toThrow(NoTransportError)
  })

  it('ignores http entry without url', () => {
    const card = makeCard([
      { type: 'http', priority: 1 }  // no url
    ])
    expect(() => selectTransport(card)).toThrow(NoTransportError)
  })
})
```

### P2.2.3 — discovery.test.ts

**New file:** `lib/js/tests/discovery.test.ts`

```typescript
import { describe, it, expect } from 'vitest'
import { DmedDiscovery } from '../src/discovery.js'
import type { DmedDevice } from '../src/types.js'

class TestableDiscovery extends DmedDiscovery {
  expose_addDevice(d: DmedDevice) { this.addDevice(d) }
  expose_removeDevice(key: string) { this.removeDevice(key) }
}

function makeDevice(instanceId: string, via: 'ble' | 'mdns' = 'mdns'): DmedDevice {
  return {
    instanceId,
    name: `Device ${instanceId}`,
    discoveredVia: via,
    address: via === 'mdns' ? '192.168.1.1:3100' : 'AA:BB:CC:DD:EE:FF',
    httpHost: via === 'mdns' ? '192.168.1.1' : undefined,
    httpPort: via === 'mdns' ? 3100 : undefined
  }
}

describe('DmedDiscovery', () => {
  it('fires device event when device added', () => {
    const discovery = new TestableDiscovery()
    const seen: DmedDevice[] = []
    discovery.on('device', d => seen.push(d))
    discovery.expose_addDevice(makeDevice('abc'))
    expect(seen).toHaveLength(1)
    expect(seen[0].instanceId).toBe('abc')
  })

  it('does not fire duplicate device event for same instanceId', () => {
    const discovery = new TestableDiscovery()
    const seen: DmedDevice[] = []
    discovery.on('device', d => seen.push(d))
    discovery.expose_addDevice(makeDevice('abc'))
    discovery.expose_addDevice(makeDevice('abc'))  // duplicate
    expect(seen).toHaveLength(1)
  })

  it('fires lost event when device removed', () => {
    const discovery = new TestableDiscovery()
    const lost: DmedDevice[] = []
    discovery.on('lost', d => lost.push(d))
    discovery.expose_addDevice(makeDevice('abc'))
    discovery.expose_removeDevice('abc')
    expect(lost).toHaveLength(1)
  })

  it('getDevices returns all current devices', () => {
    const discovery = new TestableDiscovery()
    discovery.expose_addDevice(makeDevice('a'))
    discovery.expose_addDevice(makeDevice('b'))
    expect(discovery.getDevices()).toHaveLength(2)
  })

  it('getDevices excludes removed devices', () => {
    const discovery = new TestableDiscovery()
    discovery.expose_addDevice(makeDevice('a'))
    discovery.expose_addDevice(makeDevice('b'))
    discovery.expose_removeDevice('a')
    const devices = discovery.getDevices()
    expect(devices).toHaveLength(1)
    expect(devices[0].instanceId).toBe('b')
  })

  it('two devices with different instanceIds both appear', () => {
    const discovery = new TestableDiscovery()
    const seen: DmedDevice[] = []
    discovery.on('device', d => seen.push(d))
    discovery.expose_addDevice(makeDevice('x'))
    discovery.expose_addDevice(makeDevice('y'))
    expect(seen).toHaveLength(2)
  })
})
```

---

## P2.3 — Python library tests

**New directory:** `lib/python/tests/`

**New file:** `lib/python/tests/test_card.py`

```python
import pytest
import json
from dmed import DmedCard, DmedClient

VALID_CARD = {
    "dmed_version": "0.2.0",
    "instance_id": "c0ffee01",
    "name": "Smart Coffee Machine",
    "description": "Brews coffee",
    "service_type": "iot_device",
    "transports": [{"type": "http", "url": "http://localhost:3100/mcp", "priority": 1}],
    "auth": {"type": "none"},
    "capabilities": {
        "tools": [
            {"name": "brew_coffee", "description": "Brew coffee"},
            {"name": "get_status", "description": "Get status"}
        ]
    }
}

def test_card_from_dict():
    card = DmedCard.from_dict(VALID_CARD)
    assert card.name == "Smart Coffee Machine"
    assert len(card.capabilities["tools"]) == 2

def test_card_from_json():
    card = DmedCard.from_json(json.dumps(VALID_CARD))
    assert card.instance_id == "c0ffee01"

def test_card_missing_required_field():
    bad = {**VALID_CARD}
    del bad["instance_id"]
    with pytest.raises(ValueError, match="instance_id"):
        DmedCard.from_dict(bad)

def test_card_tool_names():
    card = DmedCard.from_dict(VALID_CARD)
    names = [t["name"] for t in card.capabilities["tools"]]
    assert "brew_coffee" in names
    assert "get_status" in names

def test_card_tolerates_extra_fields():
    extended = {**VALID_CARD, "future_field": "ignored"}
    card = DmedCard.from_dict(extended)  # should not raise
    assert card.name == "Smart Coffee Machine"
```

Add `pytest` to `pyproject.toml` optional dependencies:
```toml
[project.optional-dependencies]
test = ["pytest>=7.0"]
```

---

### Phase 2 Definition of Done

- [ ] `lib/js/` builds with `npm run build` — no TypeScript errors
- [ ] `npm test` in `lib/js/` → all green
- [ ] `pytest lib/python/` → all green
- [ ] Android scanner refactored to match SDK contracts — `./gradlew test` still green
- [ ] Discovery Framework spec document updated in `spec/`

---

# Phase 3 — MCP Gateway

**Goal:** A local process that discovers DMED devices and exposes them as MCP tools to remote Claude.

---

## P3.1 — Scaffold gateway

**New directory:** `gateway/`

```
gateway/
  src/
    index.ts          — entry point
    discovery.ts      — DMED discovery (uses lib/js)
    tool-mapper.ts    — DmedCard → MCP tool definitions
    proxy.ts          — MCP tool call → DMED dispatch
    auth.ts           — API key middleware
    server.ts         — MCP HTTP server
  tests/
    tool-mapper.test.ts
    proxy.test.ts
    auth.test.ts
    gateway.test.ts
  config.json.example
  package.json
  tsconfig.json
  README.md
```

---

## P3.2 — Tool mapper

**File:** `gateway/src/tool-mapper.ts`

Converts a DMED device's capability card into MCP tool definitions.

**Slug rules:** device name → lowercase, spaces/special chars → `_`, max 32 chars.
**Tool name:** `{device_slug}__{action_name}` (double underscore separator).

---

## P3.3 — Tool mapper tests

**File:** `gateway/tests/tool-mapper.test.ts`

```typescript
describe('slugify', () => {
  ✓ "Smart Coffee Machine" → "smart_coffee_machine"
  ✓ "Device #1 (test)" → "device_1_test"
  ✓ already lowercase with underscores → unchanged
  ✓ name longer than 32 chars → truncated
})

describe('toolsFromCard', () => {
  ✓ device with 3 actions → 3 MCP tools
  ✓ tool name = "{device_slug}__{action_name}"
  ✓ tool description = "{action.description} ({device.name})"
  ✓ tool inputSchema copied verbatim from card
  ✓ empty tools list → empty array (no error)
  ✓ two devices same action name → different prefixes, no collision
})
```

---

## P3.4 — Proxy tests

**File:** `gateway/tests/proxy.test.ts`

```typescript
describe('resolveToolCall', () => {
  ✓ "coffee_machine__brew_coffee" → device "coffee_machine", action "brew_coffee"
  ✓ unknown tool name → throws ToolNotFoundError
  ✓ tool with no registered device → throws DeviceNotFoundError
})

describe('dispatchToolCall', () => {
  ✓ valid call → dispatches to correct device + action
  ✓ params passed through unchanged
  ✓ ok response → MCP success result
  ✓ error response → MCP error result
  ✓ device offline (network error) → MCP error, gateway stays up
})
```

---

## P3.5 — Auth tests

**File:** `gateway/tests/auth.test.ts`

```typescript
describe('apiKeyAuth middleware', () => {
  ✓ request with correct X-API-Key header → passes through
  ✓ request with wrong key → 401
  ✓ request with no key → 401
  ✓ key loaded from config file, not hardcoded
  ✓ missing config file → server refuses to start
})
```

---

## P3.6 — Integration tests

**File:** `gateway/tests/gateway.test.ts`

Run against the live coffee machine server on localhost.

```typescript
describe('Gateway integration (requires npm start in smart-coffee-machine/)', () => {
  ✓ gateway starts, devices discovered via mDNS
  ✓ MCP tools/list includes coffee_machine__brew_coffee
  ✓ MCP tools/list includes coffee_machine__get_status
  ✓ MCP tools/call brew_coffee → action dispatched → result returned
  ✓ MCP tools/call get_status → returns machine state
  ✓ MCP initialize → returns server name "DMED Gateway"
})
```

---

### Phase 3 Definition of Done

- [ ] `npm test` in `gateway/` → all unit tests green
- [ ] Integration tests pass against live coffee machine server
- [ ] Works with Claude Desktop configured as MCP client
- [ ] `gateway/README.md` has setup instructions
- [ ] API key auth blocks unauthorized requests

---

# Phase 4 — v1.0

**Goal:** harden the gateway for safe WAN exposure, give third-party implementers a
self-check tool, align with the ratified MCP Server Cards discovery spec, and document a
real deployment target.

---

## P4.1 — Config schema: TLS + allowlists

**Files:** `gateway/src/types.ts`, `gateway/src/auth.ts`

Added `TlsConfig { cert, key }` and `AllowlistConfig { devices?, actions? }` to
`GatewayConfig`. `loadConfig` validates the cert/key files exist at startup when `tls` is
present, so misconfiguration fails fast instead of on the first request. No allowlist
configured = allow-all, preserving Phase 3 behavior for LAN-only use.

## P4.2 — Allowlist enforcement

**Files:** `gateway/src/allowlist.ts` (new), `gateway/src/tool-mapper.ts`

`isDeviceAllowed()` / `isActionAllowed()` are pure functions over `AllowlistConfig`, wired
into `registerDevice()`. A non-allowlisted device never enters the registry at all — it's
absent from `tools/list` and any `tools/call` on it resolves as `ToolNotFoundError`.
`allowlist.actions` further narrows an allowlisted device to specific action names.

## P4.3 — TLS support

**File:** `gateway/src/listen.ts` (new)

`startListening()` picks `https.createServer` (given `config.tls`) or plain `app.listen`.
The `https` factory is dependency-injected so unit tests don't need real certs; verified
separately end-to-end with an actual self-signed cert over real TLS 1.3.

## P4.4 — API key hardening

**File:** `gateway/src/auth.ts`

`X-API-Key` comparison hashes both sides (SHA-256) before `crypto.timingSafeEqual`, so
comparison time no longer leaks how many characters of a guess matched — a fixed-length
digest sidesteps the length-mismatch branch a naive constant-time compare would still leak.

## P4.5 — Conformance CLI: `dmed-conform`

**Files:** `lib/js/src/conformance.ts`, `lib/js/src/cli.ts` (both new)

Added to `lib/js` (it only exercises the SDK's own `parseCard`/`fetchCard`) rather than a
new package. Non-mutating by default: validates `/dmed/card`, required fields, tool
well-formedness, `/dmed/actions` count matches `capabilities.tools`, correct 400/404 on
missing/unknown actions, and `transports` shape. `--call <action>` opts into one live
dispatch; `--json` for CI. Tested against in-process fixture servers (good and deliberately
broken), not `examples/`, keeping `lib/js` self-contained.

## P4.6 — MCP Server Cards alignment (SEP-1649)

**Files:** `gateway/src/server-card.ts`, `gateway/src/constants.ts` (both new)

Unauthenticated `GET /.well-known/mcp/server-card.json`, registered before the
`apiKeyAuth` middleware — discovery has to work before a client has credentials, and the
spec forbids secrets in this document. The transport endpoint is built from the request's
actual protocol/host, so it's correct behind a reverse proxy without hardcoding a public
hostname. SEP-1960 (a separate, less-mature `.well-known/mcp` manifest proposal) is out of
scope until it stabilizes.

## P4.7 — Raspberry Pi deployment guide

**File:** `docs/deployment/raspberry-pi.md` (new)

Node setup, building `gateway` plus its `lib/js` dependency, a systemd unit, the
mDNS-needs-same-L2-segment caveat, two WAN-exposure paths (reverse-proxy TLS vs the
gateway's built-in TLS), and a security checklist tying together allowlist/TLS/API-key.

---

### Phase 4 Definition of Done

- [x] Device/action allowlists enforced, tested (default allow-all preserves Phase 3 behavior)
- [x] Gateway can run over TLS from a local cert/key; timing-safe API key comparison
- [x] `dmed-conform <url>` runs and passes against smart-coffee-machine; correctly fails on a broken card fixture
- [x] Gateway serves `.well-known/mcp/server-card.json` unauthenticated, spec-shaped, no secrets
- [x] Raspberry Pi guide published in `docs/`
- [x] All new code covered by tests — `lib/js` 25/25, `gateway` 58/58

---

# Summary Table

| ID | Task | Phase | New Files | Modified Files |
|---|---|---|---|---|
| P0.1 | Delete session JSON | 0 | — | — |
| P0.2 | Commit untracked work | 0 | — | — |
| P0.3 | Coffee machine test runner | 0 | `server.test.js` | `server.js`, `package.json` |
| P0.4 | Android test infra | 0 | `PlaceholderTest.kt` | `build.gradle.kts`, `AndroidManifest.xml` |
| P1.1 | Update Models | 1 | — | `Models.kt` |
| P1.2 | HTTP Card Reader | 1 | `HttpCardReader.kt` | — |
| P1.3 | HTTP Action Client | 1 | `HttpActionClient.kt` | — |
| P1.4 | BLE DMED-only filter | 1 | — | `DmedDiscovery.kt` |
| P1.5 | mDNS discovery (NSD) | 1 | — | `DmedDiscovery.kt` |
| P1.6 | Route by transport | 1 | — | `MainViewModel.kt` |
| P1.7 | AI Brain | 1 | `AiBrain.kt` | — |
| P1.8 | Wire brain into ViewModel | 1 | — | `MainViewModel.kt` |
| P1.9 | Update ChatScreen | 1 | — | `ChatScreen.kt` |
| P1.10 | API key in local.properties | 1 | — | `local.properties` |
| P1.11 | Server tests (full suite) | 1 | — | `server.test.js` |
| P1.12 | Android unit tests | 1 | 4 test files | — |
| P1.13 | Demo checklist | 1 | `demo-checklist.md` | — |
| P2.1 | lib/js scaffold + implementation | 2 | 6 source files | — |
| P2.2 | lib/js tests | 2 | 3 test files | — |
| P2.3 | Python tests | 2 | `test_card.py` | `pyproject.toml` |
| P3.1 | Gateway scaffold | 3 | directory + 5 src files | — |
| P3.2 | Tool mapper | 3 | `tool-mapper.ts` | — |
| P3.3–P3.6 | Gateway tests | 3 | 4 test files | — |
