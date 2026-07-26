#pragma once

// Minimal local page for Classic (flash budget). Full UI is on GitHub Pages.
const char WEB_UI[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="el"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Classic</title>
<style>
body{font-family:system-ui,sans-serif;background:#0a0f1a;color:#e2e8f0;margin:0;padding:20px;max-width:420px}
a{color:#60a5fa}code{background:#1e293b;padding:2px 6px;border-radius:4px}
.card{background:#111827;border:1px solid:#243044;border-radius:12px;padding:16px;margin-top:14px}
</style></head><body>
<h1>ESP32 Classic</h1>
<p>Τοπικό UI είναι slim. Χρησιμοποίησε το cloud dashboard:</p>
<p><a href="https://papazachariakis.github.io/ESP32/esp32.html">Άνοιγμα dashboard</a></p>
<div class="card">
<div>IP: <code id="ip">…</code></div>
<div>FW: <code id="fw">…</code></div>
<div>ID: <code id="id">…</code></div>
</div>
<script>
fetch('/api/status').then(r=>r.json()).then(j=>{
  ip.textContent=j.ip||'—'; fw.textContent=j.firmware||'—'; id.textContent=j.device_id||'—';
}).catch(()=>{});
</script>
</body></html>
)rawliteral";
