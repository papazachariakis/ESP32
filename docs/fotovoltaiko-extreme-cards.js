/**
 * PVX — Fotovoltaiko Extreme custom Lovelace cards
 * Original cockpit cards (not mushroom/sunsynk wrappers).
 */
(() => {
  const FONT =
    "https://fonts.googleapis.com/css2?family=Orbitron:wght@500;700;900&family=JetBrains+Mono:wght@400;600;700&display=swap";

  const CSS = `
  :host{display:block}
  .pvx{
    --bg:#05080f;--panel:#0c1220;--panel2:#111a2e;--line:#1e2d4a;
    --text:#e8f0ff;--muted:#6b7f9e;--dim:#3d4f6a;
    --pv:#ffb000;--load:#39ff14;--grid:#00e5ff;--bat:#a855f7;
    --ok:#22c55e;--warn:#f59e0b;--bad:#ef4444;--charge:#38bdf8;--discharge:#fb923c;
    --mono:'JetBrains Mono',ui-monospace,monospace;
    --display:'Orbitron',system-ui,sans-serif;
    font-family:var(--mono);color:var(--text);
    background:
      radial-gradient(ellipse 90% 60% at 50% -20%,rgba(255,176,0,.12),transparent 55%),
      radial-gradient(ellipse 50% 40% at 100% 100%,rgba(168,85,247,.08),transparent),
      linear-gradient(160deg,var(--panel2),var(--panel));
    border:1px solid var(--line);border-radius:14px;overflow:hidden;position:relative;
  }
  .pvx::before{
    content:'';position:absolute;inset:0 0 auto 0;height:2px;
    background:linear-gradient(90deg,transparent,var(--accent,#ffb000),transparent);
  }
  .pvx .scan{
    pointer-events:none;position:absolute;inset:0;opacity:.04;z-index:0;
    background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,.5) 2px,rgba(0,0,0,.5) 4px);
  }
  .pvx .inner{position:relative;z-index:1;padding:14px 16px}
  .pvx .label{
    font-family:var(--display);font-size:.68rem;letter-spacing:.18em;text-transform:uppercase;
    color:var(--accent,#ffb000);font-weight:700;margin-bottom:8px;
  }
  .pvx .sub{color:var(--muted);font-size:.72rem;margin-top:-4px;margin-bottom:12px}
  .hero-title{
    font-family:var(--display);font-weight:900;font-size:clamp(1.6rem,4.5vw,2.6rem);
    letter-spacing:.08em;line-height:1.05;
    background:linear-gradient(90deg,#fff7c2,#ffb000,#39ff14,#00e5ff);
    -webkit-background-clip:text;background-clip:text;color:transparent;
  }
  .hero-tag{
    margin-top:6px;font-family:var(--display);font-weight:700;letter-spacing:.28em;
    color:var(--load);font-size:.95rem;text-shadow:0 0 16px rgba(57,255,20,.55);
  }
  .hero-meta{margin-top:10px;color:var(--muted);font-size:.75rem}
  .kpi-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:10px}
  .kpi{
    background:rgba(0,0,0,.28);border:1px solid var(--line);border-radius:10px;padding:12px 10px;text-align:center;
  }
  .kpi .k{font-size:.62rem;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);margin-bottom:6px}
  .kpi .v{font-family:var(--display);font-weight:900;font-size:1.35rem;line-height:1.1}
  .kpi .u{font-size:.65rem;color:var(--muted);margin-top:2px}
  .kpi.pv .v{color:var(--pv)} .kpi.load .v{color:var(--load)}
  .kpi.grid .v{color:var(--grid)} .kpi.bat .v{color:var(--bat)}
  .flow{
    display:grid;grid-template-columns:1fr auto 1fr;grid-template-rows:auto auto auto;
    gap:8px 12px;align-items:center;justify-items:center;min-height:220px;padding:8px 0;
  }
  .node{
    min-width:110px;padding:12px 14px;border-radius:12px;border:1px solid var(--line);
    background:rgba(0,0,0,.35);text-align:center;
  }
  .node .n{font-size:.62rem;letter-spacing:.14em;text-transform:uppercase;color:var(--muted)}
  .node .w{font-family:var(--display);font-weight:900;font-size:1.25rem;margin-top:4px}
  .node.pv{border-color:rgba(255,176,0,.45);box-shadow:0 0 18px rgba(255,176,0,.2)} .node.pv .w{color:var(--pv)}
  .node.load{border-color:rgba(57,255,20,.45);box-shadow:0 0 18px rgba(57,255,20,.2)} .node.load .w{color:var(--load)}
  .node.grid{border-color:rgba(0,229,255,.45);box-shadow:0 0 18px rgba(0,229,255,.2)} .node.grid .w{color:var(--grid)}
  .node.bat{border-color:rgba(168,85,247,.5);box-shadow:0 0 18px rgba(168,85,247,.22)} .node.bat .w{color:var(--bat)}
  .node.inv{
    grid-column:2;grid-row:2;border-color:rgba(255,255,255,.25);
    background:linear-gradient(145deg,rgba(20,30,50,.9),rgba(8,12,22,.95));
  }
  .node.inv .w{color:var(--text);font-size:1rem}
  .arrow{color:var(--dim);font-size:.85rem;letter-spacing:.05em}
  .arrow.on{color:var(--ok);text-shadow:0 0 8px rgba(34,197,94,.5)}
  .soc-wrap{display:flex;gap:16px;align-items:center;flex-wrap:wrap}
  .ring{
    --p:0;width:140px;height:140px;border-radius:50%;position:relative;
    background:conic-gradient(var(--ring,#a855f7) calc(var(--p)*1%),rgba(255,255,255,.08) 0);
    display:grid;place-items:center;
  }
  .ring::after{
    content:'';position:absolute;inset:12px;border-radius:50%;
    background:radial-gradient(circle at 40% 30%,#162035,#0a101c 70%);
    border:1px solid var(--line);
  }
  .ring .mid{position:relative;z-index:1;text-align:center}
  .ring .mid .pct{font-family:var(--display);font-weight:900;font-size:2rem}
  .ring .mid .cap{font-size:.65rem;color:var(--muted);letter-spacing:.08em}
  .bat-stats{flex:1;min-width:180px;display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .stat{
    background:rgba(0,0,0,.25);border:1px solid var(--line);border-radius:8px;padding:8px 10px;
  }
  .stat .k{font-size:.58rem;letter-spacing:.1em;text-transform:uppercase;color:var(--muted)}
  .stat .v{font-family:var(--display);font-weight:700;font-size:1rem;margin-top:2px}
  .pills{display:flex;flex-wrap:wrap;gap:6px;margin-top:12px}
  .pill{
    padding:4px 10px;border-radius:999px;font-size:.62rem;letter-spacing:.08em;text-transform:uppercase;
    border:1px solid var(--line);color:var(--muted);
  }
  .pill.on{color:var(--ok);border-color:rgba(34,197,94,.5);box-shadow:0 0 10px rgba(34,197,94,.25)}
  .pill.chg{color:var(--charge);border-color:rgba(56,189,248,.5)}
  .pill.dsg{color:var(--discharge);border-color:rgba(251,146,60,.5)}
  .pill.bad{color:var(--bad);border-color:rgba(239,68,68,.55);animation:blink .8s ease infinite}
  @keyframes blink{50%{opacity:.55}}
  .cells{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:6px}
  @media(min-width:700px){.cells{grid-template-columns:repeat(8,minmax(0,1fr))}}
  .cell{
    background:rgba(0,0,0,.3);border:1px solid var(--line);border-radius:8px;padding:8px 4px;text-align:center;
  }
  .cell .i{font-size:.55rem;color:var(--muted)}
  .cell .cv{font-family:var(--display);font-weight:700;font-size:.85rem;margin-top:2px}
  .cell.hi{border-color:rgba(239,68,68,.55)} .cell.hi .cv{color:var(--bad)}
  .cell.lo{border-color:rgba(56,189,248,.45)} .cell.lo .cv{color:var(--charge)}
  .cell.ok .cv{color:var(--ok)}
  .btn-row{display:flex;flex-wrap:wrap;gap:8px;margin-top:4px}
  .btn{
    flex:1;min-width:120px;padding:12px 14px;border-radius:10px;border:1px solid var(--line);
    background:rgba(0,0,0,.35);color:var(--text);font-family:var(--display);font-weight:700;
    letter-spacing:.1em;font-size:.75rem;cursor:pointer;text-transform:uppercase;
  }
  .btn:hover{border-color:var(--pv);color:var(--pv)}
  .btn.on{border-color:rgba(34,197,94,.55);color:var(--ok);box-shadow:0 0 14px rgba(34,197,94,.25)}
  .btn.off{border-color:rgba(239,68,68,.45);color:var(--bad)}
  .phases{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:10px}
  .err{padding:16px;color:var(--bad);font-size:.8rem}
  `;

  function ensureFont() {
    if (document.getElementById("pvx-font")) return;
    const l = document.createElement("link");
    l.id = "pvx-font";
    l.rel = "stylesheet";
    l.href = FONT;
    document.head.appendChild(l);
  }

  function num(hass, entity, digits = 0) {
    if (!hass || !entity) return null;
    const st = hass.states[entity];
    if (!st) return null;
    const v = parseFloat(st.state);
    return Number.isFinite(v) ? Number(v.toFixed(digits)) : null;
  }

  function state(hass, entity) {
    return hass?.states?.[entity]?.state ?? "unavailable";
  }

  function fmt(v, unit = "", digits = 0) {
    if (v === null || v === undefined) return "—";
    if (Math.abs(v) >= 1000 && (unit === "W" || unit === "VA")) {
      return `${(v / 1000).toFixed(2)} k${unit === "W" ? "W" : unit}`;
    }
    return `${Number(v).toFixed(digits)}${unit ? " " + unit : ""}`;
  }

  function fmtW(v) {
    if (v === null || v === undefined) return "—";
    if (Math.abs(v) >= 1000) return `${(v / 1000).toFixed(2)} kW`;
    return `${Math.round(v)} W`;
  }

  function shell(accent, bodyHtml) {
    return `
      <div class="pvx" style="--accent:${accent}">
        <div class="scan"></div>
        <div class="inner">${bodyHtml}</div>
      </div>`;
  }

  class PvxBase extends HTMLElement {
    _config = {};
    _hass = null;

    setConfig(config) {
      this._config = config || {};
      this._render();
    }

    set hass(hass) {
      this._hass = hass;
      this._render();
    }

    connectedCallback() {
      ensureFont();
      if (!this.shadowRoot) {
        this.attachShadow({ mode: "open" });
      }
      this._render();
    }

    _root() {
      if (!this.shadowRoot) this.attachShadow({ mode: "open" });
      return this.shadowRoot;
    }

    _paint(html) {
      const root = this._root();
      root.innerHTML = `<style>${CSS}</style>${html}`;
    }
  }

  class PvxHeroCard extends PvxBase {
    getCardSize() {
      return 2;
    }
    _render() {
      const c = this._config;
      const title = c.title || "ΦΩΤΟΒΟΛΤΑΪΚΟ";
      const tag = c.tag || "PVX · EXTREME";
      const meta = c.meta || "Deye · Basen 30kWh · Grid · Breaker";
      this._paint(
        shell(
          "#ffb000",
          `<div class="label">SOLAR COMMAND · LIVE</div>
           <div class="hero-title">${title}</div>
           <div class="hero-tag">${tag}</div>
           <div class="hero-meta">${meta}</div>`
        )
      );
    }
  }

  class PvxKpiCard extends PvxBase {
    getCardSize() {
      return 2;
    }
    _render() {
      const h = this._hass;
      const c = this._config;
      const pv = num(h, c.pv_entity || "sensor.inverter_pv_power", 0);
      const load = num(h, c.load_entity || "sensor.inverter_load_power", 0);
      const grid = num(h, c.grid_entity || "sensor.inverter_grid_power", 0);
      const bat = num(h, c.battery_entity || "sensor.inverter_battery_power", 0);
      this._paint(
        shell(
          "#00e5ff",
          `<div class="label">${c.title || "LIVE POWER"}</div>
           <div class="sub">${c.subtitle || "PV · Load · Grid · Battery"}</div>
           <div class="kpi-grid">
             <div class="kpi pv"><div class="k">PV</div><div class="v">${fmtW(pv)}</div></div>
             <div class="kpi load"><div class="k">Load</div><div class="v">${fmtW(load)}</div></div>
             <div class="kpi grid"><div class="k">Grid</div><div class="v">${fmtW(grid)}</div></div>
             <div class="kpi bat"><div class="k">Battery</div><div class="v">${fmtW(bat)}</div></div>
           </div>`
        )
      );
    }
  }

  class PvxFlowCard extends PvxBase {
    getCardSize() {
      return 4;
    }
    _render() {
      const h = this._hass;
      const c = this._config;
      const pv = num(h, c.pv_entity || "sensor.inverter_pv_power", 0) || 0;
      const load = num(h, c.load_entity || "sensor.inverter_load_power", 0) || 0;
      const grid = num(h, c.grid_entity || "sensor.inverter_grid_power", 0) || 0;
      const bat = num(h, c.battery_entity || "sensor.inverter_battery_power", 0) || 0;
      const soc = num(h, c.soc_entity || "sensor.inverter_battery", 0);
      const on = (v) => (Math.abs(v) > 20 ? "on" : "");
      this._paint(
        shell(
          "#39ff14",
          `<div class="label">${c.title || "POWER FLOW"}</div>
           <div class="sub">Custom PVX topology · not sunsynk</div>
           <div class="flow">
             <div></div>
             <div class="node pv"><div class="n">Solar</div><div class="w">${fmtW(pv)}</div></div>
             <div></div>
             <div class="node bat"><div class="n">Battery</div><div class="w">${fmtW(bat)}</div>
               <div class="u" style="margin-top:4px;font-size:.65rem;color:var(--muted)">SOC ${soc ?? "—"}%</div></div>
             <div class="node inv"><div class="n">Inverter</div><div class="w">DEYE</div>
               <div class="arrow ${on(pv)}">PV ↕</div>
               <div class="arrow ${on(bat)}">BAT ↕</div>
               <div class="arrow ${on(grid)}">GRID ↕</div>
               <div class="arrow ${on(load)}">LOAD →</div>
             </div>
             <div class="node load"><div class="n">Load</div><div class="w">${fmtW(load)}</div></div>
             <div></div>
             <div class="node grid"><div class="n">Grid</div><div class="w">${fmtW(grid)}</div></div>
             <div></div>
           </div>`
        )
      );
    }
  }

  class PvxBatteryCard extends PvxBase {
    getCardSize() {
      return 4;
    }
    _render() {
      const h = this._hass;
      const c = this._config;
      const p = c.prefix || "sensor.tp_bstbd_25c_2_";
      const soc = num(h, c.soc_entity || p + "state_of_charge", 0) || 0;
      const v = num(h, c.voltage_entity || p + "total_voltage", 1);
      const i = num(h, c.current_entity || p + "current", 1);
      const w = num(h, c.power_entity || p + "power", 0);
      const t = num(h, c.temp_entity || p + "temperature", 1);
      const soh = num(h, c.soh_entity || p + "state_of_health", 0);
      const rem = num(h, c.remaining_entity || p + "capacity_remaining", 0);
      const cycles = num(h, c.cycles_entity || p + "charge_cycles", 0);
      const delta = num(h, c.delta_entity || p + "delta_cell_voltage", 3);
      const chg = state(h, c.charging_entity || "binary_sensor.tp_bstbd_25c_2_charging") === "on";
      const dsg = state(h, c.discharging_entity || "binary_sensor.tp_bstbd_25c_2_discharging") === "on";
      const bal = state(h, c.balancing_entity || "binary_sensor.tp_bstbd_25c_2_balancing") === "on";
      const prob = state(h, c.problem_entity || "binary_sensor.tp_bstbd_25c_2_problem") === "on";
      let ring = "#a855f7";
      if (soc > 70) ring = "#22c55e";
      else if (soc > 35) ring = "#f59e0b";
      else ring = "#ef4444";
      this._paint(
        shell(
          "#a855f7",
          `<div class="label">${c.title || "BASEN 30kWh"}</div>
           <div class="sub">ESP32 Classic · BLE · MQTT Discovery</div>
           <div class="soc-wrap">
             <div class="ring" style="--p:${Math.max(0, Math.min(100, soc))};--ring:${ring}">
               <div class="mid"><div class="pct" style="color:${ring}">${soc}%</div><div class="cap">SOC</div></div>
             </div>
             <div class="bat-stats">
               <div class="stat"><div class="k">Voltage</div><div class="v">${fmt(v, "V", 1)}</div></div>
               <div class="stat"><div class="k">Current</div><div class="v">${fmt(i, "A", 1)}</div></div>
               <div class="stat"><div class="k">Power</div><div class="v">${fmtW(w)}</div></div>
               <div class="stat"><div class="k">Temp</div><div class="v">${fmt(t, "°C", 1)}</div></div>
               <div class="stat"><div class="k">SOH</div><div class="v">${fmt(soh, "%", 0)}</div></div>
               <div class="stat"><div class="k">Remaining</div><div class="v">${fmt(rem, "Ah", 0)}</div></div>
               <div class="stat"><div class="k">Cycles</div><div class="v">${fmt(cycles, "", 0)}</div></div>
               <div class="stat"><div class="k">Δ Cell</div><div class="v">${fmt(delta, "V", 3)}</div></div>
             </div>
           </div>
           <div class="pills">
             <span class="pill ${chg ? "chg on" : ""}">Charging</span>
             <span class="pill ${dsg ? "dsg on" : ""}">Discharging</span>
             <span class="pill ${bal ? "on" : ""}">Balancing</span>
             <span class="pill ${prob ? "bad" : "on"}">${prob ? "Problem" : "Healthy"}</span>
           </div>`
        )
      );
    }
  }

  class PvxCellsCard extends PvxBase {
    getCardSize() {
      return 3;
    }
    _render() {
      const h = this._hass;
      const c = this._config;
      const count = c.count || 16;
      const base = c.base_entity || "sensor.tp_bstbd_25c_2_cell_voltage";
      const vals = [];
      for (let i = 1; i <= count; i++) {
        const eid = i === 1 ? base : `${base}_${i}`;
        vals.push({ i, v: num(h, eid, 3) });
      }
      const known = vals.map((x) => x.v).filter((v) => v !== null);
      const hi = known.length ? Math.max(...known) : null;
      const lo = known.length ? Math.min(...known) : null;
      const cells = vals
        .map(({ i, v }) => {
          let cls = "ok";
          if (v !== null && hi !== null && v === hi && hi !== lo) cls = "hi";
          if (v !== null && lo !== null && v === lo && hi !== lo) cls = "lo";
          return `<div class="cell ${cls}"><div class="i">C${i}</div><div class="cv">${v === null ? "—" : v.toFixed(3)}</div></div>`;
        })
        .join("");
      this._paint(
        shell(
          "#38bdf8",
          `<div class="label">${c.title || "CELL MATRIX"}</div>
           <div class="sub">Hi ${hi ?? "—"} V · Lo ${lo ?? "—"} V · Δ ${
             hi !== null && lo !== null ? (hi - lo).toFixed(3) : "—"
           } V</div>
           <div class="cells">${cells}</div>`
        )
      );
    }
  }

  class PvxBreakerCard extends PvxBase {
    getCardSize() {
      return 3;
    }
    _render() {
      const h = this._hass;
      const c = this._config;
      const sw = c.switch_entity || "switch.breaker_switch";
      const on = state(h, sw) === "on";
      const pa = num(h, c.pa || "sensor.breaker_phase_a_power_2", 0);
      const pb = num(h, c.pb || "sensor.breaker_phase_b_power_2", 0);
      const pc = num(h, c.pc || "sensor.breaker_phase_c_power_2", 0);
      const temp = num(h, c.temp || "sensor.breaker_temperature", 1);
      this._paint(
        shell(
          "#ff3d00",
          `<div class="label">${c.title || "BREAKER"}</div>
           <div class="sub">3-phase · temperature ${fmt(temp, "°C", 1)}</div>
           <div class="btn-row">
             <button class="btn ${on ? "on" : "off"}" data-act="toggle">${on ? "ON · TAP TO TOGGLE" : "OFF · TAP TO TOGGLE"}</button>
           </div>
           <div class="phases">
             <div class="stat"><div class="k">Phase A</div><div class="v">${fmtW(pa)}</div></div>
             <div class="stat"><div class="k">Phase B</div><div class="v">${fmtW(pb)}</div></div>
             <div class="stat"><div class="k">Phase C</div><div class="v">${fmtW(pc)}</div></div>
           </div>`
        )
      );
      const btn = this.shadowRoot.querySelector("button[data-act=toggle]");
      if (btn) {
        btn.onclick = () => {
          if (!this._hass) return;
          this._hass.callService("switch", "toggle", { entity_id: sw });
        };
      }
    }
  }

  class PvxTodayCard extends PvxBase {
    getCardSize() {
      return 2;
    }
    _render() {
      const h = this._hass;
      const c = this._config;
      const items = [
        ["PV Today", c.pv || "sensor.inverter_today_production", "kWh", 1],
        ["Load Today", c.load || "sensor.inverter_today_load_consumption", "kWh", 1],
        ["Import", c.imp || "sensor.inverter_today_energy_import", "kWh", 1],
        ["Export", c.exp || "sensor.inverter_today_energy_export", "kWh", 1],
        ["Bat Charge", c.bch || "sensor.inverter_today_battery_charge", "kWh", 1],
        ["Bat Discharge", c.bdch || "sensor.inverter_today_battery_discharge", "kWh", 1],
      ];
      const grid = items
        .map(([k, e, u, d]) => {
          const v = num(h, e, d);
          return `<div class="kpi"><div class="k">${k}</div><div class="v" style="color:var(--pv)">${fmt(v, u, d)}</div></div>`;
        })
        .join("");
      this._paint(
        shell(
          "#ffb000",
          `<div class="label">${c.title || "TODAY ENERGY"}</div>
           <div class="sub">Inverter daily totals</div>
           <div class="kpi-grid">${grid}</div>`
        )
      );
    }
  }

  const cards = [
    ["pvx-hero-card", PvxHeroCard, "PVX Hero", "Fotovoltaiko extreme hero banner"],
    ["pvx-kpi-card", PvxKpiCard, "PVX KPI", "Live PV/Load/Grid/Battery KPIs"],
    ["pvx-flow-card", PvxFlowCard, "PVX Power Flow", "Custom solar power topology"],
    ["pvx-battery-card", PvxBatteryCard, "PVX Basen Battery", "Basen SOC cockpit"],
    ["pvx-cells-card", PvxCellsCard, "PVX Cell Matrix", "16-cell voltage grid"],
    ["pvx-breaker-card", PvxBreakerCard, "PVX Breaker", "Breaker toggle + phases"],
    ["pvx-today-card", PvxTodayCard, "PVX Today", "Daily energy totals"],
  ];

  window.customCards = window.customCards || [];
  for (const [tag, Cls, name, description] of cards) {
    if (!customElements.get(tag)) customElements.define(tag, Cls);
    if (!window.customCards.find((x) => x.type === tag)) {
      window.customCards.push({ type: tag, name, description, preview: true });
    }
  }

  console.info(
    "%c PVX Extreme cards %c loaded ",
    "background:#ffb000;color:#000;font-weight:bold",
    "background:#0c1220;color:#39ff14"
  );
})();
