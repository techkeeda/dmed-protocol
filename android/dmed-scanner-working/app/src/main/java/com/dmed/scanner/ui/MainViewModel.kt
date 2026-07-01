package com.dmed.scanner.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.dmed.scanner.BuildConfig
import com.dmed.scanner.ai.AiBrain
import com.dmed.scanner.data.*
import com.dmed.scanner.discovery.DmedDiscovery
import com.dmed.scanner.network.BleActionClient
import com.dmed.scanner.network.BleCardReader
import com.dmed.scanner.network.HttpActionClient
import com.dmed.scanner.network.HttpCardReader
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class MainViewModel(app: Application) : AndroidViewModel(app) {
    val discovery = DmedDiscovery(app)
    private val bleCardReader = BleCardReader(app)
    private val bleActionClient = BleActionClient(app)
    private val httpCardReader = HttpCardReader()
    private val httpActionClient = HttpActionClient()
    private val aiBrain = AiBrain(BuildConfig.CLAUDE_API_KEY)
    private val aiHistory = mutableListOf<AiBrain.ApiMessage>()

    private val _selectedEndpoint = MutableStateFlow<DmedEndpoint?>(null)
    val selectedEndpoint: StateFlow<DmedEndpoint?> = _selectedEndpoint

    private val _actions = MutableStateFlow<List<Tool>>(emptyList())
    val actions: StateFlow<List<Tool>> = _actions

    private val _messages = MutableStateFlow<List<ChatMessage>>(emptyList())
    val messages: StateFlow<List<ChatMessage>> = _messages

    fun startScan() = discovery.startScan()
    fun stopScan() = discovery.stopScan()

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

    fun processMessage(userInput: String) {
        val endpoint = _selectedEndpoint.value ?: return
        val card = endpoint.card ?: return

        val current = _messages.value.toMutableList()
        current.add(ChatMessage(userInput, true))
        current.add(ChatMessage("…", false))
        _messages.value = current

        viewModelScope.launch {
            val response = try {
                aiBrain.chat(card, aiHistory.toList(), userInput)
            } catch (e: Exception) {
                AiResponse("❌ ${e.message ?: "Claude error"}")
            }

            if (response.toolCall != null) {
                val actionResp = dispatch(endpoint, response.toolCall.name, response.toolCall.input)
                val rawResult = when {
                    actionResp == null -> "No response from device"
                    actionResp.status == "ok" -> actionResp.result?.text ?: "Done"
                    else -> actionResp.message ?: "Error"
                }

                val narration = try {
                    aiBrain.narrateResult(card, aiHistory.toList(), userInput, response.toolCall, rawResult)
                } catch (_: Exception) {
                    rawResult
                }

                aiHistory.add(AiBrain.ApiMessage("user", userInput))
                aiHistory.add(AiBrain.ApiMessage("assistant", narration))

                val updated = _messages.value.toMutableList()
                updated[updated.lastIndex] = ChatMessage(narration, false)
                _messages.value = updated
            } else {
                aiHistory.add(AiBrain.ApiMessage("user", userInput))
                aiHistory.add(AiBrain.ApiMessage("assistant", response.message))

                val updated = _messages.value.toMutableList()
                updated[updated.lastIndex] = ChatMessage(response.message, false)
                _messages.value = updated
            }
        }
    }

    fun clearSelection() {
        _selectedEndpoint.value = null
        _messages.value = emptyList()
        _actions.value = emptyList()
        aiHistory.clear()
    }
}
