#pragma once

const char REMOTE_UI[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="el">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0f172a">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>ESP32 Control</title>
<style>
*{box-sizing:border-box}body{margin:0;font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0}
header{padding:16px 20px;background:#1e293b;border-bottom:1px solid #334155}
h1{margin:0;font-size:1.2rem}
main{padding:16px;max-width:560px;margin:0 auto}
.card{background:#1e293b;border-radius:12px;padding:16px;margin:12px 0}
input{width:100%;padding:10px;border-radius:8px;border:1px solid #475569;background:#0f172a;color:#e2e8f0;margin:6px 0}
button{padding:12px;border:0;border-radius:8px;background:#2563eb;color:#fff;width:100%;margin-top:8px;cursor:pointer}
button.sec{background:#475569}
.badge{display:inline-block;padding:4px 10px;border-radius:999px;font-size:.75rem;margin-left:8px}
.ok{background:#14532d;color:#86efac}.warn{background:#713f12;color:#fde047}.off{background:#475569;color:#cbd5e1}
.toggle{display:flex;justify-content:space-between;align-items:center;padding:10px 0;border-bottom:1px solid #334155}
.toggle:last-child{border:0}
.switch{position:relative;width:52px;height:28px}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;inset:0;background:#475569;border-radius:28px;cursor:pointer;transition:.2s}
.slider:before{content:"";position:absolute;height:22px;width:22px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.2s}
input:checked+.slider{background:#22c55e}
input:checked+.slider:before{transform:translateX(24px)}
pre{background:#0f172a;padding:12px;border-radius:8px;white-space:pre-wrap;font-size:.75rem;max-height:420px;overflow:auto}
.small{color:#94a3b8;font-size:.8rem}
</style>
</head>
<body>
<header>
  <h1>ESP32 Control <span id="badge" class="badge off">...</span></h1>
  <div class="small" id="info">Αυτόματη σύνδεση...</div>
</header>
<main>
<div class="card" id="setupCard" style="display:none">
  <p class="small">Πρώτη φορά: βάλε Device ID από το σπίτι (http://esp32.local → Internet)</p>
  <label>Device ID</label>
  <input id="deviceId" placeholder="π.χ. 38182B8BD5CC">
  <button onclick="saveIdAndConnect()">Αποθήκευση &amp; Σύνδεση</button>
</div>
<div class="card"><h2 style="margin:0 0 12px">BMS</h2><pre id="bms">-</pre></div>
<div class="card"><h2 style="margin:0 0 12px">Επαφές</h2><div id="relays"></div>
<button class="sec" onclick="allOff()">Όλα OFF</button></div>
<div class="card">
  <p class="small" id="modeInfo">Σπίτι → τοπικό WiFi. Έξω → MQTT αυτόματα.</p>
  <button class="sec" onclick="autoConnect(true)">Επανασύνδεση</button>
</div>
</main>
<script src="https://unpkg.com/mqtt@5.3.5/dist/mqtt.min.js"></script>
<script>
const LS={id:'esp32_device_id',ip:'esp32_ip',broker:'esp32_ws_broker'};
let client=null,cmdTopic='',mode='offline',localBase='',outputs=[];
const WS_DEFAULT='wss://broker.hivemq.com:8884/mqtt';

function formatBms(b){
  if(!b||!b.valid)return'Αναμονή δεδομένων BMS...';
  let s='';
  if(b.type_label)s+=b.type_label+'\n';
  if(b.name)s+='Device: '+b.name+'\n';
  s+=`SOC: ${b.soc}%  |  SOH: ${b.soh}%\n`;
  s+=`Τάση: ${Number(b.voltage).toFixed(2)} V  |  Ρεύμα: ${Number(b.current).toFixed(2)} A\n`;
  s+=`Ισχύς: ${Number(b.power).toFixed(0)} W\n`;
  if(b.min_cell_v>0)s+=`Cells: ${Number(b.min_cell_v).toFixed(3)}-${Number(b.max_cell_v).toFixed(3)}V\n`;
  if(b.cells&&b.cells.length){
    s+='--- Cells ---\n';
    b.cells.forEach((v,i)=>{if(v>0.5){s+=`C${i+1}:${Number(v).toFixed(3)}V `;if((i+1)%4===0)s+='\n';}});
  }
  return s;
}

function setBadge(text,cls){
  const b=document.getElementById('badge');
  b.textContent=text;
  b.className='badge '+cls;
}

function saveProfile(s){
  if(s.device_id)localStorage.setItem(LS.id,s.device_id.toUpperCase());
  if(s.ip)localStorage.setItem(LS.ip,s.ip);
  localStorage.setItem(LS.broker,WS_DEFAULT);
  if(s.device_id)document.getElementById('deviceId').value=s.device_id;
}

async function fetchLocal(base){
  const c=new AbortController();
  const t=setTimeout(()=>c.abort(),2500);
  try{
    const r=await fetch(base+'/api/status',{signal:c.signal});
    clearTimeout(t);
    if(!r.ok)throw 0;
    return await r.json();
  }catch(e){clearTimeout(t);throw e;}
}

async function tryLocal(){
  const bases=[];
  const ip=localStorage.getItem(LS.ip);
  if(ip)bases.push('http://'+ip);
  bases.push('http://esp32.local');
  if(location.hostname&&location.hostname!=='esp32.local'&&!location.hostname.startsWith('192.168.'))
    bases.push('http://'+location.hostname);
  for(const b of bases){
    try{const s=await fetchLocal(b);return{base:b,status:s};}
    catch(e){}
  }
  return null;
}

function applyStatus(s){
  outputs=s.outputs||[];
  renderRelays();
  if(s.ble&&s.ble.data&&s.ble.data.length>10)document.getElementById('bms').textContent=s.ble.data;
  else if(s.bms&&s.bms.valid)document.getElementById('bms').textContent=formatBms(s.bms);
  document.getElementById('info').textContent=`${s.device_id} | ${s.ip||'-'} | ${s.wifi_ssid||''}`;
  saveProfile(s);
  document.getElementById('setupCard').style.display='none';
}

function renderRelays(){
  document.getElementById('relays').innerHTML=outputs.map((o,i)=>`
  <div class="toggle"><span>${o.name}</span>
  <label class="switch"><input type="checkbox" ${o.on?'checked':''} onchange="setRelay(${i},this.checked)"><span class="slider"></span></label></div>`).join('');
}

function disconnectMqtt(){
  if(client){client.end();client=null;}
}

function connectMqtt(id){
  disconnectMqtt();
  const url=localStorage.getItem(LS.broker)||WS_DEFAULT;
  cmdTopic=`home/${id}/cmd`;
  client=mqtt.connect(url,{clientId:'app-'+Math.random().toString(16).slice(2),reconnectPeriod:5000});
  client.on('connect',()=>{
    if(mode==='mqtt'){setBadge('Remote','ok');document.getElementById('modeInfo').textContent='Συνδεδεμένο μέσω Internet (MQTT)';}
    client.subscribe(`home/${id}/status`);
    client.subscribe(`home/${id}/bms`);
  });
  client.on('close',()=>{if(mode==='mqtt')setBadge('Offline','off');});
  client.on('message',(t,msg)=>{
    if(mode!=='mqtt')return;
    const j=JSON.parse(msg.toString());
    if(t.endsWith('/status')&&j.outputs){outputs=j.outputs;renderRelays();}
    if(t.endsWith('/bms')&&j.valid)document.getElementById('bms').textContent=formatBms(j);
    if(t.endsWith('/status')&&j.bms&&j.bms.valid)document.getElementById('bms').textContent=formatBms(j.bms);
    if(t.endsWith('/status')&&j.device_id)saveProfile(j);
  });
}

async function autoConnect(force){
  const local=await tryLocal();
  if(local){
    mode='local';
    localBase=local.base;
    applyStatus(local.status);
    const id=local.status.device_id||localStorage.getItem(LS.id);
    if(id&&!client)connectMqtt(id.toUpperCase());
    setBadge('Σπίτι (Local)','ok');
    document.getElementById('modeInfo').textContent='Στο τοπικό WiFi — άμεση σύνδεση. Remote ενεργό όταν φύγεις.';
    return;
  }
  mode='mqtt';
  localBase='';
  const id=(document.getElementById('deviceId').value||localStorage.getItem(LS.id)||'').trim().toUpperCase();
  if(!id){
    document.getElementById('setupCard').style.display='block';
    setBadge('Χρειάζεται ID','warn');
    document.getElementById('modeInfo').textContent='Άνοιξε μία φορά http://esp32.local από σπίτι για αυτόματη ρύθμιση.';
    return;
  }
  connectMqtt(id);
  setBadge('Remote...','warn');
  document.getElementById('modeInfo').textContent='Εκτός σπιτιού — σύνδεση MQTT...';
}

function saveIdAndConnect(){
  const id=document.getElementById('deviceId').value.trim().toUpperCase();
  if(!id)return alert('Βάλε Device ID');
  localStorage.setItem(LS.id,id);
  autoConnect(true);
}

async function setRelay(i,on){
  if(mode==='local'&&localBase){
    await fetch(localBase+'/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:i,on})});
    const s=await fetchLocal(localBase);applyStatus(s);
    return;
  }
  if(client)client.publish(cmdTopic,JSON.stringify({relay:i,on}));
}

function allOff(){
  if(mode==='local'&&localBase){fetch(localBase+'/api/relay/alloff',{method:'POST'});return;}
  if(client)client.publish(cmdTopic,JSON.stringify({relay:'all',on:false}));
}

const saved=localStorage.getItem(LS.id);
if(saved)document.getElementById('deviceId').value=saved;
autoConnect(false);
setInterval(()=>autoConnect(false),15000);
setInterval(async()=>{
  if(mode!=='local'||!localBase)return;
  try{const s=await fetchLocal(localBase);applyStatus(s);}catch(e){autoConnect(true);}
},3000);
</script>
</body>
</html>
)rawliteral";
