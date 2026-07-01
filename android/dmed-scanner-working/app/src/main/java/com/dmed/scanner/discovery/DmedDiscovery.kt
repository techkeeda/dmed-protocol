package com.dmed.scanner.discovery

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.os.ParcelUuid
import com.dmed.scanner.data.DmedEndpoint
import com.dmed.scanner.data.TransportType
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import java.util.UUID

@SuppressLint("MissingPermission")
class DmedDiscovery(context: Context) {

    companion object {
        val DMED_BEACON_SERVICE_UUID: UUID = UUID.fromString("D14D0000-1000-4000-8000-00805F9B34FB")
        val DMED_CARD_SERVICE_UUID: UUID = UUID.fromString("D14D0001-1000-4000-8000-00805F9B34FB")
        val DMED_CARD_DATA_UUID: UUID = UUID.fromString("D14D0002-1000-4000-8000-00805F9B34FB")
        val DMED_CARD_LENGTH_UUID: UUID = UUID.fromString("D14D0003-1000-4000-8000-00805F9B34FB")
    }

    private val bluetoothManager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val scanner: BluetoothLeScanner? get() = bluetoothManager.adapter?.bluetoothLeScanner
    private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private var discoveryListener: NsdManager.DiscoveryListener? = null

    private val _endpoints = MutableStateFlow<List<DmedEndpoint>>(emptyList())
    val endpoints: StateFlow<List<DmedEndpoint>> = _endpoints
    private val _isScanning = MutableStateFlow(false)
    val isScanning: StateFlow<Boolean> = _isScanning

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val record = result.scanRecord ?: return

            if (!isDmedDevice(record)) return

            val device = result.device
            val name = record.deviceName ?: device.name ?: return

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
                discoveredVia = TransportType.BLE
            )

            val current = _endpoints.value.toMutableList()
            if (current.none { it.address == endpoint.address }) {
                current.add(endpoint)
                _endpoints.value = current
            }
        }

        override fun onScanFailed(errorCode: Int) {
            _isScanning.value = false
            val errMsg = when (errorCode) {
                1 -> "SCAN_FAILED_ALREADY_STARTED"
                2 -> "SCAN_FAILED_APPLICATION_REGISTRATION_FAILED"
                3 -> "SCAN_FAILED_INTERNAL_ERROR"
                4 -> "SCAN_FAILED_FEATURE_UNSUPPORTED"
                else -> "SCAN_FAILED_UNKNOWN ($errorCode)"
            }
            _endpoints.value = listOf(DmedEndpoint(name = "⚠️ Scan Failed: $errMsg", address = "error"))
        }
    }

    private val DMED_UUID_LE = byteArrayOf(
        0xFB.toByte(), 0x34, 0x9B.toByte(), 0x5F, 0x80.toByte(), 0x00, 0x00, 0x80.toByte(),
        0x00, 0x40, 0x00, 0x10, 0x00, 0x00, 0x4D, 0xD1.toByte()
    )

    private fun isDmedDevice(record: ScanRecord): Boolean {
        val uuids = record.serviceUuids
        if (uuids != null && uuids.any { it.uuid == DMED_BEACON_SERVICE_UUID }) return true
        val bytes = record.bytes ?: return false
        val raw = bytes.toList()
        for (i in 0..raw.size - 16) {
            if (raw.subList(i, i + 16) == DMED_UUID_LE.toList()) return true
        }
        return false
    }

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

    fun startScan() {
        if (_isScanning.value) return
        _endpoints.value = emptyList()
        _isScanning.value = true

        val s = scanner
        if (s != null) {
            s.startScan(scanCallback)
        }

        startMdnsScan()
    }

    fun stopScan() {
        stopMdnsScan()
        try { scanner?.stopScan(scanCallback) } catch (_: Exception) {}
        _isScanning.value = false
    }
}
