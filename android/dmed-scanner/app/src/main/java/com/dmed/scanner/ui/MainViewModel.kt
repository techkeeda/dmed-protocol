package com.dmed.scanner.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.dmed.scanner.data.*
import com.dmed.scanner.discovery.DmedDiscovery
import com.dmed.scanner.network.BleActionClient
import com.dmed.scanner.network.BleCardReader
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class MainViewModel(app: Application) : AndroidViewModel(app) {
    val discovery = DmedDiscovery(app)
    private val bleCardReader = BleCardReader(app)
    private val bleActionClient = BleActionClient(app)

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
            _messages.value = listOf(ChatMessage("🔗 Connecting via BLE to read card...", false))

            val card = bleCardReader.readCard(endpoint.address)
            if (card != null) {
                endpoint.card = card
                _actions.value = card.capabilities.tools
                _messages.value = listOf(
                    ChatMessage("Connected to ${card.name}\n${card.description}\n\n📡 All communication over BLE\n\nAvailable actions:", false),
                    ChatMessage(card.capabilities.tools.joinToString("\n") { "• ${it.name} — ${it.description}" }, false)
                )
            } else {
                val err = bleCardReader.debugError.ifEmpty { "no details" }
                _messages.value = listOf(ChatMessage("❌ Could not read capability card via BLE\n\n$err", false))
            }
        }
    }

    fun sendAction(actionName: String, params: Map<String, Any> = emptyMap()) {
        val endpoint = _selectedEndpoint.value ?: return
        val current = _messages.value.toMutableList()
        val displayText = if (params.isEmpty()) actionName else "$actionName(${params.entries.joinToString { "${it.key}=${it.value}" }})"
        current.add(ChatMessage(displayText, true))
        _messages.value = current

        viewModelScope.launch {
            val resp = bleActionClient.sendAction(endpoint.address, actionName, params)
            val reply = when {
                resp == null -> "❌ No response from device (BLE)"
                resp.status == "ok" -> resp.result?.text ?: "✅ Done"
                else -> "⚠️ ${resp.message ?: "Error"}"
            }
            val updated = _messages.value.toMutableList()
            updated.add(ChatMessage(reply, false))
            _messages.value = updated
        }
    }

    fun clearSelection() {
        _selectedEndpoint.value = null
        _messages.value = emptyList()
        _actions.value = emptyList()
    }
}
