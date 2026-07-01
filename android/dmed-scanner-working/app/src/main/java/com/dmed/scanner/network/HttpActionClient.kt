package com.dmed.scanner.network

import com.dmed.scanner.data.ActionResponse
import com.google.gson.Gson
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit

class HttpActionClient {

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(15, TimeUnit.SECONDS)
        .build()
    private val gson = Gson()
    private val json = "application/json".toMediaType()

    suspend fun sendAction(
        host: String,
        port: Int,
        action: String,
        params: Map<String, Any> = emptyMap()
    ): ActionResponse? = withContext(Dispatchers.IO) {
        try {
            val body = gson.toJson(mapOf("action" to action, "params" to params))
            val request = Request.Builder()
                .url("http://$host:$port/dmed/action")
                .post(body.toRequestBody(json))
                .build()
            val response = client.newCall(request).execute()
            val responseBody = response.body?.string() ?: return@withContext null
            gson.fromJson(responseBody, ActionResponse::class.java)
        } catch (e: Exception) {
            null
        }
    }
}
