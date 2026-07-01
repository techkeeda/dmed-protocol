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
