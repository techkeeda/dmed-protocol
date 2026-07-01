package com.dmed.scanner.ui

import com.dmed.scanner.data.*
import org.junit.Assert.*
import org.junit.Test

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
    fun chatMessageUserFlag() {
        val user = ChatMessage("Make a latte", isUser = true)
        val bot = ChatMessage("Brewing now!", isUser = false)
        assertTrue(user.isUser)
        assertFalse(bot.isUser)
    }

    @Test
    fun dmedEndpointDefaultsToBlue() {
        val ep = DmedEndpoint(name = "Test", address = "AA:BB:CC:DD:EE:FF")
        assertEquals(TransportType.BLE, ep.discoveredVia)
        assertNull(ep.httpHost)
        assertEquals(8080, ep.httpPort)
    }

    @Test
    fun toolListFromCard() {
        val card = makeCard()
        assertEquals(2, card.capabilities.tools.size)
        assertEquals("brew_coffee", card.capabilities.tools[0].name)
        assertEquals("get_status", card.capabilities.tools[1].name)
    }

    @Test
    fun cardAssignedToEndpoint() {
        val ep = makeEndpoint()
        assertNull(ep.card)
        ep.card = makeCard()
        assertNotNull(ep.card)
        assertEquals("Smart Coffee Machine", ep.card!!.name)
    }

    @Test
    fun actionResponseSuccess() {
        val resp = ActionResponse(
            status = "ok",
            action = "brew_coffee",
            result = ActionResult("☕ Your large latte is ready!")
        )
        assertEquals("ok", resp.status)
        assertEquals("☕ Your large latte is ready!", resp.result?.text)
        assertNull(resp.message)
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
        assertNull(resp.result)
    }

    @Test
    fun mdnsEndpointHasHttpFields() {
        val ep = makeEndpoint(TransportType.MDNS)
        assertEquals(TransportType.MDNS, ep.discoveredVia)
        assertEquals("192.168.1.42", ep.httpHost)
        assertEquals(3100, ep.httpPort)
    }

    @Test
    fun thinkingIndicatorText() {
        val thinking = ChatMessage("…", isUser = false)
        assertEquals("…", thinking.text)
        assertFalse(thinking.isUser)
    }

    @Test
    fun transportEntryParsed() {
        val entry = TransportEntry(type = "http", url = "http://192.168.1.42:3100/mcp", priority = 1)
        assertEquals("http", entry.type)
        assertEquals("http://192.168.1.42:3100/mcp", entry.url)
        assertEquals(1, entry.priority)
    }
}
