/* =============================================================================
   Ferrocell — web UI logic (M5, issue #6).

   Two parts in one self-contained file (no CDN, no build step):
     1. `FerrocellUI.pure` — PURE, framework-free helpers: HSV<->RGB, the
        wheel<->(hue,saturation) mapping, the preset table, and the request
        payload builders. These have NO DOM / network dependencies, so the M6
        Node harness can import this file and assert the JS contract against
        the C++ core (the "stable contract for the Node harness" acceptance).
     2. the DOM glue — wires the controls to the JSON API on port 80.

   API contract (must stay in sync with ferrocell/src/core/Api.{h,cpp}):
     GET  /api/state            -> { ok, settings:{...}, wifi:{...}, fw }
     POST /api/set              -> { ok, settings:{...}, rebooting? }
     POST /api/wifi {scan}      -> { ok, networks:[{ssid,rssi,locked}] }
     POST /api/wifi {save}      -> { ok, rebooting:true }
     POST /api/reboot           -> { ok, rebooting:true }
     POST /api/reset            -> { ok, rebooting:true }
   ========================================================================== */
(function (global) {
  "use strict";

  // --- pure helpers (host-testable, no DOM) --------------------------------
  var pure = {
    // The JSON API endpoints (single source of truth on the JS side; mirrors
    // the C++ side served by UiServer + core/Api.h). The M6 Node harness
    // (tests/ui/harness.mjs) asserts this table against the C++ core.
    endpoints: [
      { method: "GET",  path: "/api/state" },
      { method: "POST", path: "/api/set" },
      { method: "POST", path: "/api/wifi" },
      { method: "POST", path: "/api/reboot" },
      { method: "POST", path: "/api/reset" }
    ],

    knownModes: ["solid", "rainbow", "breathing", "gradient", "off"],

    // Canonical ranges (mirror ferrocell/src/core/Settings.h).
    ranges: {
      ledCount: { min: 1, max: 512 },
      hue: { min: 0, max: 360 },
      saturation: { min: 0, max: 1 },
      brightness: { min: 0, max: 1 },
      speed: { min: 0, max: 10 }
    },

    clamp: function (x, lo, hi) { return x < lo ? lo : (x > hi ? hi : x); },
    clamp01: function (x) { return pure.clamp(x, 0, 1); },
    wrapHue: function (h) { h = h % 360; if (h < 0) h += 360; return h; },

    // HSV (h:0..360, s:0..1, v:0..1) -> {r,g,b} each 0..255.
    // Mirrors ferrocell/src/core/Color.cpp hsvToRgb exactly.
    hsvToRgb: function (h, s, v) {
      h = pure.wrapHue(h);
      s = pure.clamp01(s);
      v = pure.clamp01(v);
      if (v === 0) return { r: 0, g: 0, b: 0 };
      if (s === 0) { var grey = Math.round(v * 255); return { r: grey, g: grey, b: grey }; }
      var c = v * s;
      var hp = h / 60;
      var x = c * (1 - Math.abs((hp % 2) - 1));
      var m = v - c;
      var r1 = 0, g1 = 0, b1 = 0;
      if (hp < 1) { r1 = c; g1 = x; }
      else if (hp < 2) { r1 = x; g1 = c; }
      else if (hp < 3) { g1 = c; b1 = x; }
      else if (hp < 4) { g1 = x; b1 = c; }
      else if (hp < 5) { r1 = x; b1 = c; }
      else { r1 = c; b1 = x; }
      return {
        r: Math.round((r1 + m) * 255),
        g: Math.round((g1 + m) * 255),
        b: Math.round((b1 + m) * 255)
      };
    },

    // {r,g,b} 0..255 -> {h:0..360, s:0..1, v:0..1}.
    rgbToHsv: function (r, g, b) {
      r /= 255; g /= 255; b /= 255;
      var max = Math.max(r, g, b), min = Math.min(r, g, b), d = max - min;
      var h = 0;
      if (d !== 0) {
        if (max === r) h = ((g - b) / d) % 6;
        else if (max === g) h = (b - r) / d + 2;
        else h = (r - g) / d + 4;
        h *= 60;
        if (h < 0) h += 360;
      }
      var s = max === 0 ? 0 : d / max;
      return { h: h, s: s, v: max };
    },

    // Wheel mapping: center = white (saturation 0), edge = full saturation.
    // hue = angle (degrees, 0 at +x axis, increasing clockwise on screen).
    // (x,y) are pixel offsets from the wheel center; r is the wheel radius.
    wheelToHueSat: function (dx, dy, r) {
      var dist = Math.sqrt(dx * dx + dy * dy);
      var sat = pure.clamp01(dist / r);
      var hue = (Math.atan2(dy, dx) * 180) / Math.PI;
      if (hue < 0) hue += 360;
      return { hue: hue, saturation: sat };
    },

    hueSatToWheel: function (hue, saturation, r) {
      var rad = (hue * Math.PI) / 180;
      var dist = saturation * r;
      return { x: Math.cos(rad) * dist, y: Math.sin(rad) * dist };
    },

    // One-tap presets (client-side bundles sent as a single /api/set).
    presets: [
      { name: "Red",    hue: 0,   saturation: 1, brightness: 0.9, mode: "solid",     speed: 1 },
      { name: "Green",  hue: 120, saturation: 1, brightness: 0.9, mode: "solid",     speed: 1 },
      { name: "Blue",   hue: 240, saturation: 1, brightness: 0.9, mode: "solid",     speed: 1 },
      { name: "Warm",   hue: 30,  saturation: 0.8, brightness: 0.7, mode: "solid",   speed: 1 },
      { name: "Rainbow",hue: 0,   saturation: 1, brightness: 0.8, mode: "rainbow",   speed: 1 },
      { name: "Breathe",hue: 200, saturation: 0.9, brightness: 0.8, mode: "breathing", speed: 1 },
      { name: "Off",    hue: 0,   saturation: 1, brightness: 0,   mode: "off",       speed: 1 }
    ],

    // Build a /api/set payload from a partial settings object, keeping only
    // the known fields (mirrors the C++ applySet validation on the wire).
    knownSetFields: ["hue", "saturation", "brightness", "mode", "speed", "ledCount"],
    buildSetPayload: function (partial) {
      var out = {};
      if (!partial || typeof partial !== "object") return out;
      for (var i = 0; i < pure.knownSetFields.length; i++) {
        var k = pure.knownSetFields[i];
        if (Object.prototype.hasOwnProperty.call(partial, k)) out[k] = partial[k];
      }
      return out;
    },

    // Normalize a /api/state document into a flat, safe view for the UI.
    parseState: function (doc) {
      var s = (doc && doc.settings) || {};
      var w = (doc && doc.wifi) || {};
      return {
        ok: !!(doc && doc.ok),
        fw: (doc && doc.fw) || "",
        ledCount: Number(s.ledCount) || 0,
        hue: Number(s.hue) || 0,
        saturation: (s.saturation === undefined) ? 1 : Number(s.saturation),
        brightness: (s.brightness === undefined) ? 0 : Number(s.brightness),
        mode: String(s.mode || "solid"),
        speed: (s.speed === undefined) ? 1 : Number(s.speed),
        wifiMode: String(w.mode || "ap"),
        wifiConnected: !!w.connected,
        ip: String(w.ip || ""),
        ssid: String(w.ssid || ""),
        rssi: Number(w.rssi) || 0,
        apName: String(w.apName || ""),
        configUrl: String(w.configUrl || "")
      };
    }
  };

  var FerrocellUI = { pure: pure };

  // --- DOM glue (only when a browser is present) ---------------------------
  if (typeof document !== "undefined" && typeof window !== "undefined") {
    var api = {
      state: function () { return fetch("/api/state").then(function (r) { return r.json(); }); },
      set: function (payload) {
        return fetch("/api/set", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(pure.buildSetPayload(payload))
        }).then(function (r) { return r.json(); });
      },
      wifi: function (body) {
        return fetch("/api/wifi", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(body)
        }).then(function (r) { return r.json(); });
      },
      reboot: function () { return fetch("/api/reboot", { method: "POST" }).then(function (r) { return r.json(); }); },
      reset: function () { return fetch("/api/reset", { method: "POST" }).then(function (r) { return r.json(); }); }
    };

    var ui = {
      el: {},
      state: null,
      wheel: { size: 240, dragging: false },

      init: function () {
        this.cacheDom();
        this.bind();
        this.refresh();
      },

      cacheDom: function () {
        var ids = ["wheel", "wheelKnob", "hueVal", "satVal", "swatch", "bright", "brightVal",
                   "speed", "speedVal", "modes", "presets", "ledMinus", "ledPlus",
                   "ledCount", "saveLeds", "status", "statusText", "tabLight", "tabWifi",
                   "paneLight", "paneWifi", "scan", "netlist", "ssid", "password",
                   "saveWifi", "reboot", "reset", "toast", "fw"];
        for (var i = 0; i < ids.length; i++) this.el[ids[i]] = document.getElementById(ids[i]);
      },

      toast: function (msg, kind) {
        var t = this.el.toast;
        t.textContent = msg;
        t.className = "show " + (kind || "");
        clearTimeout(this._toastTimer);
        this._toastTimer = setTimeout(function () { t.className = ""; }, 2600);
      },

      refresh: function () {
        var self = this;
        return api.state().then(function (doc) {
          self.state = pure.parseState(doc);
          self.render();
        }).catch(function (e) { self.toast("state error: " + e.message, "err"); });
      },

      render: function () {
        var st = this.state;
        if (!st) return;
        this.positionKnob(st.hue, st.saturation);
        this.swatchColor(st.hue, st.saturation, st.brightness);
        this.el.hueVal.textContent = Math.round(st.hue) + "\u00b0";
        this.el.satVal.textContent = Math.round(st.saturation * 100) + "%";
        this.el.bright.value = Math.round(st.brightness * 100);
        this.el.brightVal.textContent = Math.round(st.brightness * 100) + "%";
        this.el.speed.value = st.speed;
        this.el.speedVal.textContent = st.speed.toFixed(1) + "x";
        this.el.ledCount.textContent = st.ledCount;
        this.setActiveMode(st.mode);
        this.renderStatus(st);
        if (this.el.fw) this.el.fw.textContent = "fw " + st.fw;
      },

      renderStatus: function (st) {
        var el = this.el.status;
        el.className = "status " + (st.wifiConnected ? "ok" : (st.wifiMode === "ap" ? "ap" : ""));
        var txt;
        if (st.wifiConnected) txt = "STA \u00b7 " + st.ip + " \u00b7 " + st.ssid;
        else if (st.wifiMode === "ap") txt = "AP \u00b7 " + st.ip + " \u00b7 " + st.apName;
        else txt = "offline";
        this.el.statusText.textContent = txt;
      },

      positionKnob: function (hue, sat) {
        var wrap = this.el.wheel;   // the wheel-wrap div
        var rect = wrap.getBoundingClientRect();
        var size = Math.min(rect.width, rect.height);
        var r = size / 2;
        var p = pure.hueSatToWheel(hue, sat, r);
        var knob = this.el.wheelKnob;
        knob.style.left = (rect.width / 2 + p.x) + "px";
        knob.style.top = (rect.height / 2 + p.y) + "px";
      },

      swatchColor: function (hue, sat, bright) {
        var rgb = pure.hsvToRgb(hue, sat, bright);
        this.el.swatch.style.background = "rgb(" + rgb.r + "," + rgb.g + "," + rgb.b + ")";
      },

      wheelPoint: function (evt) {
        var wrap = this.el.wheel;
        var rect = wrap.getBoundingClientRect();
        var size = Math.min(rect.width, rect.height);
        var r = size / 2;
        var cx = rect.left + rect.width / 2;
        var cy = rect.top + rect.height / 2;
        var dx = evt.clientX - cx;
        var dy = evt.clientY - cy;
        return pure.wheelToHueSat(dx, dy, r);
      },

      onWheel: function (evt) {
        var self = this;
        if (!this.wheel.dragging) return;
        evt.preventDefault();
        var hs = this.wheelPoint(evt);
        this.pushSet({ hue: hs.hue, saturation: hs.saturation });
      },

      pushSet: function (partial) {
        var self = this;
        api.set(partial).then(function (doc) {
          if (doc && doc.ok) {
            self.state = pure.parseState(doc);
            self.render();
            if (doc.rebooting) { self.toast("Saving \u2014 rebooting\u2026", "ok"); }
          } else {
            self.toast((doc && doc.error) || "set failed", "err");
          }
        }).catch(function (e) { self.toast("set error: " + e.message, "err"); });
      },

      setActiveMode: function (mode) {
        var btns = this.el.modes.querySelectorAll("button");
        for (var i = 0; i < btns.length; i++) {
          btns[i].classList.toggle("active", btns[i].getAttribute("data-mode") === mode);
        }
      },

      renderPresets: function () {
        var self = this;
        var box = this.el.presets;
        box.innerHTML = "";
        pure.presets.forEach(function (p) {
          var b = document.createElement("button");
          var rgb = pure.hsvToRgb(p.hue, p.saturation, p.brightness);
          var dot = document.createElement("span");
          dot.className = "dot";
          dot.style.background = "rgb(" + rgb.r + "," + rgb.g + "," + rgb.b + ")";
          b.appendChild(dot);
          b.appendChild(document.createTextNode(p.name));
          b.addEventListener("click", function () {
            self.pushSet({
              hue: p.hue, saturation: p.saturation, brightness: p.brightness,
              mode: p.mode, speed: p.speed
            });
          });
          box.appendChild(b);
        });
      },

      switchTab: function (name) {
        this.el.tabLight.classList.toggle("active", name === "light");
        this.el.tabWifi.classList.toggle("active", name === "wifi");
        this.el.paneLight.classList.toggle("hidden", name !== "light");
        this.el.paneWifi.classList.toggle("hidden", name !== "wifi");
      },

      renderNetlist: function (networks) {
        var self = this;
        var box = this.el.netlist;
        box.innerHTML = "";
        if (!networks || !networks.length) {
          box.innerHTML = '<div class="empty">No networks found. Use the config page on port 8080.</div>';
          return;
        }
        networks.forEach(function (n) {
          var row = document.createElement("div");
          row.className = "row";
          var left = document.createElement("div");
          var ssid = document.createElement("div");
          ssid.className = "ssid";
          ssid.textContent = n.ssid;
          var meta = document.createElement("div");
          meta.className = "meta";
          meta.textContent = n.rssi + " dBm" + (n.locked ? " \u00b7 secured" : " \u00b7 open");
          left.appendChild(ssid);
          left.appendChild(meta);
          var lock = document.createElement("div");
          lock.className = "lock";
          lock.textContent = n.locked ? "\ud83d\udd12" : "\ud83d\udd13";
          row.appendChild(left);
          row.appendChild(lock);
          row.addEventListener("click", function () {
            box.querySelectorAll(".row").forEach(function (r) { r.classList.remove("sel"); });
            row.classList.add("sel");
            self.el.ssid.value = n.ssid;
            self.el.password.focus();
          });
          box.appendChild(row);
        });
      },

      bind: function () {
        var self = this;

        // Tabs.
        this.el.tabLight.addEventListener("click", function () { self.switchTab("light"); });
        this.el.tabWifi.addEventListener("click", function () { self.switchTab("wifi"); });

        // Wheel (pointer events for mouse + touch).
        var c = this.el.wheel;
        var start = function (evt) { self.wheel.dragging = true; self.onWheel(evt); };
        var move = function (evt) { self.onWheel(evt); };
        var end = function () { self.wheel.dragging = false; };
        c.addEventListener("pointerdown", start);
        window.addEventListener("pointermove", move);
        window.addEventListener("pointerup", end);
        c.addEventListener("touchstart", function (e) { e.preventDefault(); start(e.touches[0]); }, { passive: false });
        c.addEventListener("touchmove", function (e) { e.preventDefault(); self.onWheel(e.touches[0]); }, { passive: false });
        c.addEventListener("touchend", end);

        // Brightness + speed sliders.
        this.el.bright.addEventListener("input", function () {
          self.el.brightVal.textContent = this.value + "%";
          self.pushSet({ brightness: Number(this.value) / 100 });
        });
        this.el.speed.addEventListener("input", function () {
          self.el.speedVal.textContent = Number(this.value).toFixed(1) + "x";
          self.pushSet({ speed: Number(this.value) });
        });

        // Mode chips.
        this.el.modes.querySelectorAll("button").forEach(function (b) {
          b.addEventListener("click", function () { self.pushSet({ mode: b.getAttribute("data-mode") }); });
        });

        // LED count stepper.
        this.el.ledMinus.addEventListener("click", function () {
          var n = Math.max(1, (self.state ? self.state.ledCount : 1) - 1);
          self.el.ledCount.textContent = n;
        });
        this.el.ledPlus.addEventListener("click", function () {
          var n = Math.min(512, (self.state ? self.state.ledCount : 1) + 1);
          self.el.ledCount.textContent = n;
        });
        this.el.saveLeds.addEventListener("click", function () {
          var n = Number(self.el.ledCount.textContent);
          self.pushSet({ ledCount: n });
        });

        // Presets.
        this.renderPresets();

        // WiFi tab.
        this.el.scan.addEventListener("click", function () {
          self.el.scan.disabled = true;
          self.el.scan.textContent = "Scanning\u2026";
          api.wifi({ action: "scan" }).then(function (doc) {
            self.el.scan.disabled = false;
            self.el.scan.textContent = "Scan networks";
            if (doc && doc.ok) self.renderNetlist(doc.networks);
            else self.toast((doc && doc.error) || "scan failed", "err");
          }).catch(function (e) {
            self.el.scan.disabled = false;
            self.el.scan.textContent = "Scan networks";
            self.toast("scan error: " + e.message, "err");
          });
        });
        this.el.saveWifi.addEventListener("click", function () {
          var ssid = self.el.ssid.value.trim();
          if (!ssid) { self.toast("enter an SSID", "err"); return; }
          self.el.saveWifi.disabled = true;
          api.wifi({ action: "save", ssid: ssid, password: self.el.password.value }).then(function (doc) {
            if (doc && doc.ok) self.toast("Saved \u2014 rebooting\u2026", "ok");
            else self.toast((doc && doc.error) || "save failed", "err");
          }).catch(function (e) { self.toast("save error: " + e.message, "err"); })
            .finally(function () { self.el.saveWifi.disabled = false; });
        });
        this.el.reboot.addEventListener("click", function () {
          if (!window.confirm("Reboot the Ferrocell now?")) return;
          api.reboot().then(function (doc) {
            if (doc && doc.ok) self.toast("Rebooting\u2026", "ok");
            else self.toast((doc && doc.error) || "reboot failed", "err");
          }).catch(function (e) { self.toast("reboot error: " + e.message, "err"); });
        });
        this.el.reset.addEventListener("click", function () {
          if (!window.confirm("Reset saved WiFi credentials and reboot?")) return;
          api.reset().then(function (doc) {
            if (doc && doc.ok) self.toast("Reset \u2014 rebooting\u2026", "ok");
            else self.toast((doc && doc.error) || "reset failed", "err");
          }).catch(function (e) { self.toast("reset error: " + e.message, "err"); });
        });
      }
    };

    FerrocellUI.ui = ui;
    FerrocellUI.api = api;

    if (document.readyState === "loading") {
      document.addEventListener("DOMContentLoaded", function () { ui.init(); });
    } else {
      ui.init();
    }
  }

  // Export for the M6 Node harness (pure part only) + browser global.
  if (typeof module !== "undefined" && module.exports) module.exports = FerrocellUI;
  global.FerrocellUI = FerrocellUI;
})(typeof window !== "undefined" ? window : globalThis);
