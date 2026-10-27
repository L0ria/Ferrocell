# Docs

Deep-dive documentation for Ferrocell (the README keeps build + usage):

- [`core.md`](core.md) — the pure, host-testable core + the firmware-only hardware layer (class tables)
- [`wifi.md`](wifi.md) — `WifiManager`: AP config page + STA auto-reconnect
- [`web-ui-api.md`](web-ui-api.md) — the web UI + the JSON API endpoint table
- [`ota.md`](ota.md) — OTA over WiFi (`ElegantOTA` on the shared `WebServer`)
- [`design-notes.md`](design-notes.md) — design decisions (modularity, GPIO choice, reproducible builds, responsive UI, OTA)
