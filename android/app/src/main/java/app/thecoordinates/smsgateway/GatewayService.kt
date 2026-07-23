package app.thecoordinates.smsgateway

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.IBinder
import android.telephony.SmsManager
import android.util.Log
import androidx.core.app.NotificationCompat

/**
 * Foreground service: polls the server for pending outbound SMS, sends them via SmsManager,
 * and reports sent/delivered/failed status back to the server.
 */
class GatewayService : android.app.Service() {

    private val tag = "GatewayService"
    private val channelId = "gateway"
    @Volatile private var running = false
    private lateinit var prefs: Prefs
    private var worker: Thread? = null

    private val sentReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            val id = intent.getIntExtra("msgId", -1)
            if (id < 0) return
            val ok = resultCode == android.app.Activity.RESULT_OK
            Thread {
                api()?.postStatus(id, if (ok) "sent" else "failed",
                    if (ok) null else "send result=$resultCode")
            }.start()
        }
    }

    private val deliveredReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            val id = intent.getIntExtra("msgId", -1)
            if (id < 0) return
            Thread { api()?.postStatus(id, "delivered") }.start()
        }
    }

    private fun api(): ApiClient? {
        val url = prefs.baseUrl
        val key = prefs.apiKey
        return if (url.isBlank() || key.isBlank()) null else ApiClient(url, key)
    }

    override fun onCreate() {
        super.onCreate()
        prefs = Prefs(this)
        registerExported(sentReceiver, ACTION_SENT)
        registerExported(deliveredReceiver, ACTION_DELIVERED)
    }

    private fun registerExported(receiver: BroadcastReceiver, action: String) {
        val filter = IntentFilter(action)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(receiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            @Suppress("UnspecifiedRegisterReceiverFlag")
            registerReceiver(receiver, filter)
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(1, buildNotification())
        prefs.enabled = true
        if (!running) {
            running = true
            worker = Thread { pollLoop() }.also { it.start() }
        }
        return START_STICKY
    }

    private fun pollLoop() {
        while (running) {
            try {
                val client = api()
                if (client != null) {
                    for (msg in client.fetchPending()) {
                        sendSms(msg)
                    }
                }
            } catch (e: Exception) {
                Log.w(tag, "poll error: ${e.message}")
            }
            try {
                Thread.sleep(prefs.pollSeconds.coerceAtLeast(5) * 1000L)
            } catch (_: InterruptedException) {
                break
            }
        }
    }

    private fun smsManager(): SmsManager =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
            getSystemService(SmsManager::class.java)
        else
            @Suppress("DEPRECATION") SmsManager.getDefault()

    private fun sendSms(msg: ApiClient.PendingMessage) {
        try {
            val sm = smsManager()
            val parts = sm.divideMessage(msg.text)
            val sentPI = pendingIntent(ACTION_SENT, msg.id)
            val deliveredPI = pendingIntent(ACTION_DELIVERED, msg.id)
            if (parts.size <= 1) {
                sm.sendTextMessage(msg.phoneNumber, null, msg.text, sentPI, deliveredPI)
            } else {
                // Attach the status intents to the first part only → one callback per message.
                val sentList = ArrayList<PendingIntent?>(parts.size).apply {
                    add(sentPI); repeat(parts.size - 1) { add(null) }
                }
                val deliveredList = ArrayList<PendingIntent?>(parts.size).apply {
                    add(deliveredPI); repeat(parts.size - 1) { add(null) }
                }
                sm.sendMultipartTextMessage(msg.phoneNumber, null, parts, sentList, deliveredList)
            }
        } catch (e: Exception) {
            Log.w(tag, "sendSms ${msg.id} failed: ${e.message}")
            Thread { api()?.postStatus(msg.id, "failed", e.message) }.start()
        }
    }

    private fun pendingIntent(action: String, msgId: Int): PendingIntent {
        val intent = Intent(action).setPackage(packageName).putExtra("msgId", msgId)
        val flags = PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        return PendingIntent.getBroadcast(this, msgId * 2 + action.hashCode(), intent, flags)
    }

    private fun buildNotification(): Notification {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val nm = getSystemService(NotificationManager::class.java)
            nm.createNotificationChannel(
                NotificationChannel(channelId, "SMS Gateway", NotificationManager.IMPORTANCE_LOW)
            )
        }
        return NotificationCompat.Builder(this, channelId)
            .setContentTitle("SMS Gateway active")
            .setContentText("Relaying SMS for ${prefs.baseUrl}")
            .setSmallIcon(android.R.drawable.stat_sys_upload)
            .setOngoing(true)
            .build()
    }

    override fun onDestroy() {
        running = false
        prefs.enabled = false
        worker?.interrupt()
        runCatching { unregisterReceiver(sentReceiver) }
        runCatching { unregisterReceiver(deliveredReceiver) }
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    companion object {
        const val ACTION_SENT = "app.thecoordinates.smsgateway.SMS_SENT"
        const val ACTION_DELIVERED = "app.thecoordinates.smsgateway.SMS_DELIVERED"
    }
}
