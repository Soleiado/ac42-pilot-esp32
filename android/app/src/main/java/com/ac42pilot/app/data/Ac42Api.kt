package com.ac42pilot.app.data

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit

class ApiException(message: String, val statusCode: Int? = null) : Exception(message)

class Ac42Api(
    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(4, TimeUnit.SECONDS)
        .readTimeout(5, TimeUnit.SECONDS)
        .writeTimeout(5, TimeUnit.SECONDS)
        .build(),
) {
    @Volatile
    var baseUrl: String = DEFAULT_BASE_URL

    fun updateBaseUrl(raw: String) {
        baseUrl = normalizeBaseUrl(raw)
    }

    suspend fun getStatus(): Ac42Status = withContext(Dispatchers.IO) {
        val obj = executeJson("GET", "/api/status")
            ?: throw ApiException("Réponse vide du boîtier")
        parseStatus(obj)
    }

    suspend fun getSettings(): DeviceSettings = withContext(Dispatchers.IO) {
        val obj = executeJson("GET", "/api/settings")
            ?: throw ApiException("Réponse vide du boîtier")
        parseSettings(obj.toString())
    }

    suspend fun saveSettings(update: SettingsUpdate): CommandResponse = withContext(Dispatchers.IO) {
        val obj = JSONObject().apply {
            put("ap_ssid", update.apSsid)
            if (!update.apPassword.isNullOrEmpty()) put("ap_password", update.apPassword)
            put("sta_ssid", update.staSsid)
            if (!update.staPassword.isNullOrEmpty()) put("sta_password", update.staPassword)
            put("hostname", update.hostname)
            put("controller_override", update.controllerOverride)
            put("ap_override", update.apOverride)
            put("rudder_timeout_ms", update.rudderTimeoutMs)
        }
        parseCommandResponse(executeJson("POST", "/api/settings", obj)?.toString() ?: "{}")
    }

    suspend fun setMode(mode: String): CommandResponse = post("/api/mode", JSONObject().put("mode", mode))

    suspend fun changeCourse(side: String, degrees: Int): CommandResponse =
        post("/api/course", JSONObject().put("side", side).put("deg", degrees))

    suspend fun tack(direction: String): CommandResponse =
        post("/api/tack", JSONObject().put("dir", direction))

    suspend fun rudder(direction: String): CommandResponse =
        post("/api/rudder", JSONObject().put("dir", direction))

    suspend fun setResponse(set: String): CommandResponse =
        post("/api/response", JSONObject().put("set", set))

    suspend fun reboot(): CommandResponse = post("/api/reboot", JSONObject())

    suspend fun getLog(): List<LogEntry> = withContext(Dispatchers.IO) {
        val text = executeText("GET", "/api/log")
        if (text.isBlank()) emptyList() else parseLog(text)
    }

    private suspend fun post(path: String, body: JSONObject): CommandResponse =
        withContext(Dispatchers.IO) {
            val text = executeText("POST", path, body)
            parseCommandResponse(text.ifBlank { "{}" })
        }

    private fun executeJson(method: String, path: String, body: JSONObject? = null): JSONObject? {
        val text = executeText(method, path, body)
        return if (text.isBlank()) null else JSONObject(text)
    }

    private fun executeText(method: String, path: String, body: JSONObject? = null): String {
        val builder = Request.Builder().url(baseUrl + path)
        if (method == "POST") {
            val payload = (body ?: JSONObject()).toString()
            builder.post(payload.toRequestBody(JSON_MEDIA_TYPE))
        } else {
            builder.get()
        }

        client.newCall(builder.build()).execute().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) {
                throw ApiException(errorMessage(text, "Erreur HTTP ${response.code}"), response.code)
            }
            return text
        }
    }

    private fun errorMessage(text: String, fallback: String): String {
        return runCatching {
            val obj = JSONObject(text)
            obj.optString("error").ifBlank { obj.optString("message") }
        }.getOrNull()?.takeIf { it.isNotBlank() } ?: fallback
    }

    companion object {
        const val DEFAULT_BASE_URL = "http://192.168.4.1"
        private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()

        fun normalizeBaseUrl(raw: String): String {
            var value = raw.trim()
            if (value.isEmpty()) value = DEFAULT_BASE_URL
            if (!value.startsWith("http://") && !value.startsWith("https://")) {
                value = "http://$value"
            }
            return value.trimEnd('/')
        }
    }
}
