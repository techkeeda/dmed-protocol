package com.dmed.scanner.network

import com.dmed.scanner.data.DmedCard
import com.google.gson.Gson
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import java.util.concurrent.TimeUnit

class HttpCardReader {

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .build()
    private val gson = Gson()

    var debugError: String = ""
        private set

    suspend fun readCard(host: String, port: Int): DmedCard? = withContext(Dispatchers.IO) {
        try {
            val request = Request.Builder()
                .url("http://$host:$port/dmed/card")
                .get()
                .build()
            val response = client.newCall(request).execute()
            if (!response.isSuccessful) {
                debugError = "HTTP ${response.code}"
                return@withContext null
            }
            val body = response.body?.string() ?: run {
                debugError = "Empty response body"
                return@withContext null
            }
            gson.fromJson(body, DmedCard::class.java)
        } catch (e: Exception) {
            debugError = e.message ?: "Unknown error"
            null
        }
    }
}
