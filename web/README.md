The Ferrocell web UI source of truth lives in `ferrocell/data/` (M5, issue
#6): `index.html` + `ui.css` + `ui.js`. It is embedded into PROGMEM by
`tools/make_ui_assets.py` (→ `ferrocell/src/net/ui_assets.h`) and served by
`UiServer` on port 80. This directory is kept only as a pointer — see
`ferrocell/data/`.
