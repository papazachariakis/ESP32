/* Clickable metric history + canvas charts for ESP32 dashboards */
(function (global) {
  const MAX_POINTS = 480;
  const MIN_INTERVAL_MS = 1500;
  const store = new Map();
  const meta = new Map();
  let lastRecord = new Map();
  let activeKey = null;

  const KPI_BINDINGS = [
    { id: 'soc', key: 'bms.soc', label: 'SOC', unit: '%', dec: 0 },
    { id: 'voltage', key: 'bms.voltage', label: 'Τάση μπαταρίας', unit: 'V', dec: 2 },
    { id: 'current', key: 'bms.current', label: 'Ρεύμα μπαταρίας', unit: 'A', dec: 2 },
    { id: 'power', key: 'bms.power', label: 'Ισχύς μπαταρίας', unit: 'W', dec: 0 },
    { id: 'soh', key: 'bms.soh', label: 'SOH', unit: '%', dec: 0 },
    { id: 'remAh', key: 'bms.remaining_ah', label: 'Διαθέσιμα Ah', unit: 'Ah', dec: 1 },
    { id: 'capAh', key: 'bms.capacity_ah', label: 'Χωρητικότητα', unit: 'Ah', dec: 1 },
    { id: 'dv', key: 'bms.delta_cell_v', label: 'Διαφορά κελιών', unit: 'V', dec: 3 },
    { id: 'ovV', key: 'bms.voltage', label: 'Τάση μπαταρίας', unit: 'V', dec: 2 },
    { id: 'ovA', key: 'bms.current', label: 'Ρεύμα μπαταρίας', unit: 'A', dec: 2 },
    { id: 'ovW', key: 'bms.power', label: 'Ισχύς μπαταρίας', unit: 'W', dec: 0 },
    { id: 'ovTemp', key: 'bms.avg_temp', label: 'Θερμοκρασία πακέτου', unit: '°C', dec: 1 },
    { id: 'ovAh', key: 'bms.remaining_ah', label: 'Διαθέσιμα Ah', unit: 'Ah', dec: 1 },
    { id: 'ovHz', key: 'gen.frequency', label: 'Συχνότητα γεννήτριας', unit: 'Hz', dec: 1 },
    { id: 'ovKw', key: 'gen.kw_total', label: 'kW γεννήτριας', unit: 'kW', dec: 1 },
    { id: 'ovRpm', key: 'gen.engine_rpm', label: 'RPM κινητήρα', unit: 'rpm', dec: 0 },
    { id: 'gVll', key: 'gen.volt_avg_ll', label: 'Τάση L-L', unit: 'V', dec: 0 },
    { id: 'gHz', key: 'gen.frequency', label: 'Συχνότητα', unit: 'Hz', dec: 1 },
    { id: 'gRpm', key: 'gen.engine_rpm', label: 'RPM', unit: 'rpm', dec: 0 },
    { id: 'gKva', key: 'gen.kva_total', label: 'kVA', unit: 'kVA', dec: 1 },
    { id: 'gCurr', key: 'gen.curr_avg', label: 'Ρεύμα μέσο', unit: 'A', dec: 1 },
    { id: 'gBatt', key: 'gen.battery_v', label: 'Μπαταρία γεννήτριας', unit: 'V', dec: 1 },
    { id: 'gOil', key: 'gen.oil_kpa', label: 'Πίεση λαδιού', unit: 'kPa', dec: 0 },
    { id: 'gCool', key: 'gen.coolant_c', label: 'Ψυκτικό', unit: '°C', dec: 1 },
    { id: 'gRuns', key: 'gen.total_runs', label: 'Εκκινήσεις', unit: '', dec: 0 },
    { id: 'gHours', key: 'gen.runtime_hours', label: 'Ώρες λειτουργίας', unit: 'h', dec: 1 }
  ];

  function ensureMeta(key, label, unit, dec) {
    if (!meta.has(key)) meta.set(key, { label: label || key, unit: unit || '', dec: dec ?? 1 });
  }

  function record(key, value, label, unit, dec) {
    const n = Number(value);
    if (!Number.isFinite(n)) return;
    ensureMeta(key, label, unit, dec);
    const now = Date.now();
    const last = lastRecord.get(key) || 0;
    if (now - last < MIN_INTERVAL_MS) return;
    lastRecord.set(key, now);
    if (!store.has(key)) store.set(key, []);
    const arr = store.get(key);
    arr.push({ t: now, v: n });
    if (arr.length > MAX_POINTS) arr.shift();
    if (activeKey === key) redraw();
  }

  function bindClickable(el, key, label, unit, dec) {
    if (!el || !key) return;
    ensureMeta(key, label, unit, dec);
    el.classList.add('chartable');
    el.dataset.chartKey = key;
    if (label) el.dataset.chartLabel = label;
    if (unit) el.dataset.chartUnit = unit;
    if (dec != null) el.dataset.chartDec = String(dec);
    el.title = 'Κλικ για διάγραμμα';
  }

  function bindById(elId, key, label, unit, dec) {
    const el = document.getElementById(elId);
    if (!el) return;
    const host = el.closest('.kpi') || el.closest('.hero-card') || el.closest('.soc-ring') || el;
    bindClickable(host, key, label, unit, dec);
  }

  function fmt(v, dec) {
    if (!Number.isFinite(v)) return '—';
    return Number(v).toFixed(dec ?? 1);
  }

  function timeLabel(ts) {
    const d = new Date(ts);
    return d.toLocaleTimeString('el-GR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }

  function drawChart(canvas, points, m) {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w = canvas.clientWidth || 800;
    const h = canvas.clientHeight || 300;
    if (canvas.width !== w) canvas.width = w;
    if (canvas.height !== h) canvas.height = h;

    ctx.clearRect(0, 0, w, h);
    ctx.fillStyle = '#0f172a';
    ctx.fillRect(0, 0, w, h);

    if (!points || points.length < 2) {
      ctx.fillStyle = '#94a3b8';
      ctx.font = '14px Inter, system-ui, sans-serif';
      ctx.textAlign = 'center';
      ctx.fillText('Συλλογή δεδομένων... (περίμενε ~30s)', w / 2, h / 2);
      return;
    }

    const pad = { l: 48, r: 16, t: 16, b: 32 };
    const plotW = w - pad.l - pad.r;
    const plotH = h - pad.t - pad.b;
    const vals = points.map(p => p.v);
    let minV = Math.min(...vals);
    let maxV = Math.max(...vals);
    if (minV === maxV) { minV -= 1; maxV += 1; }
    const padV = (maxV - minV) * 0.08 || 0.5;
    minV -= padV;
    maxV += padV;
    const t0 = points[0].t;
    const t1 = points[points.length - 1].t;
    const tSpan = Math.max(t1 - t0, 1);

    const xAt = t => pad.l + ((t - t0) / tSpan) * plotW;
    const yAt = v => pad.t + plotH - ((v - minV) / (maxV - minV)) * plotH;

    ctx.strokeStyle = '#243044';
    ctx.lineWidth = 1;
    for (let i = 0; i <= 4; i++) {
      const y = pad.t + (plotH * i) / 4;
      ctx.beginPath();
      ctx.moveTo(pad.l, y);
      ctx.lineTo(w - pad.r, y);
      ctx.stroke();
      const v = maxV - ((maxV - minV) * i) / 4;
      ctx.fillStyle = '#64748b';
      ctx.font = '11px Inter, system-ui, sans-serif';
      ctx.textAlign = 'right';
      ctx.fillText(fmt(v, m.dec), pad.l - 6, y + 4);
    }

    ctx.strokeStyle = '#3b82f6';
    ctx.lineWidth = 2;
    ctx.beginPath();
    points.forEach((p, i) => {
      const x = xAt(p.t);
      const y = yAt(p.v);
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();

    const last = points[points.length - 1];
    const lx = xAt(last.t);
    const ly = yAt(last.v);
    ctx.fillStyle = '#22c55e';
    ctx.beginPath();
    ctx.arc(lx, ly, 4, 0, Math.PI * 2);
    ctx.fill();

    ctx.fillStyle = '#64748b';
    ctx.font = '11px Inter, system-ui, sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(timeLabel(t0), pad.l, h - 10);
    ctx.fillText(timeLabel(t1), w - pad.r, h - 10);
  }

  function redraw() {
    const canvas = document.getElementById('chartCanvas');
    const title = document.getElementById('chartTitle');
    const stats = document.getElementById('chartStats');
    const foot = document.getElementById('chartFoot');
    if (!activeKey || !canvas) return;
    const m = meta.get(activeKey) || { label: activeKey, unit: '', dec: 1 };
    const points = store.get(activeKey) || [];
    if (title) title.textContent = m.label + (m.unit ? ` (${m.unit})` : '');
    if (stats) {
      if (points.length) {
        const vals = points.map(p => p.v);
        const cur = vals[vals.length - 1];
        const min = Math.min(...vals);
        const max = Math.max(...vals);
        const mins = Math.round((points[points.length - 1].t - points[0].t) / 60000);
        stats.innerHTML = `
          <span>Τώρα: <b>${fmt(cur, m.dec)}</b></span>
          <span>Min: <b>${fmt(min, m.dec)}</b></span>
          <span>Max: <b>${fmt(max, m.dec)}</b></span>
          <span>${points.length} σημεία · ~${mins || '<1'} λεπτά</span>`;
      } else {
        stats.innerHTML = '<span class="muted">Δεν υπάρχουν ακόμα δεδομένα</span>';
      }
    }
    if (foot) foot.textContent = `Ιστορικό session (έως ${MAX_POINTS} σημεία) · ενημέρωση κάθε ~${MIN_INTERVAL_MS / 1000}s`;
    drawChart(canvas, points, m);
  }

  function openChart(key, label, unit, dec) {
    ensureMeta(key, label, unit, dec);
    activeKey = key;
    const overlay = document.getElementById('chartOverlay');
    if (overlay) {
      overlay.classList.add('open');
      overlay.setAttribute('aria-hidden', 'false');
    }
    redraw();
  }

  function closeChart() {
    activeKey = null;
    const overlay = document.getElementById('chartOverlay');
    if (overlay) {
      overlay.classList.remove('open');
      overlay.setAttribute('aria-hidden', 'true');
    }
  }

  function recordBms(b) {
    if (!b || !b.valid) return;
    record('bms.soc', b.soc, 'SOC', '%', 0);
    record('bms.voltage', b.voltage, 'Τάση', 'V', 2);
    record('bms.current', b.current, 'Ρεύμα', 'A', 2);
    record('bms.power', b.power, 'Ισχύς', 'W', 0);
    record('bms.soh', b.soh, 'SOH', '%', 0);
    record('bms.remaining_ah', b.remaining_ah, 'Διαθέσιμα Ah', 'Ah', 1);
    record('bms.capacity_ah', b.capacity_ah, 'Χωρητικότητα', 'Ah', 1);
    record('bms.delta_cell_v', b.delta_cell_v, 'Διαφορά κελιών', 'V', 3);
    record('bms.avg_temp', b.avg_temp, 'Θερμοκρασία πακέτου', '°C', 1);
    record('bms.ambient_temp', b.ambient_temp, 'Περιβάλλον', '°C', 1);
    record('bms.mosfet_temp', b.mosfet_temp, 'MOSFET', '°C', 1);
    if (Array.isArray(b.cells)) {
      b.cells.forEach((v, i) => {
        if (Number(v) > 0.5) record(`bms.cell.${i}`, v, `Κελί C${i + 1}`, 'V', 3);
      });
    }
    if (Array.isArray(b.temps)) {
      b.temps.forEach((t, i) => {
        if (Number(t) !== 0) record(`bms.temp.${i}`, t, `Θερμοκρασία T${i + 1}`, '°C', 1);
      });
    }
  }

  function recordGenset(g) {
    if (!g || !g.valid) return;
    record('gen.frequency', g.frequency, 'Συχνότητα', 'Hz', 1);
    record('gen.kw_total', g.kw_total, 'kW', 'kW', 1);
    record('gen.kva_total', g.kva_total, 'kVA', 'kVA', 1);
    record('gen.engine_rpm', g.engine_rpm, 'RPM', 'rpm', 0);
    record('gen.volt_avg_ll', g.volt_avg_ll, 'Τάση L-L', 'V', 0);
    record('gen.curr_avg', g.curr_avg, 'Ρεύμα μέσο', 'A', 1);
    record('gen.battery_v', g.battery_v, 'Μπαταρία', 'V', 1);
    record('gen.oil_kpa', g.oil_kpa, 'Λάδι', 'kPa', 0);
    record('gen.coolant_c', g.coolant_c, 'Ψυκτικό', '°C', 1);
    record('gen.load_l1_pct', g.load_l1_pct, 'Φόρτιση L1', '%', 1);
    record('gen.load_l2_pct', g.load_l2_pct, 'Φόρτιση L2', '%', 1);
    record('gen.load_l3_pct', g.load_l3_pct, 'Φόρτιση L3', '%', 1);
    record('gen.total_runs', g.total_runs, 'Εκκινήσεις', '', 0);
    record('gen.runtime_hours', g.runtime_hours, 'Ώρες λειτ.', 'h', 1);
    [['L1', g.volt_l1n, g.curr_l1, g.kw_l1], ['L2', g.volt_l2n, g.curr_l2, g.kw_l2], ['L3', g.volt_l3n, g.curr_l3, g.kw_l3]]
      .forEach(([ph, v, a, kw]) => {
        record(`gen.${ph.toLowerCase()}_v`, v, `${ph} τάση`, 'V', 0);
        record(`gen.${ph.toLowerCase()}_a`, a, `${ph} ρεύμα`, 'A', 1);
        record(`gen.${ph.toLowerCase()}_kw`, kw, `${ph} kW`, 'kW', 1);
      });
  }

  function init() {
    KPI_BINDINGS.forEach(b => bindById(b.id, b.key, b.label, b.unit, b.dec));

    const socRing = document.querySelector('.soc-ring');
    if (socRing) bindClickable(socRing, 'bms.soc', 'SOC', '%', 0);

    document.querySelectorAll('.load-row').forEach((row, i) => {
      const keys = ['gen.load_l1_pct', 'gen.load_l2_pct', 'gen.load_l3_pct'];
      const labels = ['Φόρτιση L1', 'Φόρτιση L2', 'Φόρτιση L3'];
      bindClickable(row, keys[i], labels[i], '%', 1);
    });

    document.addEventListener('click', e => {
      const host = e.target.closest('.chartable');
      if (!host || !host.dataset.chartKey) return;
      e.preventDefault();
      openChart(
        host.dataset.chartKey,
        host.dataset.chartLabel,
        host.dataset.chartUnit,
        host.dataset.chartDec != null ? Number(host.dataset.chartDec) : undefined
      );
    });

    const overlay = document.getElementById('chartOverlay');
    const closeBtn = document.getElementById('chartClose');
    if (closeBtn) closeBtn.addEventListener('click', closeChart);
    if (overlay) overlay.addEventListener('click', e => { if (e.target === overlay) closeChart(); });
    document.addEventListener('keydown', e => { if (e.key === 'Escape') closeChart(); });
    window.addEventListener('resize', () => { if (activeKey) redraw(); });
  }

  global.ChartHub = { init, record, recordBms, recordGenset, openChart, closeChart, bindClickable };
})(typeof window !== 'undefined' ? window : globalThis);
