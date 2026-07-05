#pragma once

const char WEB_UI[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="el"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 Control</title>
<style>
:root{--bg:#0a0f1a;--surface:#111827;--s2:#1a2332;--bd:#243044;--tx:#f1f5f9;--mut:#94a3b8;--acc:#3b82f6;--ok:#22c55e;--warn:#f59e0b;--bad:#ef4444}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--tx);padding:14px;max-width:820px;margin:0 auto;line-height:1.4}
h1{font-size:1.2rem;display:flex;align-items:center;gap:10px;margin-bottom:4px}
.logo{width:34px;height:34px;border-radius:9px;background:linear-gradient(135deg,var(--acc),#6366f1);display:flex;align-items:center;justify-content:center;font-size:18px}
.sub{color:var(--mut);font-size:.78rem;margin-bottom:12px}
.dot{width:9px;height:9px;border-radius:50%;background:var(--mut);display:inline-block}
.dot.on{background:var(--ok);box-shadow:0 0 6px var(--ok)}
.card{background:var(--surface);border:1px solid var(--bd);padding:14px;border-radius:12px;margin:10px 0}
.card h2{font-size:.72rem;text-transform:uppercase;letter-spacing:.05em;color:var(--mut);margin-bottom:10px;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:8px}
.kpi{background:var(--s2);border-radius:9px;padding:9px 10px}
.kpi .l{font-size:.66rem;color:var(--mut)}
.kpi .v{font-size:1.05rem;font-weight:700;margin-top:2px}
.kpi .u{font-size:.7rem;color:var(--mut);font-weight:400}
label{display:block;font-size:.72rem;color:var(--mut);margin:8px 0 3px}
button,input,select{font-family:inherit;font-size:.85rem}
input,select{width:100%;padding:9px 10px;border-radius:9px;border:1px solid var(--bd);background:var(--s2);color:var(--tx)}
button{padding:10px 14px;border:0;border-radius:9px;background:var(--acc);color:#fff;font-weight:600;cursor:pointer;width:100%;margin-top:8px}
button.sec{background:var(--s2);border:1px solid var(--bd);color:var(--tx)}
button.ok{background:var(--ok)}button.bad{background:var(--bad)}
.row{display:flex;gap:8px;flex-wrap:wrap}.row>*{flex:1;min-width:90px}
.pill{display:inline-block;padding:3px 9px;border-radius:999px;font-size:.68rem;font-weight:600}
.pill.ok{background:#052e16;color:#86efac}.pill.bad{background:#450a0a;color:#fca5a5}.pill.off{background:#1e293b;color:var(--mut)}
.tog{display:flex;justify-content:space-between;align-items:center;padding:9px 0;border-bottom:1px solid var(--bd)}
.tog:last-child{border:0}
.sw{position:relative;width:44px;height:24px}.sw input{display:none}
.sl{position:absolute;inset:0;background:#334155;border-radius:24px;cursor:pointer;transition:.2s}
.sl:before{content:"";position:absolute;width:18px;height:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:.2s}
.sw input:checked+.sl{background:var(--ok)}.sw input:checked+.sl:before{transform:translateX(20px)}
.msg{font-size:.75rem;padding:8px 10px;border-radius:8px;margin-top:8px;display:none}
.msg.show{display:block}.msg.ok{background:#052e16;color:#86efac}.msg.bad{background:#450a0a;color:#fca5a5}.msg.info{background:#0c2c4a;color:#93c5fd}
.bd{border-top:1px solid var(--bd);margin:10px 0}
small{color:var(--mut);font-size:.7rem}
details summary{cursor:pointer;font-size:.75rem;color:var(--mut)}
pre{white-space:pre-wrap;font-size:11px;max-height:40vh;overflow:auto;background:var(--s2);padding:10px;border-radius:8px;margin-top:8px}
.dev{background:var(--s2);border:1px solid var(--bd);border-radius:8px;padding:8px 10px;margin:5px 0;cursor:pointer;font-size:.8rem}
.dev:hover{border-color:var(--acc)}
</style></head><body>
<h1><span class="logo">&#9889;</span> ESP32 Control</h1>
<div class="sub"><span class="dot" id="dot"></span> <span id="hdr">Σύνδεση...</span></div>

<div class="card">
<h2>Μπαταρία (JK BMS)</h2>
<div id="bmsChip"><span class="pill off">—</span></div>
<div class="grid" style="margin-top:10px">
<div class="kpi"><div class="l">SOC</div><div class="v"><span id="bSoc">—</span><span class="u">%</span></div></div>
<div class="kpi"><div class="l">Τάση</div><div class="v"><span id="bV">—</span><span class="u">V</span></div></div>
<div class="kpi"><div class="l">Ρεύμα</div><div class="v"><span id="bA">—</span><span class="u">A</span></div></div>
<div class="kpi"><div class="l">Ισχύς</div><div class="v"><span id="bW">—</span><span class="u">W</span></div></div>
<div class="kpi"><div class="l">Διαθέσιμα</div><div class="v"><span id="bAh">—</span><span class="u">Ah</span></div></div>
<div class="kpi"><div class="l">Θερμ.</div><div class="v"><span id="bT">—</span><span class="u">°C</span></div></div>
</div>
</div>

<div class="card">
<h2>Γεννήτρια (PS0600)</h2>
<div id="genChip"><span class="pill off">—</span></div>
<div class="grid" style="margin-top:10px">
<div class="kpi"><div class="l">Κατάσταση</div><div class="v" style="font-size:.85rem" id="gState">—</div></div>
<div class="kpi"><div class="l">Τάση L-L</div><div class="v"><span id="gV">—</span><span class="u">V</span></div></div>
<div class="kpi"><div class="l">Συχνότητα</div><div class="v"><span id="gHz">—</span><span class="u">Hz</span></div></div>
<div class="kpi"><div class="l">Ισχύς</div><div class="v"><span id="gKw">—</span><span class="u">kW</span></div></div>
<div class="kpi"><div class="l">RPM</div><div class="v" id="gRpm">—</div></div>
<div class="kpi"><div class="l">Μπαταρία</div><div class="v"><span id="gBatt">—</span><span class="u">V</span></div></div>
</div>
<div id="genErr" class="msg bad"></div>
</div>

<div class="card">
<h2>Ρυθμίσεις Modbus</h2>
<div class="row">
<div><label>Profile</label><select id="mp"><option value="ps0600">PS0600 Genset</option><option value="entes">ENTES MPR-46S</option></select></div>
<div><label>Baud</label><select id="mbaud"><option>9600</option><option>19200</option><option>38400</option><option>57600</option><option>115200</option></select></div>
</div>
<div class="row">
<div><label>Slave ID</label><input type="number" id="msid" min="1" max="247" value="1"></div>
<div><label>Ενεργό</label><select id="me"><option value="1">Ναι</option><option value="0">Όχι</option></select></div>
</div>
<button onclick="saveM()">Αποθήκευση Modbus</button>
<button class="sec" onclick="scanM()">&#128269; Auto-scan baud/slave</button>
<div id="mMsg" class="msg"></div>
</div>

<div class="card">
<h2>Επαφές / Relays</h2>
<div id="relays"><small>—</small></div>
</div>

<div class="card">
<h2>Bluetooth BMS</h2>
<button class="sec" onclick="bleScan()">&#128246; BLE scan</button>
<div id="ble" style="margin-top:8px"></div>
<button class="sec" onclick="bleDisc()">Αποσύνδεση / διαγραφή BMS</button>
</div>

<div class="card">
<h2>Ενημέρωση firmware (OTA)</h2>
<input type="file" id="bin" accept=".bin">
<button onclick="ota()">Ανέβασμα .bin</button>
<div id="oMsg" class="msg"></div>
<small>Ή απομακρυσμένα από GitHub Pages dashboard.</small>
</div>

<details><summary>Πλήρες JSON κατάστασης</summary><pre id="raw">...</pre></details>

<script>
var E=function(i){return document.getElementById(i)};
function j(u,o){return fetch(u,o).then(function(r){return r.json()})}
function msg(id,txt,cls){var m=E(id);m.textContent=txt;m.className='msg show '+cls}
function f(x,d){return (x!=null&&isFinite(x))?Number(x).toFixed(d):'—'}
var editing=false;
['mp','mbaud','msid','me'].forEach(function(id){
  document.addEventListener('focusin',function(e){if(e.target.id===id)editing=true});
});
document.addEventListener('focusout',function(){setTimeout(function(){editing=false},400)});

function go(){
 j('/api/status').then(function(s){
  E('raw').textContent=JSON.stringify(s,null,2);
  E('dot').className='dot on';
  E('hdr').textContent=(s.wifi_ssid||'?')+' · '+(s.ip||'?')+' · '+(s.rssi!=null?s.rssi+'dBm':'');
  var b=s.bms||{},ble=s.ble||{};
  var bv=b.valid;
  E('bmsChip').innerHTML=bv?'<span class="pill ok">ONLINE '+(b.device_model||'')+'</span>':(ble.connected?'<span class="pill off">BLE OK, αναμονή</span>':'<span class="pill bad">OFFLINE</span>');
  E('bSoc').textContent=bv?f(b.soc,0):'—';
  E('bV').textContent=bv?f(b.voltage,2):'—';
  E('bA').textContent=bv?f(b.current,2):'—';
  E('bW').textContent=bv?f(b.power,0):'—';
  E('bAh').textContent=bv?f(b.remaining_ah,1):'—';
  E('bT').textContent=bv?f(b.avg_temp,1):'—';
  var g=s.genset||{},gv=g.valid;
  E('genChip').innerHTML=gv?'<span class="pill ok">'+(g.running?'RUNNING':'READY')+'</span>':'<span class="pill bad">OFFLINE</span>';
  E('gState').textContent=gv?(g.genset_state_label_el||g.genset_state_label||'—'):'—';
  E('gV').textContent=gv?f(g.volt_avg_ll,0):'—';
  E('gHz').textContent=gv?f(g.frequency,2):'—';
  E('gKw').textContent=gv?f(g.kw_total,1):'—';
  E('gRpm').textContent=gv?f(g.engine_rpm,0):'—';
  E('gBatt').textContent=gv?f(g.battery_v,1):'—';
  var ge=E('genErr');
  if(!gv&&g.error){ge.textContent=g.error+(g.scan_result?(' · '+g.scan_result):'');ge.className='msg show bad'}
  else if(g.scan_result){ge.textContent=g.scan_result;ge.className='msg show info'}
  else{ge.className='msg'}
  if(!editing){
   E('me').value=g.enabled?'1':'0';
   var pr=(g.profile||'').toLowerCase();E('mp').value=pr.indexOf('entes')>=0?'entes':'ps0600';
   if(g.slave_id!=null)E('msid').value=g.slave_id;
   if(g.baud!=null)E('mbaud').value=String(g.baud);
  }
  var rl=s.outputs||[];
  E('relays').innerHTML=rl.length?rl.map(function(o,i){
   return '<div class="tog"><span>'+o.name+' <small>GPIO'+o.gpio+'</small></span><label class="sw"><input type="checkbox" '+(o.on?'checked':'')+' onchange="setR('+i+',this.checked)"><span class="sl"></span></label></div>'
  }).join(''):'<small>—</small>';
 }).catch(function(){E('dot').className='dot';E('hdr').textContent='offline'})
}
function saveM(){
 j('/api/modbus',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled:E('me').value==='1',profile:E('mp').value,slave_id:+E('msid').value,baud:+E('mbaud').value})})
 .then(function(){msg('mMsg','Αποθηκεύτηκε','ok');go()});
}
function scanM(){
 msg('mMsg','Σάρωση baud/slave... (~10s)','info');
 j('/api/modbus/scan',{method:'POST'}).then(function(r){
  msg('mMsg',(r.ok?'✓ ':'✗ ')+(r.result||''),r.ok?'ok':'bad');go();
 }).catch(function(){msg('mMsg','Σφάλμα σάρωσης','bad')});
}
function setR(i,on){j('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:i,on:on})}).then(go)}
function bleScan(){E('ble').innerHTML='<small>scan...</small>';
 j('/api/ble/scan').then(function(r){
  E('ble').innerHTML=(r.devices||[]).map(function(d){
   return '<div class="dev" onclick="bleConn(\''+d.mac+'\',\''+(d.name||'')+'\')">'+(d.name||'(no name)')+' <small>'+d.mac+' '+(d.rssi||'')+'</small></div>'
  }).join('')||'<small>Καμία συσκευή</small>';
 })}
function bleConn(mac,name){j('/api/ble/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mac:mac,name:name,type:'jk'})}).then(go)}
function bleDisc(){if(confirm('Διαγραφή αποθηκευμένου BMS;'))j('/api/ble/disconnect',{method:'POST'}).then(go)}
function ota(){var fl=E('bin').files[0];if(!fl){msg('oMsg','Διάλεξε .bin','bad');return}
 msg('oMsg','Ανέβασμα...','info');var fd=new FormData();fd.append('firmware',fl);
 fetch('/api/ota',{method:'POST',body:fd}).then(function(r){return r.json()}).then(function(){msg('oMsg','OK - reboot','ok')}).catch(function(){msg('oMsg','Σφάλμα','bad')});
}
go();setInterval(go,3000);
</script>
</body></html>
)rawliteral";
