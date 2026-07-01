package com.dmed.scanner.data

enum class TransportType { BLE, MDNS }

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
    val metadata: Map<String, String>? = null,
    val transports: List<TransportEntry>? = null,
    val auth: Map<String, String>? = null
)

data class Capabilities(
    val tools: List<Tool>
)

data class Tool(
    val name: String,
    val description: String,
    val inputSchema: Map<String, Any>? = null
)

data class ActionResponse(
    val status: String,
    val action: String,
    val result: ActionResult? = null,
    val message: String? = null
)

data class ActionResult(
    val text: String
)

data class ChatMessage(
    val text: String,
    val isUser: Boolean
)

data class ToolCall(
    val id: String,
    val name: String,
    val input: Map<String, Any>
)

data class AiResponse(
    val message: String,
    val toolCall: ToolCall? = null
)
