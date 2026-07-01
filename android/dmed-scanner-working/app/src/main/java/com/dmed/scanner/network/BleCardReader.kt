package com.dmed.scanner.network

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.os.Build
import com.dmed.scanner.data.DmedCard
import com.dmed.scanner.discovery.DmedDiscovery.Companion.DMED_CARD_DATA_UUID
import com.dmed.scanner.discovery.DmedDiscovery.Companion.DMED_CARD_SERVICE_UUID
import com.google.gson.Gson
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import java.util.UUID

@SuppressLint("MissingPermission")
class BleCardReader(private val context: Context) {

    companion object {
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    private val gson = Gson()
    var debugError: String = ""
        private set

    suspend fun readCard(address: String): DmedCard? = withContext(Dispatchers.IO) {
        withTimeoutOrNull(20_000L) {
            val adapter = BluetoothAdapter.getDefaultAdapter() ?: return@withTimeoutOrNull null
            val device = adapter.getRemoteDevice(address)
            val result = CompletableDeferred<DmedCard?>()

            val callback = object : BluetoothGattCallback() {
                private var gatt: BluetoothGatt? = null

                override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
                    gatt = g
                    if (newState == BluetoothProfile.STATE_CONNECTED) {
                        // Clear GATT cache immediately on connect
                        try {
                            val refresh = g.javaClass.getMethod("refresh")
                            refresh.invoke(g)
                        } catch (_: Exception) {}
                        Thread.sleep(500)
                        g.requestMtu(512)
                    } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                        if (!result.isCompleted) result.complete(null)
                    }
                }

                override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
                    g.discoverServices()
                }

                override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                    if (status != BluetoothGatt.GATT_SUCCESS) {
                        debugError = "Service discovery failed status=$status"
                        finish(null); return
                    }
                    // Log all services
                    val svcList = g.services.joinToString(", ") { it.uuid.toString().take(8) }
                    debugError = "Services: $svcList"

                    val service = g.getService(DMED_CARD_SERVICE_UUID)
                    if (service == null) {
                        debugError += "\n→ DMED svc NOT found"
                        finish(null); return
                    }
                    val chr = service.getCharacteristic(DMED_CARD_DATA_UUID)
                    if (chr == null) {
                        debugError += "\n→ DMED chr NOT found"
                        finish(null); return
                    }
                    // Enable notifications
                    g.setCharacteristicNotification(chr, true)
                    val desc = chr.getDescriptor(CCCD_UUID)
                    if (desc != null) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            g.writeDescriptor(desc, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE)
                        } else {
                            @Suppress("DEPRECATION")
                            desc.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                            @Suppress("DEPRECATION")
                            g.writeDescriptor(desc)
                        }
                    } else {
                        // No CCCD, try writing directly
                        writeCardRequest(g, chr)
                    }
                }

                override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
                    // Small delay to ensure notification subscription is active
                    Thread.sleep(500)
                    val service = g.getService(DMED_CARD_SERVICE_UUID) ?: return
                    val chr = service.getCharacteristic(DMED_CARD_DATA_UUID) ?: return
                    writeCardRequest(g, chr)
                }

                private fun writeCardRequest(g: BluetoothGatt, chr: BluetoothGattCharacteristic) {
                    val cmd = """{"cmd":"card"}""".toByteArray(Charsets.UTF_8)
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        g.writeCharacteristic(chr, cmd, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
                    } else {
                        @Suppress("DEPRECATION")
                        chr.value = cmd
                        chr.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                        @Suppress("DEPRECATION")
                        g.writeCharacteristic(chr)
                    }
                }

                override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
                    debugError += "\nWrite done status=$status, waiting for notify..."
                }

                // Notification callback - Android 13+
                override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                    handleNotification(value)
                }

                // Notification callback - pre-Android 13
                @Suppress("DEPRECATION")
                @Deprecated("Deprecated in Java")
                override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                        handleNotification(characteristic.value ?: byteArrayOf())
                    }
                }

                private fun handleNotification(value: ByteArray) {
                    val json = String(value, Charsets.UTF_8)
                    debugError = "Got ${value.size}b via notify"
                    val card = try {
                        gson.fromJson(json, DmedCard::class.java)
                    } catch (e: Exception) {
                        debugError = "Parse err: ${e.message}\nJSON: ${json.take(200)}"
                        null
                    }
                    finish(card)
                }

                private fun finish(card: DmedCard?) {
                    gatt?.disconnect()
                    gatt?.close()
                    if (!result.isCompleted) result.complete(card)
                }
            }

            device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
            result.await()
        }
    }
}
