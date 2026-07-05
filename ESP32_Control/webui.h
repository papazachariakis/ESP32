#pragma once

const char WEB_UI[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="el"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32</title>
<style>body{font-family:system-ui;background:#0f172a;color:#e2e8f0;margin:0;padding:12px}
a{color:#60a5fa}button,input,select{width:100%;padding:8px;margin:6px 0}
.card{background:#1e293b;padding:12px;border-radius:8px;margin:8px 0}
pre{white-space:pre-wrap;font-size:12px;max-height:50vh;overflow:auto}</style>
</head><body>
<h1>ESP32 Control</h1>
<div class="card"><pre id="st">...</pre></div>
<div class="card">
<button onclick="scan()">BLE scan</button><div id="ble"></div>
<label>Modbus <input type="checkbox" id="me" checked></label>
<select id="mp"><option value="ps0600">PS0600 Genset</option><option value="entes">ENTES MPR-46S</option></select>
<label>Slave ID <input type="number" id="msid" min="1" max="247" value="1" style="width:100%"></label>
<label>Baud <select id="mbaud"><option value="9600">9600</option><option value="19200">19200</option><option value="38400">38400</option><option value="57600">57600</option><option value="115200">115200</option></select></label>
<button onclick="saveM()">Save Modbus</button>
<input type="file" id="bin" accept=".bin"><button onclick="ota()">OTA upload</button>
</div>
<p><a href="/api/status" target="_blank">JSON</a> | Remote OTA: GitHub Pages</p>
<script>
async function j(u,o){const r=await fetch(u,o);return r.json()}
async function go(){try{const s=await j('/api/status');st.textContent=JSON.stringify(s,null,2);
me.checked=!!s.genset?.enabled;
const pr=(s.genset?.profile||'').toLowerCase();
mp.value=pr.includes('entes')?'entes':'ps0600';
msid.value=s.genset?.slave_id||1;
mbaud.value=String(s.genset?.baud||9600)}catch(e){st.textContent='offline'}}
async function scan(){ble.textContent='scan...';const r=await j('/api/ble/scan');
ble.innerHTML=(r.devices||[]).map(d=>`<button onclick="conn('${d.mac}','${d.name||''}')">${d.name||d.mac}</button>`).join('')}
async function conn(mac,name){await j('/api/ble/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mac,name,type:'jk'})});go()}
async function saveM(){await j('/api/modbus',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:me.checked,profile:mp.value,slave_id:+msid.value,baud:+mbaud.value})});go()}
async function ota(){const f=bin.files[0];if(!f)return alert('pick .bin');const fd=new FormData();fd.append('firmware',f);
await fetch('/api/ota',{method:'POST',body:fd});alert('uploading...')}
go();setInterval(go,4000)
</script></body></html>
)rawliteral";
