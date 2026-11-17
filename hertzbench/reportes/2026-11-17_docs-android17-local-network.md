# Docs: Android 17 necesita ACCESS_LOCAL_NETWORK

Estado: **enviado** el 2026-11-17 como [#114](https://github.com/Rixmerz/hertzbench/issues/114).
Título: Docs: Android 17 apps need ACCESS_LOCAL_NETWORK to reach the bridge at 10.0.2.2 / Android 17 necesita el permiso ACCESS_LOCAL_NETWORK

---

**English**

On Android 17 (API 37), an app cannot reach the IP bridge at `10.0.2.2` (or a host on the LAN) unless it holds the new runtime permission **`android.permission.ACCESS_LOCAL_NETWORK`**. Without it, the TCP connect just times out, so it looks like a bridge or network problem:

```
SocketTimeoutException: failed to connect to /10.0.2.2 (port 1883) from /10.0.2.17 (port 47360) after 5000ms
```

The same route works from `adb shell` (`echo | nc -w 3 10.0.2.2 1883` succeeds, also bound to the Wi-Fi address), because the shell is not subject to the permission. That makes the cause hard to find.

**Fix on the app side** (tested, emulator Android 17 arm64, hertzbench `7f5bd4ae`, MQTT with Paho):

```xml
<uses-permission android:name="android.permission.ACCESS_LOCAL_NETWORK" />
```

```kotlin
if (Build.VERSION.SDK_INT >= 37) requestPermissions(arrayOf(Manifest.permission.ACCESS_LOCAL_NETWORK), 1)
// or, for a test run: adb shell pm grant <package> android.permission.ACCESS_LOCAL_NETWORK
```

**Suggestion:** a short note in `docs/mobile-bridge.md`, next to the `10.0.2.2` instructions, so students targeting API 37 do not lose time on it. Found while preparing the test report for #110.

---

**Español:** en Android 17 (API 37) la app necesita el permiso en tiempo de ejecución `ACCESS_LOCAL_NETWORK` para conectarse al puente en `10.0.2.2` (o a la LAN). Sin él la conexión expira, aunque `adb shell nc` funcione. Sugiero una nota en `docs/mobile-bridge.md`.
