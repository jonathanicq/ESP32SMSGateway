package app.thecoordinates.smsgateway

import android.content.Context

/** Thin wrapper over SharedPreferences for server config + enabled state. */
class Prefs(context: Context) {
    private val sp = context.getSharedPreferences("smsgw", Context.MODE_PRIVATE)

    var baseUrl: String
        get() = sp.getString("base_url", "https://test.sms-gateway.thecoordinates.app") ?: ""
        set(v) = sp.edit().putString("base_url", v.trimEnd('/')).apply()

    var apiKey: String
        get() = sp.getString("api_key", "") ?: ""
        set(v) = sp.edit().putString("api_key", v.trim()).apply()

    var enabled: Boolean
        get() = sp.getBoolean("enabled", false)
        set(v) = sp.edit().putBoolean("enabled", v).apply()

    var pollSeconds: Int
        get() = sp.getInt("poll_seconds", 10)
        set(v) = sp.edit().putInt("poll_seconds", v).apply()
}
