package app.thecoordinates.smsgateway

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import app.thecoordinates.smsgateway.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding
    private lateinit var prefs: Prefs
    private val main = Handler(Looper.getMainLooper())
    @Volatile private var pairing = false

    private val permissions = buildList {
        add(Manifest.permission.SEND_SMS)
        add(Manifest.permission.RECEIVE_SMS)
        add(Manifest.permission.READ_PHONE_STATE)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            add(Manifest.permission.POST_NOTIFICATIONS)
        }
    }.toTypedArray()

    private val permLauncher = registerForActivityResult(
        androidx.activity.result.contract.ActivityResultContracts.RequestMultiplePermissions()
    ) { startGateway() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)
        prefs = Prefs(this)

        binding.serverUrl.setText(prefs.baseUrl)

        binding.pairButton.setOnClickListener { beginPairing() }
        binding.startButton.setOnClickListener { onStartClicked() }
        binding.stopButton.setOnClickListener {
            stopService(Intent(this, GatewayService::class.java))
            prefs.enabled = false
            render()
        }
        render()
    }

    private fun beginPairing() {
        if (pairing) return
        prefs.baseUrl = binding.serverUrl.text.toString()
        val base = prefs.baseUrl
        if (base.isBlank()) { binding.claimCode.text = "Enter a server URL first"; return }
        pairing = true
        binding.claimCode.text = "…"
        binding.instructions.text = "Requesting a code…"
        Thread {
            val start = ApiClient.pairStart(base, Build.MODEL ?: "Android device")
            if (start == null) {
                main.post { pairing = false; binding.claimCode.text = "Pairing failed"
                    binding.instructions.text = "Check the server URL and try again." }
                return@Thread
            }
            main.post {
                binding.claimCode.text = start.claimCode
                binding.instructions.text =
                    "In the backoffice → Devices → Pair, enter this code. Waiting for link…"
            }
            // Poll until linked / expired (TTL ~15 min).
            repeat(180) {
                Thread.sleep(5000)
                val poll = ApiClient.pairPoll(base, start.pollToken) ?: return@repeat
                when (poll.status) {
                    "linked" -> {
                        prefs.apiKey = poll.apiKey ?: ""
                        main.post {
                            pairing = false
                            binding.claimCode.text = "✓ Linked"
                            binding.instructions.text = "Device linked. Tap Start to begin relaying."
                            render()
                        }
                        return@Thread
                    }
                    "expired" -> {
                        main.post { pairing = false; binding.claimCode.text = "Code expired"
                            binding.instructions.text = "Tap Pair to get a new code." }
                        return@Thread
                    }
                }
            }
            main.post { pairing = false }
        }.start()
    }

    private fun onStartClicked() {
        prefs.baseUrl = binding.serverUrl.text.toString()
        if (prefs.apiKey.isBlank()) {
            binding.instructions.text = "Pair the device first to get an API key."
            return
        }
        if (!hasAllPermissions()) { permLauncher.launch(permissions); return }
        startGateway()
    }

    private fun startGateway() {
        if (!hasAllPermissions()) return
        ContextCompat.startForegroundService(this, Intent(this, GatewayService::class.java))
        render()
    }

    private fun hasAllPermissions() = permissions.all {
        ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
    }

    private fun render() {
        val linked = prefs.apiKey.isNotBlank()
        binding.status.text = when {
            prefs.enabled -> "Status: running"
            linked -> "Status: linked, stopped"
            else -> "Status: not paired"
        }
        binding.startButton.isEnabled = linked
    }

    override fun onResume() { super.onResume(); render() }
}
