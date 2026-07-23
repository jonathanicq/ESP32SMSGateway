#!/usr/bin/env bash
# Install the Android SMS Gateway APK onto a USB-connected device via ADB.
#
# Usage:
#   scripts/install-apk.sh                 # installs releases/android/sms-gateway-latest.apk
#   scripts/install-apk.sh path/to.apk     # installs a specific APK
#
# This is the supported way to (re)install/update the app on a test phone.
# After the one-time device + host setup below, updating is just re-running this.
#
# One-time device setup (Settings -> Additional settings -> Developer options):
#   - USB debugging                ON
#   - Install via USB              ON   (Xiaomi/MIUI/HyperOS; may need SIM + Mi account)
#   - USB debugging (Security ...) ON   (Xiaomi/MIUI/HyperOS)
#   Then approve the "Allow USB debugging?" prompt (tick "Always allow").
#
# One-time host setup (so you never need sudo/chmod again):
#   sudo cp scripts/udev/51-android.rules /etc/udev/rules.d/
#   sudo udevadm control --reload-rules && sudo udevadm trigger
#   (then replug the phone)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APK="${1:-$ROOT/releases/android/sms-gateway-latest.apk}"
PKG="app.thecoordinates.smsgateway"

# --- locate adb -------------------------------------------------------------
ADB="$(command -v adb || true)"
if [ -z "$ADB" ]; then
  for c in "${ANDROID_HOME:-}" "${ANDROID_SDK_ROOT:-}" \
           "$HOME/android-sdk" "$HOME/Android/Sdk" /opt/android-sdk; do
    [ -n "$c" ] && [ -x "$c/platform-tools/adb" ] && ADB="$c/platform-tools/adb" && break
  done
fi
[ -n "$ADB" ] || { echo "ERROR: adb not found. Install platform-tools or set ANDROID_HOME." >&2; exit 1; }
[ -f "$APK" ] || { echo "ERROR: APK not found: $APK" >&2; exit 1; }

echo "adb: $ADB"
echo "apk: $APK"

# --- check device state -----------------------------------------------------
"$ADB" start-server >/dev/null 2>&1
STATE="$("$ADB" get-state 2>/dev/null || true)"
LINE="$("$ADB" devices | sed -n '2p')"

case "$LINE" in
  *unauthorized*)
    echo "Device is UNAUTHORIZED. Approve the 'Allow USB debugging?' prompt on the phone, then re-run." >&2
    exit 1 ;;
  *"no permissions"*)
    echo "Host has NO PERMISSIONS for the USB node. Install the udev rule once:" >&2
    echo "  sudo cp $ROOT/scripts/udev/51-android.rules /etc/udev/rules.d/ && sudo udevadm control --reload-rules && sudo udevadm trigger" >&2
    echo "Then replug the phone and re-run." >&2
    exit 1 ;;
esac
[ "$STATE" = "device" ] || { echo "No authorized device connected (state: ${STATE:-none})." >&2; "$ADB" devices -l >&2; exit 1; }

echo "device: $("$ADB" shell getprop ro.product.model | tr -d '\r') (Android $("$ADB" shell getprop ro.build.version.release | tr -d '\r'))"

# --- install ----------------------------------------------------------------
echo "Installing..."
set +e
OUT="$("$ADB" install -r "$APK" 2>&1)"
RC=$?
set -e
echo "$OUT"

if [ $RC -ne 0 ]; then
  if echo "$OUT" | grep -q INSTALL_FAILED_USER_RESTRICTED; then
    echo >&2
    echo "Xiaomi/MIUI blocked the install (INSTALL_FAILED_USER_RESTRICTED)." >&2
    echo "Enable Developer options -> 'Install via USB' AND 'USB debugging (Security settings)'," >&2
    echo "(may require a SIM inserted + Mi account signed in), then re-run." >&2
  elif echo "$OUT" | grep -q INSTALL_FAILED_UPDATE_INCOMPATIBLE; then
    echo >&2
    echo "Signature mismatch with an already-installed build. Uninstall first:" >&2
    echo "  $ADB uninstall $PKG" >&2
  fi
  exit $RC
fi

# --- verify + launch --------------------------------------------------------
VER="$("$ADB" shell dumpsys package "$PKG" 2>/dev/null | grep -m1 versionName | tr -d '\r' | sed 's/.*versionName=//')"
echo "Installed $PKG version $VER"
"$ADB" shell monkey -p "$PKG" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1 || true
echo "Launched. Done."
