#pragma once

const char WEB_UI[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="el">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<meta name="theme-color" content="#0f172a">
<title>ESP32 Control</title>
<style>
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,sans-serif;background:#0f172a;color:#e2e8f0}
header{padding:16px 20px;background:#1e293b;border-bottom:1px solid #334155}
header h1{margin:0;font-size:1.25rem}
.badge{display:inline-block;padding:4px 10px;border-radius:999px;font-size:.75rem;margin-left:8px}
.ok{background:#14532d;color:#86efac}.warn{background:#713f12;color:#fde047}
nav{display:flex;gap:4px;padding:12px 16px;overflow:auto;background:#1e293b}
nav button{flex:1;min-width:80px;padding:10px;border:0;border-radius:8px;background:#334155;color:#e2e8f0;cursor:pointer}
nav button.active{background:#2563eb;color:#fff}
main{padding:16px;max-width:560px;margin:0 auto}
.card{background:#1e293b;border-radius:12px;padding:16px;margin-bottom:12px}
.card h2{margin:0 0 12px;font-size:1rem}
.row{display:flex;gap:8px;align-items:center;margin:8px 0}
.row label{flex:1;font-size:.9rem;color:#94a3b8}
input,select{width:100%;padding:10px;border-radius:8px;border:1px solid #475569;background:#0f172a;color:#e2e8f0;margin:6px 0}
button.btn{padding:12px 16px;border:0;border-radius:8px;background:#2563eb;color:#fff;cursor:pointer;width:100%;margin-top:8px}
button.btn.secondary{background:#475569}
button.btn.danger{background:#dc2626}
button.btn.success{background:#16a34a}
.toggle{display:flex;justify-content:space-between;align-items:center;padding:12px 0;border-bottom:1px solid #334155}
.toggle:last-child{border:0}
.switch{position:relative;width:52px;height:28px}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;inset:0;background:#475569;border-radius:28px;cursor:pointer;transition:.2s}
.slider:before{content:"";position:absolute;height:22px;width:22px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.2s}
input:checked+.slider{background:#22c55e}
input:checked+.slider:before{transform:translateX(24px)}
pre{background:#0f172a;padding:12px;border-radius:8px;overflow:auto;font-size:.75rem;max-height:420px;white-space:pre-wrap}
.tab{display:none}.tab.active{display:block}
.small{font-size:.8rem;color:#94a3b8}
.ble-item{padding:10px;background:#0f172a;border-radius:8px;margin:6px 0;cursor:pointer}
.ble-item:hover{background:#334155}
</style>
</head>
<body>
<header>
  <h1>ESP32 Control <span id="connBadge" class="badge warn">...</span></h1>
  <div class="small" id="deviceInfo"></div>
</header>
<nav>
  <button class="active" data-tab="dash">Dashboard</button>
  <button data-tab="genset">Γεννήτρια</button>
  <button data-tab="ble">Bluetooth</button>
  <button data-tab="wifi">WiFi</button>
  <button data-tab="remote">Internet</button>
</nav>
<main>
  <section id="tab-dash" class="tab active">
    <div class="card"><h2>Επαφές / Relays</h2><div id="relays"></div>
    <button class="btn secondary" onclick="allOff()">Όλα OFF</button></div>
    <div class="card"><h2>BMS Battery</h2><pre id="bmsData">Δεν υπάρχουν δεδομένα</pre></div>
  </section>
  <section id="tab-genset" class="tab">
    <div class="card"><h2>Cummins Modbus (RS485)</h2>
    <p class="small">PCC 1301 / PowerCommand 1.x / PS0500. Σύνδεση MAX485: RO→GPIO16, DI→GPIO17, DE+RE→GPIO19, A/B στο Cummins RS485.</p>
    <label><input type="checkbox" id="modbusEn" checked> Ενεργό</label>
    <label>Slave ID</label><input id="modbusId" type="number" value="1" min="1" max="247">
    <label>Baud</label><select id="modbusBaud"><option>9600</option><option>19200</option><option>38400</option><option>2400</option><option>4800</option></select>
    <button class="btn success" onclick="saveModbus()">Αποθήκευση Modbus</button>
    <p class="small" id="modbusStatus">-</p></div>
    <div class="card"><h2>Έλεγχος γεννήτριας</h2>
    <p class="small">Modbus write: 40300 start/stop, 40301 reset, 40302 e-stop</p>
    <button class="btn success" onclick="genCmd('start')">Εκκίνηση</button>
    <button class="btn danger" onclick="genCmd('stop')">Διακοπή</button>
    <button class="btn" onclick="genCmd('reset')">Reset σφάλματος</button>
    <button class="btn secondary" onclick="genCmd('estop_on')">E-Stop ON</button>
    <button class="btn secondary" onclick="genCmd('estop_off')">E-Stop OFF</button>
    <p class="small" id="genCmdStatus">-</p></div>
    <div class="card"><h2>Κατάσταση Γεννήτριας</h2><pre id="genData">Αναμονή δεδομένων Modbus...</pre></div>
  </section>
  <section id="tab-ble" class="tab">
    <div class="card"><h2>Bluetooth σκανάρισμα</h2>
    <p class="small">Εμφανίζονται μόνο πιθανές συσκευές BMS (TP_*, JBD, Daly, JK, ANT). Κλείσε την app της μπαταρίας στο κινητό πριν τη σύνδεση.</p>
    <button class="btn" onclick="bleScan()">Σκανάρισμα συσκευών</button>
    <p class="small" id="bleScanStatus">-</p>
    <div id="bleList"></div></div>
    <div class="card"><h2>Συνδεδεμένη συσκευή</h2><pre id="bleStatus">-</pre>
    <button class="btn danger" onclick="bleDisconnect()">Αποσύνδεση BLE</button></div>
  </section>
  <section id="tab-wifi" class="tab">
    <div class="card"><h2>WiFi ρύθμιση</h2>
    <p class="small" id="wifiInfo">-</p>
    <p class="small" id="wifiSaved">-</p>
    <button class="btn" onclick="wifiReset()">Προσθήκη / Αλλαγή WiFi (Setup Portal)</button>
    <p class="small">Αποθηκεύει έως 5 δίκτυα. Στην επόμενη εκκίνηση συνδέεται αυτόματα στο διαθέσιμο. Μετά το κουμπί: σύνδεση στο <b>ESP32-Setup</b>.</p></div>
    <div class="card"><h2>MQTT (Internet)</h2>
    <label>Broker</label><input id="mqttBroker" placeholder="broker.hivemq.com">
    <label>Port</label><input id="mqttPort" type="number" placeholder="1883">
    <button class="btn success" onclick="saveMqtt()">Αποθήκευση MQTT</button></div>
    <div class="card"><h2>Firmware Update (OTA)</h2>
    <p class="small">Αναβάθμιση μέσω WiFi — χωρίς USB. Πρέπει να είσαι στο ίδιο WiFi σπιτιού.</p>
    <label>Firmware (.bin)</label>
    <input type="file" id="otaFile" accept=".bin">
    <button class="btn" onclick="otaUpload()">Ανέβασμα firmware</button>
    <p class="small">Ή από PC: <code>upload-ota.ps1</code> (κωδικός: esp32ota)</p>
    <p class="small" id="otaStatus">-</p></div>
  </section>
  <section id="tab-remote" class="tab">
    <div class="card"><h2>Απομακρυσμένη πρόσβαση (αυτόματη)</h2>
    <p class="small">Όταν το ESP32 είναι στο WiFi σπιτιού, δημοσιεύει αυτόματα στο MQTT.</p>
    <p class="small" id="remoteStatus">Σύνδεση...</p>
    <pre id="mqttTopics">-</pre>
    <p class="small"><b>Για κινητό (4G):</b> άνοιξε <a href="/remote" style="color:#60a5fa">/remote</a> και «Προσθήκη στην αρχική» — δουλεύει αυτόματα και εκτός σπιτιού.</p>
    <label>Device ID</label><input id="remoteDeviceId" readonly>
    <label>MQTT Broker (WebSocket)</label><input id="wsBroker" value="wss://broker.hivemq.com:8884/mqtt" readonly>
    </div>
  </section>
</main>
<script src="https://unpkg.com/mqtt@5.3.5/dist/mqtt.min.js"></script>
<script>
const API='';
let mqttClient=null, remoteMode=false, mqttBridgeReady=false;

function saveRemoteProfile(s){
  if(!s||!s.device_id)return;
  localStorage.setItem('esp32_device_id',s.device_id);
  localStorage.setItem('esp32_ip',s.ip||'');
  localStorage.setItem('esp32_ws_broker',document.getElementById('wsBroker').value);
}

function updateConnBadge(s){
  const el=document.getElementById('connBadge');
  if(mqttBridgeReady&&s&&s.wifi_connected){el.textContent='Local + Remote';el.className='badge ok';}
  else if(s&&s.wifi_connected){el.textContent='Local';el.className='badge ok';}
  else{el.textContent='Offline';el.className='badge warn';}
}

function formatBms(b){
  if(!b||!b.valid)return'Δεν υπάρχουν δεδομένα BMS';
  let s='';
  if(b.type_label)s+=b.type_label+'\n';
  if(b.name)s+='Device: '+b.name+'\n';
  if(b.device_model)s+='Model: '+b.device_model+'\n';
  if(b.sw_version)s+='FW: '+b.sw_version+'\n';
  s+='---\n';
  s+=`SOC: ${b.soc}%  |  SOH: ${b.soh}%\n`;
  s+=`Τάση: ${Number(b.voltage).toFixed(2)} V  |  Ρεύμα: ${Number(b.current).toFixed(2)} A\n`;
  s+=`Ισχύς: ${Number(b.power).toFixed(0)} W`;
  if(b.charge_power>0)s+=`  (Φόρτιση: ${Number(b.charge_power).toFixed(0)} W)`;
  if(b.discharge_power>0)s+=`  (Εκφόρτιση: ${Number(b.discharge_power).toFixed(0)} W)`;
  s+='\n';
  let st='';
  if(b.charging)st+='ΦΟΡΤΙΣΗ ';
  if(b.discharging)st+='ΕΚΦΟΡΤΙΣΗ ';
  if(b.balancing)st+='BALANCING ';
  if(b.limiting_current)st+='LIMIT ';
  s+='Κατάσταση: '+(st||'ΑΝΕΝΕΡΓΟ')+'\n';
  s+=`Θερμ: avg ${Number(b.avg_temp).toFixed(1)}°C  |  ambient ${Number(b.ambient_temp).toFixed(1)}°C  |  MOSFET ${Number(b.mosfet_temp).toFixed(1)}°C\n`;
  if(b.capacity_ah>0)s+=`Χωρητικότητα: ${Number(b.remaining_ah).toFixed(1)} / ${Number(b.capacity_ah).toFixed(1)} Ah  |  Cycles: ${b.cycles}\n`;
  if(b.min_cell_v>0)s+=`Cells: min ${Number(b.min_cell_v).toFixed(3)}V (#${b.min_cell_num})  max ${Number(b.max_cell_v).toFixed(3)}V (#${b.max_cell_num})  delta ${Number(b.delta_cell_v).toFixed(3)}V\n`;
  if(b.cells&&b.cells.length){
    s+='--- Cell voltages ---\n';
    b.cells.forEach((v,i)=>{
      if(v>0.5){s+=`C${i+1}: ${Number(v).toFixed(3)}V`;if(b.cell_balancing&&b.cell_balancing[i])s+=' [BAL]';s+='  ';if((i+1)%4===0)s+='\n';}
    });
    s+='\n';
  }
  if(b.temps&&b.temps.some(t=>t!==0)){
    s+='--- Temperatures ---\n';
    b.temps.forEach((t,i)=>{if(t!==0)s+=`T${i+1}: ${Number(t).toFixed(1)}°C  `;});
    s+='\n';
  }
  if(b.error_mask||b.alarm_mask||b.voltage_prot_mask||b.current_prot_mask||b.temp_prot_mask){
    s+='--- Alarms ---\n';
    if(b.error_mask)s+=`Errors: 0x${b.error_mask.toString(16)}\n`;
    if(b.alarm_mask)s+=`Alarms: 0x${b.alarm_mask.toString(16)}\n`;
    if(b.voltage_prot_mask)s+=`V-prot: 0x${b.voltage_prot_mask.toString(16)}\n`;
    if(b.current_prot_mask)s+=`I-prot: 0x${b.current_prot_mask.toString(16)}\n`;
    if(b.temp_prot_mask)s+=`T-prot: 0x${b.temp_prot_mask.toString(16)}\n`;
  }
  return s;
}

function formatGenset(g){
  if(!g||!g.valid)return g&&g.error?('Modbus σφάλμα: '+g.error):'Δεν υπάρχουν δεδομένα γεννήτριας';
  let s='';
  s+=`Κατάσταση: ${g.genset_state_label||'-'}  |  Λειτουργία: ${g.op_mode_label||'-'}\n`;
  s+=`Τάση L-L: ${Number(g.volt_avg_ll).toFixed(0)} V  |  Hz: ${Number(g.frequency).toFixed(1)}\n`;
  s+=`L-N: ${Number(g.volt_l1n).toFixed(0)}/${Number(g.volt_l2n).toFixed(0)}/${Number(g.volt_l3n).toFixed(0)} V\n`;
  s+=`Ρεύμα: L1 ${Number(g.curr_l1).toFixed(1)}A  L2 ${Number(g.curr_l2).toFixed(1)}A  L3 ${Number(g.curr_l3).toFixed(1)}A\n`;
  s+=`Φόρτιση: ${Number(g.load_l1_pct).toFixed(1)}% / ${Number(g.load_l2_pct).toFixed(1)}% / ${Number(g.load_l3_pct).toFixed(1)}%\n`;
  s+=`kVA: ${Number(g.kva_total).toFixed(0)}  |  RPM: ${g.engine_rpm||0}\n`;
  s+=`Μπαταρία: ${Number(g.battery_v).toFixed(1)} V  |  Λάδι: ${Number(g.oil_kpa).toFixed(0)} kPa  |  Νερό: ${Number(g.coolant_c).toFixed(1)} °C\n`;
  s+=`Εκκινήσεις: ${g.total_runs||0}  |  Ώρες λειτ.: ${Math.floor((g.runtime_sec||0)/3600)} h\n`;
  if(g.active_fault)s+=`Σφάλμα: #${g.active_fault} (${g.fault_type_label||''})\n`;
  if(g.nfpa_bits&&g.nfpa_bits.length)s+=`NFPA: ${g.nfpa_bits.join(', ')}\n`;
  if(g.ext_bits&&g.ext_bits.length)s+=`Extended: ${g.ext_bits.join(', ')}\n`;
  if(g.aux_speed_bias!=null)s+=`AUX speed bias: ${Number(g.aux_speed_bias).toFixed(2)} RPM\n`;
  if(g.aux_volt_bias!=null)s+=`AUX volt bias: ${Number(g.aux_volt_bias).toFixed(2)} V\n`;
  if(g.baro_psi!=null)s+=`Barometric: ${Number(g.baro_psi).toFixed(1)} PSI\n`;
  if(g.lta_temp_f!=null)s+=`LTA temp: ${g.lta_temp_f} °F\n`;
  if(g.extras_valid)s+=`Fuel press valid: ${g.fuel_press_valid?'yes':'no'} | Input4: ${g.cfg_input4?'active':'inactive'}\n`;
  if(g.fault_bitmap_valid&&g.fault_bitmap){
    const active=[];
    g.fault_bitmap.forEach((w,i)=>{if(w)active.push(`4040${i}:0x${(w>>>0).toString(16).toUpperCase()}`);});
    if(active.length)s+=`Fault bitmaps: ${active.join(' ')}\n`;
  }
  return s;
}

async function genCmd(action){
  if(!confirm('Εκτέλεση: '+action+' ;'))return;
  try{
    await api('/api/genset/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({action})});
    document.getElementById('genCmdStatus').textContent='OK: '+action;
    refresh();
  }catch(e){
    document.getElementById('genCmdStatus').textContent='Σφάλμα: '+(e.message||e);
  }
}

document.querySelectorAll('nav button').forEach(b=>{
  b.onclick=()=>{
    document.querySelectorAll('nav button').forEach(x=>x.classList.remove('active'));
    document.querySelectorAll('.tab').forEach(x=>x.classList.remove('active'));
    b.classList.add('active');
    document.getElementById('tab-'+b.dataset.tab).classList.add('active');
  };
});

async function api(path,opt){
  const r=await fetch(API+path,opt);
  const j=await r.json().catch(()=>({}));
  if(!r.ok)throw Object.assign(new Error(j.error||('HTTP '+r.status)),{status:r.status,data:j});
  return j;
}

function renderRelays(outputs){
  const el=document.getElementById('relays');
  el.innerHTML=outputs.map((o,i)=>`
    <div class="toggle"><span>${o.name} (GPIO ${o.gpio})</span>
    <label class="switch"><input type="checkbox" ${o.on?'checked':''} onchange="setRelay(${i},this.checked)"><span class="slider"></span></label></div>`).join('');
}

async function refresh(){
  try{
    const s=await api('/api/status');
    updateConnBadge(s);
    saveRemoteProfile(s);
    ensureRemoteBridge(s.device_id);
    document.getElementById('deviceInfo').textContent=`${s.device_id} | ${s.ip} | RSSI ${s.rssi}`;
    renderRelays(s.outputs);
    if(s.ble&&s.ble.data&&s.ble.data.length>10){
      document.getElementById('bmsData').textContent=s.ble.data;
    }else if(s.bms&&s.bms.valid){
      document.getElementById('bmsData').textContent=formatBms(s.bms);
    }else{
      document.getElementById('bmsData').textContent='Δεν υπάρχουν δεδομένα BMS — σύνδεση Bluetooth (TP_*)';
    }
    document.getElementById('bleStatus').textContent=JSON.stringify(s.ble,null,2);
    document.getElementById('wifiInfo').textContent=`SSID: ${s.wifi_ssid||'-'} | IP: ${s.ip}`;
    const saved=(s.wifi_saved||[]);
    document.getElementById('wifiSaved').textContent=saved.length
      ? `Αποθηκευμένα δίκτυα (${saved.length}): ${saved.join(', ')}`
      : 'Αποθηκευμένα δίκτυα: κανένα ακόμα';
    document.getElementById('mqttBroker').value=s.mqtt.broker||'';
    document.getElementById('mqttPort').value=s.mqtt.port||1883;
    document.getElementById('remoteDeviceId').value=s.device_id;
    document.getElementById('mqttTopics').textContent=
      `Publish: ${s.mqtt.topic_status}\nSubscribe: ${s.mqtt.topic_cmd}\nBMS: ${s.mqtt.topic_bms}\nGenset: ${s.mqtt.topic_genset||'-'}`;
    if(s.genset){
      document.getElementById('modbusEn').checked=!!s.genset.enabled;
      document.getElementById('modbusId').value=s.genset.slave_id||1;
      document.getElementById('modbusBaud').value=String(s.genset.baud||9600);
      document.getElementById('genData').textContent=formatGenset(s.genset);
      document.getElementById('modbusStatus').textContent=s.genset.valid?'Modbus OK':(s.genset.error||'Αναμονή...');
    }
  }catch(e){document.getElementById('connBadge').textContent='Offline';}
}

async function setRelay(i,on){
  if(remoteMode&&mqttClient){mqttClient.publish(mqttCmdTopic,JSON.stringify({relay:i,on}));return;}
  await api('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({index:i,on})});
  refresh();
}
async function allOff(){
  if(remoteMode&&mqttClient){mqttClient.publish(mqttCmdTopic,JSON.stringify({relay:'all',on:false}));return;}
  await api('/api/relay/alloff',{method:'POST'});refresh();
}
let bleScanResults=[];
async function bleScan(){
  document.getElementById('bleScanStatus').textContent='Σκανάρισμα... (~8 sec)';
  document.getElementById('bleList').innerHTML='';
  try{
    const r=await api('/api/ble/scan');
    bleScanResults=r.devices||[];
    document.getElementById('bleScanStatus').textContent=bleScanResults.length
      ? `Βρέθηκαν ${bleScanResults.length} συσκευή(ές) BMS — πάτα για σύνδεση`
      : 'Δεν βρέθηκαν BMS. Βεβαιώσου ότι η μπαταρία είναι ON, κοντά στο ESP32, και κλειστή η app στο κινητό.';
    document.getElementById('bleList').innerHTML=bleScanResults.length?bleScanResults.map((d,i)=>
      `<div class="ble-item" data-idx="${i}"><b>${d.bms_label||'BMS'}: ${d.name||'Unknown'}</b><br><span class="small">${d.mac} • RSSI ${d.rssi}</span></div>`
    ).join(''):'';
  }catch(e){
    document.getElementById('bleScanStatus').textContent='Σφάλμα σάρωσης: '+e.message;
  }
}
document.getElementById('bleList').addEventListener('click',e=>{
  const item=e.target.closest('.ble-item');
  if(!item)return;
  const d=bleScanResults[+item.dataset.idx];
  if(d)bleConnect(d.mac,d.name||'',d.bms_type||'auto');
});
const BLE_ERR={
  unknown_bms_type:'Άγνωστος τύπος BMS — επίλεξε συσκευή TP_* (Basen Green)',
  connect_failed:'Αποτυχία σύνδεσης — κλείσε την app στο κινητό και δοκίμασε ξανά',
  invalid_mac:'Μη έγκυρη διεύθυνση MAC'
};
async function bleConnect(mac,name,type){
  const st=document.getElementById('bleScanStatus');
  st.textContent=`Σύνδεση σε ${name||mac}... (μπορεί να πάρει 20-30 δευτ.)`;
  try{
    const r=await api('/api/ble/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({mac,name,type:type||'auto'})});
    if(r.ok){
      st.textContent='Συνδέθηκε! Λήψη δεδομένων BMS...';
      setTimeout(refresh,1500);
      setTimeout(refresh,5000);
      return;
    }
    st.textContent='Αποτυχία σύνδεσης';
  }catch(e){
    st.textContent=BLE_ERR[e.data?.error]||('Σφάλμα: '+(e.data?.error||e.message));
  }
  refresh();
}
async function bleDisconnect(){
  await api('/api/ble/disconnect',{method:'POST'});refresh();
}
async function wifiReset(){
  if(!confirm('Θα γίνει επανεκκίνηση σε Setup mode. Σύνδεση στο ESP32-Setup.'))return;
  await api('/api/wifi/reset',{method:'POST'});
}
async function saveMqtt(){
  await api('/api/mqtt',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
    broker:document.getElementById('mqttBroker').value,
    port:parseInt(document.getElementById('mqttPort').value)||1883
  })});
  alert('Αποθηκεύτηκε');refresh();
}
async function saveModbus(){
  await api('/api/modbus',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({
    enabled:document.getElementById('modbusEn').checked,
    slave_id:parseInt(document.getElementById('modbusId').value)||1,
    baud:parseInt(document.getElementById('modbusBaud').value)||9600
  })});
  alert('Αποθηκεύτηκε');refresh();
}
async function otaUpload(){
  const f=document.getElementById('otaFile').files[0];
  if(!f){alert('Επίλεξε αρχείο .bin');return;}
  document.getElementById('otaStatus').textContent='Ανέβασμα... (~1-2 min)';
  try{
    const fd=new FormData();
    fd.append('firmware',f);
    const r=await fetch('/api/ota',{method:'POST',body:fd});
    const j=await r.json();
    document.getElementById('otaStatus').textContent=j.ok?'Επιτυχία — επανεκκίνηση...':'Αποτυχία';
    if(j.ok)setTimeout(()=>location.reload(),5000);
  }catch(e){document.getElementById('otaStatus').textContent='Σφάλμα: '+e;}
}
let mqttCmdTopic='';
function ensureRemoteBridge(id){
  if(!id||mqttBridgeReady)return;
  const url=document.getElementById('wsBroker').value;
  mqttCmdTopic=`home/${id}/cmd`;
  mqttClient=mqtt.connect(url,{clientId:'bridge-'+Math.random().toString(16).slice(2),reconnectPeriod:5000});
  mqttClient.on('connect',()=>{
    mqttBridgeReady=true;
    document.getElementById('remoteStatus').textContent='🟢 Remote ενεργό — διαθέσιμο από 4G αυτόματα';
    mqttClient.subscribe(`home/${id}/status`);
    mqttClient.subscribe(`home/${id}/bms`);
    updateConnBadge({wifi_connected:true});
  });
  mqttClient.on('close',()=>{
    mqttBridgeReady=false;
    document.getElementById('remoteStatus').textContent='🟡 Αναμονή MQTT...';
  });
}
refresh();setInterval(refresh,3000);
</script>
</body>
</html>
)rawliteral";
