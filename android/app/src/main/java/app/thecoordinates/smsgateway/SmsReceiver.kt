package app.thecoordinates.smsgateway

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.provider.Telephony
import android.util.Log

/** Receives inbound SMS and forwards them to the server. */
class SmsReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Telephony.Sms.Intents.SMS_RECEIVED_ACTION) return
        val prefs = Prefs(context)
        if (!prefs.enabled || prefs.apiKey.isBlank()) return

        // Multipart messages arrive as multiple PDUs from the same sender — concatenate.
        val messages = Telephony.Sms.Intents.getMessagesFromIntent(intent) ?: return
        if (messages.isEmpty()) return
        val sender = messages[0].originatingAddress ?: return
        val body = messages.joinToString("") { it.messageBody ?: "" }

        val client = ApiClient(prefs.baseUrl, prefs.apiKey)
        val pending = goAsync()
        Thread {
            try {
                client.postInbound(sender, body)
            } catch (e: Exception) {
                Log.w("SmsReceiver", "postInbound failed: ${e.message}")
            } finally {
                pending.finish()
            }
        }.start()
    }
}
