package com.dmed.scanner.network

import com.dmed.scanner.data.ActionResponse
import com.google.gson.Gson
import org.junit.Assert.*
import org.junit.Test

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

    @Test
    fun actionRequestSerialization() {
        val request = mapOf("action" to "brew_coffee", "params" to mapOf("drink_type" to "latte", "size" to "large"))
        val json = gson.toJson(request)
        assertTrue(json.contains("brew_coffee"))
        assertTrue(json.contains("drink_type"))
        assertTrue(json.contains("latte"))
    }

    @Test
    fun emptyParamsSerialization() {
        val request = mapOf("action" to "get_status", "params" to emptyMap<String, Any>())
        val json = gson.toJson(request)
        assertTrue(json.contains("get_status"))
        assertTrue(json.contains("params"))
    }

    @Test
    fun actionResponseWithAllFields() {
        val json = """{"status":"ok","action":"set_temperature","result":{"text":"Temperature set to 94°C"}}"""
        val resp = gson.fromJson(json, ActionResponse::class.java)
        assertEquals("ok", resp.status)
        assertEquals("set_temperature", resp.action)
        assertEquals("Temperature set to 94°C", resp.result?.text)
        assertNull(resp.message)
    }
}
