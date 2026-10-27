# OTA over WiFi (issue #16, firmware-only)

`ElegantOTA` (v4.0.0) is attached to the **same** `WebServer` the UI uses
(via `UiServer::rawServer()`), so its `/update` portal + `/ota/*` routes share
**port 80** in **both** AP and STA mode — no new port, no collision with the
M4 config page on 8080. `UiServer` remains the only class that owns the
server; the entrypoint just hands `rawServer()` to the library.

| What | Value |
|---|---|
| Portal | `http://<ip>/update` (STA) · `http://192.168.1.1/update` (AP) — also the UI's **Update** tab |
| Routes | `GET /update`, `GET /ota/metadata`, `GET /ota/start`, `POST /ota/upload` |
| Auth | open for v1 (local network; HTTP Basic auth is issue #17) |
| Auto-reboot | on — the device reboots into the new app ~2 s after a successful upload |
| Config retention | the saved WiFi config lives in **NVS**, which OTA never touches — it survives every update |
| Partition | `PartitionScheme=otanofs` → two 1.94 MiB OTA slots (≈836 KB headroom each); we mount no filesystem at runtime, so the `default` scheme's 1.5 MB SPIFFS was dead weight |

**One-time note (issue #16, A4):** the first firmware under `otanofs` must be
flashed **over USB** (`./build.sh --upload`) — a USB flash writes `app0`/factory
and replaces the old `default` layout. Every update after that can be OTA.

`ElegantOTA` is compiled into the firmware only (verified by `firmware-build`);
its route table is asserted by the `ui-contract` harness (section 9).
