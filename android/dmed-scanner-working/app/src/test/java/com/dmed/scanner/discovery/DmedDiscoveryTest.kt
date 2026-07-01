package com.dmed.scanner.discovery

import com.dmed.scanner.data.DmedEndpoint
import com.dmed.scanner.data.TransportType
import org.junit.Assert.*
import org.junit.Test

class DmedDiscoveryTest {

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
    fun dmedCardDataUuidIsCorrect() {
        val uuid = DmedDiscovery.DMED_CARD_DATA_UUID
        assertEquals("d14d0002-1000-4000-8000-00805f9b34fb", uuid.toString())
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

    @Test
    fun emptyInstanceIdDoesNotDeduplicateByDefault() {
        val ep1 = DmedEndpoint(name = "Device1", address = "AA:BB:CC:DD:EE:FF", instanceId = "")
        val ep2 = DmedEndpoint(name = "Device2", address = "11:22:33:44:55:66", instanceId = "")

        val list = mutableListOf(ep1)
        val alreadyPresent = ep2.instanceId.isNotEmpty() &&
            list.any { it.instanceId == ep2.instanceId }
        assertFalse("Empty instanceId should not trigger deduplication", alreadyPresent)
    }

    @Test
    fun defaultTransportTypeIsBle() {
        val endpoint = DmedEndpoint(name = "Test", address = "AA:BB:CC:DD:EE:FF")
        assertEquals(TransportType.BLE, endpoint.discoveredVia)
    }
}
