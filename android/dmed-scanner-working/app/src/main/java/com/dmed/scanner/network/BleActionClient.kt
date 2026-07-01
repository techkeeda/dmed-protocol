package com.dmed.scanner.network

import android.annotation.SuppressLint
import android.bluetooth.*
import android.content.Context
import android.os.Build
import com.dmed.scanner.data.ActionResponse
import com.dmed.scanner.data.ActionResult
import com.dmed.scanner.discovery.DmedDiscovery.Companion.DMED_CARD_DATA_UUID
import com.dmed.scanner.discovery.DmedDiscovery.Companion.DMED_CARD_SERVICE_UUID
import com.google.gson.Gson
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import java.util.UUID

@SuppressLint("MissingPermission")
class BleActionClient(private val context: Context) {

    companion object {
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    }

    private val gson = Gson()

    suspend fun sendAction(address: String, action: String, params: Map<String, Any> = emptyMap()): ActionResponse? =
        withContext(Dispatchers.IO) {
            withTimeoutOrNull(15_000L) {
                val adapter = BluetoothAdapter.getDefaultAdapter() ?: return@withTimeoutOrNull null
                val device = adapter.getRemoteDevice(address)
                val result = CompletableDeferred<ActionResponse?>()

                val callback = object : BluetoothGattCallback() {
                    private var gatt: BluetoothGatt? = null

                    override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
                        gatt = g
                        if (newState == BluetoothProfile.STATE_CONNECTED) {
                            g.requestMtu(512)
                        } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                            if (!result.isCompleted) result.complete(null)
                        }
                    }

                    override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
                        g.discoverServices()
                    }

                    override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                        if (status != BluetoothGatt.GATT_SUCCESS) { finish(null); return }
                        val service = g.getService(DMED_CARD_SERVICE_UUID) ?: run { finish(null); return }
                        val chr = service.getCharacteristic(DMED_CARD_DATA_UUID) ?: run { finish(null); return }

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
                            writeAction(g, chr)
                        }
                    }

                    override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
                        val service = g.getService(DMED_CARD_SERVICE_UUID) ?: return
                        val chr = service.getCharacteristic(DMED_CARD_DATA_UUID) ?: return
                        writeAction(g, chr)
                    }

                    private fun writeAction(g: BluetoothGatt, chr: BluetoothGattCharacteristic) {
                        val json = gson.toJson(mapOf("cmd" to "action", "action" to action, "params" to params))
                        val data = json.toByteArray(Charsets.UTF_8)
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            g.writeCharacteristic(chr, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
                        } else {
                            @Suppress("DEPRECATION")
                            chr.value = data
                            chr.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                            @Suppress("DEPRECATION")
                            g.writeCharacteristic(chr)
                        }
                    }

                    override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
                        handleNotification(value)
                    }

                    @Suppress("DEPRECATION")
                    @Deprecated("Deprecated in Java")
                    override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
                        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
                            handleNotification(characteristic.value ?: byteArrayOf())
                        }
                    }

                    private fun handleNotification(value: ByteArray) {
                        val json = String(value, Charsets.UTF_8)
                        val resp = try { gson.fromJson(json, ActionResponse::class.java) } catch (_: Exception) {
                            ActionResponse(status = "ok", action = action, result = ActionResult(text = json))
                        }
                        finish(resp)
                    }

                    private fun finish(resp: ActionResponse?) {
                        gatt?.disconnect()
                        gatt?.close()
                        if (!result.isCompleted) result.complete(resp)
                    }
                }

                device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
                result.await()
            }
        }
}
