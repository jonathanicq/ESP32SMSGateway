# Android Gateway App

Native Kotlin app that registers as a **device** against the private server and relays SMS:
- Foreground `GatewayService` polls `GET /api/device/messages/pending`, sends each via `SmsManager`, and reports `sent`/`delivered`/`failed` to `POST /api/device/messages/{id}/status`.
- `SmsReceiver` forwards inbound SMS to `POST /api/device/messages/inbound`.
- `MainActivity` configures server URL + device API key (from the backoffice) and starts/stops the service.

No external HTTP library — uses `HttpURLConnection` + built-in `org.json`.

## Build
```bash
export ANDROID_HOME=/home/sysadmin/android-sdk
export JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64
cd android
./gradlew :app:assembleDebug      # -> app/build/outputs/apk/debug/app-debug.apk
```
- compileSdk/targetSdk 34, minSdk 24, AGP 8.7.2, Kotlin 2.0.21, Gradle 8.11.1.

## Configure on device
1. In the backoffice (`https://test.sms-gateway.thecoordinates.app`) → Devices → add a device, copy its API key.
2. In the app: paste the server URL + API key, grant SMS/notification permissions, tap **Start**.

## Status / not yet done
- The debug APK in `releases/android/` is **debug-signed** (for sideload testing), not release-signed.
- Runtime SMS send/receive requires a physical device with a SIM — **not yet verified on hardware**.
- Release signing + NAS/DreamHost distribution: deferred to the release phase.
