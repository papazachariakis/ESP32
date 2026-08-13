#pragma once

// Minimal local page for Classic (flash budget). Full UI is on GitHub Pages.
// Shows live Basen BMS from /api/status — works on LAN without cloud MQTT.
const char WEB_UI[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="el"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Classic</title>
<style>
body{font-family:system-ui,sans-serif;background:#0a0f1a;color:#e2e8f0;margin:0;padding:20px;max-width:420px}
a{color:#60a5fa}code{background:#1e293b;padding:2px 6px;border-radius:4px}
.card{background:#111827;border:1px solid:#243044;border-radius:12px;padding:16px;margin-top:14px}
.muted{color:#94a3b8;font-size:.85rem;line-height:1.45}
.row{display:flex;justify-content:space-between;gap:12px;padding:6px 0;border-bottom:1px solid #1e293b}
.row:last-child{border-bottom:0}
.k{color:#94a3b8}.v{font-weight:700;font-variant-numeric:tabular-nums}
.ok{color:#22c55e}.warn{color:#f59e0b}.bad{color:#ef4444}
.soc{font-size:2.4rem;font-weight:800;letter-spacing:-.02em}
</style></head><body>
<h1>ESP32 Classic</h1>
<p class="muted">Τοπική οθόνη Basen (LAN). Cloud dashboard: HiveMQ <code>home/&lt;ID&gt;/status</code> + τοπικό Mosquitto HA.</p>
<p><a href="https://papazachariakis.github.io/ESP32/esp32.html">Cloud dashboard</a> · <a href="/api/status">/api/status</a></p>
<div class="card">
  <div class="soc" id="soc">—%</div>
  <div class="muted" id="bmsMeta">Αναμονή BMS…</div>
  <div class="row"><span class="k">Τάση</span><span class="v" id="v">— V</span></div>
  <div class="row"><span class="k">Ρεύμα</span><span class="v" id="a">— A</span></div>
  <div class="row"><span class="k">Ισχύς</span><span class="v" id="w">— W</span></div>
  <div class="row"><span class="k">Ah</span><span class="v" id="ah">—</span></div>
  <div class="row"><span class="k">BLE</span><span class="v" id="ble">—</span></div>
</div>
<div class="card">
  <div class="row"><span class="k">IP</span><span class="v"><code id="ip">…</code></span></div>
  <div class="row"><span class="k">FW</span><span class="v"><code id="fw">…</code></span></div>
  <div class="row"><span class="k">ID</span><span class="v"><code id="id">…</code></span></div>
  <div class="row"><span class="k">MQTT</span><span class="v" id="mq">—</span></div>
</div>
<script>
function n(x,d){return (x==null||x==='')?'—':Number(x).toFixed(d)}
function tick(){
  fetch('/api/status').then(r=>r.json()).then(j=>{
    const b=j.bms||{}, ble=j.ble||{}, mq=j.mqtt||{};
    ip.textContent=j.ip||'—'; fw.textContent=j.firmware||'—'; id.textContent=j.device_id||'—';
    mq.textContent=(mq.connected?'OK · ':'OFF · ')+(mq.broker||'?')+':'+(mq.port||'');
    mq.className='v '+(mq.connected?'ok':'bad');
    if(b.valid){
      soc.textContent=(b.soc!=null?b.soc:'—')+'%';
      soc.className='soc ok';
      bmsMeta.textContent=(b.name||b.type_label||'Basen')+(b.discharging?' · εκφόρτιση':b.charging?' · φόρτιση':'');
      v.textContent=n(b.voltage,2)+' V';
      a.textContent=n(b.current,2)+' A';
      w.textContent=n(b.power,0)+' W';
      ah.textContent=n(b.remaining_ah,1)+' / '+n(b.capacity_ah,0)+' Ah';
    }else{
      soc.textContent='—%'; soc.className='soc';
      bmsMeta.textContent='Χωρίς valid BMS — περίμενε BLE';
    }
    ble.textContent=(ble.connected?'συνδεδεμένο':'αποσυνδ.')+' · '+(ble.mac||'—');
    ble.className='v '+(ble.connected?'ok':'warn');
  }).catch(()=>{ bmsMeta.textContent='Σφάλμα /api/status'; });
}
tick(); setInterval(tick,1500);
</script>
</body></html>
)rawliteral";
