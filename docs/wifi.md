# WiFi (M4, firmware-only)

`WifiManager` (`ferrocell/src/net/WifiManager.{h,cpp}`) is the **only** class
that talks to WiFi — it wraps the vendored `tabahi/ESP-Wifi-Config` (v2.2.6)
and the application never calls `WiFi.*` directly.

| What | Value |
|---|---|
| AP IP | `192.168.1.1` (library default) |
| AP SSID | `Ferrocell_<9-digit-mac>` (unique per device; the lib appends the MAC) |
| Config page | `http://192.168.1.1:8080/` (port **8080** — root `80` stays free for the M5 web UI) |
| Config-page login | **admin / pass_ESP** (library defaults) |
| STA | saved network in range → connect + **auto-reconnect** (10 s interval) |

**How it works:**
- **No saved WiFi (or the saved one is out of range)** → the device starts an
  access point. Join `Ferrocell_<mac>`, open `http://192.168.1.1:8080/`
  (login **admin / pass_ESP**), pick the network (or type an unbroadcast SSID),
  and hit **Save**. Saving persists the credentials to flash and **reboots**
  the device into STA mode.
- **Saved WiFi in range** → the device connects in STA mode and auto-reconnects
  if the link drops. The device gets an IP; the (M5) web UI will be reachable
  at `http://<ip>/`.
- The config page stays up **perpetually** (in both modes) so the device is
  always re-configurable.
- **Reset:** `WifiManager::resetConfig()` wipes the saved credentials and
  reboots the device back into AP mode.

`WifiManager` is a thin wrapper (little pure logic) and is compiled into the
firmware only — verified by the `firmware-build` CI job, consistent with the
other hw-layer classes.
