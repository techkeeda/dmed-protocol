package com.dmed.scanner.data

data class DmedEndpoint(
    val name: String,
    val address: String, // BLE MAC address
    val icon: String = "📡",
    val description: String = "",
    val instanceId: String = "",
    val serviceType: Int = 0,
    var card: DmedCard? = null
)

data class DmedCard(
    val dmed_version: String,
    val instance_id: String,
    val name: String,
    val description: String,
    val service_type: String,
    val capabilities: Capabilities,
    val metadata: Map<String, String>?,
    val transport: Transport? = null
)

data class Transport(
    val http: HttpTransport? = null
)

data class HttpTransport(
    val host: String,
    val port: Int
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
