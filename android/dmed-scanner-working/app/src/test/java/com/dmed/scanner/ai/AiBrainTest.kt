package com.dmed.scanner.ai

import com.dmed.scanner.data.*
import org.junit.Assert.*
import org.junit.Test

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
        val prompt = brain.buildSystemPrompt(makeCard())
        assertTrue("Prompt should contain device name", prompt.contains("Smart Coffee Machine"))
    }

    @Test
    fun systemPromptContainsDeviceDescription() {
        val prompt = brain.buildSystemPrompt(makeCard())
        assertTrue("Prompt should contain description", prompt.contains("Brews coffee on demand"))
    }

    @Test
    fun systemPromptIsNotBlank() {
        val prompt = brain.buildSystemPrompt(makeCard())
        assertTrue(prompt.isNotBlank())
    }

    @Test
    fun toolCallHoldsCorrectValues() {
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
    fun aiResponseTextOnly() {
        val resp = AiResponse(message = "I can brew espresso, latte, and americano.")
        assertEquals("I can brew espresso, latte, and americano.", resp.message)
        assertNull(resp.toolCall)
    }

    @Test
    fun aiResponseWithToolCall() {
        val resp = AiResponse(
            message = "",
            toolCall = ToolCall("id1", "brew_coffee", mapOf("drink_type" to "latte", "size" to "large"))
        )
        assertNotNull(resp.toolCall)
        assertEquals("brew_coffee", resp.toolCall!!.name)
        assertEquals("latte", resp.toolCall!!.input["drink_type"])
    }

    @Test
    fun systemPromptDifferentForDifferentDevices() {
        val coffeeCard = makeCard()
        val phoneCard = DmedCard(
            dmed_version = "0.2.0",
            instance_id = "ph0ne01",
            name = "VoIP Phone",
            description = "Makes and receives calls",
            service_type = "communication",
            capabilities = Capabilities(emptyList()),
            metadata = null
        )
        val coffeePrompt = brain.buildSystemPrompt(coffeeCard)
        val phonePrompt = brain.buildSystemPrompt(phoneCard)
        assertNotEquals(coffeePrompt, phonePrompt)
        assertTrue(phonePrompt.contains("VoIP Phone"))
        assertTrue(phonePrompt.contains("Makes and receives calls"))
    }
}
