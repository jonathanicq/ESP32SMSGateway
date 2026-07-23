package app.thecoordinates.smsgateway

import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.net.HttpURLConnection
import java.net.URL

/** Device-facing API client (X-API-Key). Uses HttpURLConnection + org.json — no external deps. */
class ApiClient(private val baseUrl: String, private val apiKey: String) {

    data class PendingMessage(val id: Int, val phoneNumber: String, val text: String)

    private fun open(method: String, path: String): HttpURLConnection {
        val conn = URL("$baseUrl$path").openConnection() as HttpURLConnection
        conn.requestMethod = method
        conn.setRequestProperty("X-API-Key", apiKey)
        conn.setRequestProperty("Accept", "application/json")
        conn.connectTimeout = 15000
        conn.readTimeout = 20000
        return conn
    }

    private fun writeBody(conn: HttpURLConnection, body: JSONObject) {
        conn.doOutput = true
        conn.setRequestProperty("Content-Type", "application/json")
        conn.outputStream.use { it.write(body.toString().toByteArray()) }
    }

    private fun HttpURLConnection.readText(): String {
        val stream = if (responseCode in 200..299) inputStream else errorStream
        return stream?.bufferedReader()?.use(BufferedReader::readText) ?: ""
    }

    /** GET /api/device/messages/pending */
    fun fetchPending(): List<PendingMessage> {
        val conn = open("GET", "/api/device/messages/pending")
        try {
            if (conn.responseCode !in 200..299) return emptyList()
            val arr = JSONArray(conn.readText())
            return (0 until arr.length()).map { i ->
                val o = arr.getJSONObject(i)
                PendingMessage(o.getInt("id"), o.getString("phone_number"), o.getString("text"))
            }
        } finally {
            conn.disconnect()
        }
    }

    /** POST /api/device/messages/{id}/status */
    fun postStatus(id: Int, status: String, error: String? = null): Boolean {
        val conn = open("POST", "/api/device/messages/$id/status")
        try {
            val body = JSONObject().put("status", status)
            if (error != null) body.put("error", error)
            writeBody(conn, body)
            return conn.responseCode in 200..299
        } finally {
            conn.disconnect()
        }
    }

    /** POST /api/device/messages/inbound */
    fun postInbound(phoneNumber: String, text: String): Boolean {
        val conn = open("POST", "/api/device/messages/inbound")
        try {
            writeBody(conn, JSONObject().put("phone_number", phoneNumber).put("text", text))
            return conn.responseCode in 200..299
        } finally {
            conn.disconnect()
        }
    }

    companion object {
        data class PairStart(val claimCode: String, val pollToken: String)
        data class PairPoll(val status: String, val apiKey: String?)

        private fun postJson(baseUrl: String, path: String, body: JSONObject): String? {
            val conn = URL("${baseUrl.trimEnd('/')}$path").openConnection() as HttpURLConnection
            return try {
                conn.requestMethod = "POST"
                conn.doOutput = true
                conn.connectTimeout = 15000
                conn.readTimeout = 20000
                conn.setRequestProperty("Content-Type", "application/json")
                conn.setRequestProperty("Accept", "application/json")
                conn.outputStream.use { it.write(body.toString().toByteArray()) }
                if (conn.responseCode !in 200..299) return null
                conn.inputStream.bufferedReader().use(BufferedReader::readText)
            } catch (e: Exception) {
                null
            } finally {
                conn.disconnect()
            }
        }

        /** POST /api/pair/start — begins pairing; returns the claim code to show the user. */
        fun pairStart(baseUrl: String, deviceName: String): PairStart? {
            val resp = postJson(baseUrl, "/api/pair/start",
                JSONObject().put("device_name", deviceName)) ?: return null
            val o = JSONObject(resp)
            return PairStart(o.getString("claim_code"), o.getString("poll_token"))
        }

        /** POST /api/pair/poll — returns status (pending|linked|expired) and api_key when linked. */
        fun pairPoll(baseUrl: String, pollToken: String): PairPoll? {
            val resp = postJson(baseUrl, "/api/pair/poll",
                JSONObject().put("poll_token", pollToken)) ?: return null
            val o = JSONObject(resp)
            return PairPoll(o.getString("status"), if (o.isNull("api_key")) null else o.getString("api_key"))
        }
    }
}
