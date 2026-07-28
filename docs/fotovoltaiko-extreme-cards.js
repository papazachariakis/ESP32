/**
 * PVX Extreme — cinematic solar cockpit cards for Home Assistant
 * v2 — richer visuals, SVG gauges, animated flow
 */
(() => {
  const VER = "2.2.0";
  const FONT =
    "https://fonts.googleapis.com/css2?family=Orbitron:wght@500;700;900&family=Rajdhani:wght@500;600;700&family=JetBrains+Mono:wght@400;600;700&display=swap";

  const CSS = `
  :host{display:block;--pv:#ffcc33;--load:#5cff6a;--grid:#3de7ff;--bat:#c084fc;
    --ok:#34d399;--warn:#fbbf24;--bad:#fb7185;--charge:#38bdf8;--discharge:#fb923c;
    --text:#f4fbff;--muted:#8aa0b8;--dim:#4a6078;--line:rgba(120,160,200,.18);
    --glass:rgba(8,14,24,.72);--mono:'JetBrains Mono',ui-monospace,monospace;
    --display:'Orbitron',system-ui,sans-serif;--ui:'Rajdhani',system-ui,sans-serif}
  *{box-sizing:border-box}
  .pvx{
    position:relative;overflow:hidden;border-radius:18px;color:var(--text);
    font-family:var(--ui);border:1px solid var(--line);
    background:
      radial-gradient(ellipse 100% 80% at 15% -10%,rgba(255,204,51,.18),transparent 50%),
      radial-gradient(ellipse 70% 60% at 100% 0%,rgba(61,231,255,.12),transparent 45%),
      radial-gradient(ellipse 80% 70% at 80% 110%,rgba(192,132,252,.16),transparent 50%),
      linear-gradient(165deg,#070b14 0%,#0c1422 45%,#091018 100%);
    box-shadow:0 0 0 1px rgba(255,255,255,.03) inset,0 18px 50px rgba(0,0,0,.45);
  }
  .pvx::before{
    content:'';position:absolute;inset:0 0 auto 0;height:1px;z-index:3;
    background:linear-gradient(90deg,transparent,var(--accent,var(--pv)),transparent);
    opacity:.85;
  }
  .aurora{
    pointer-events:none;position:absolute;inset:-20%;z-index:0;opacity:.55;
    background:
      conic-gradient(from 120deg at 50% 40%,rgba(255,204,51,.08),transparent 30%,rgba(61,231,255,.08),transparent 60%,rgba(192,132,252,.1),transparent 85%);
    animation:spin 28s linear infinite;
  }
  @keyframes spin{to{transform:rotate(360deg)}}
  .grid{
    pointer-events:none;position:absolute;inset:0;z-index:0;opacity:.08;
    background-image:linear-gradient(rgba(140,180,220,.35) 1px,transparent 1px),
      linear-gradient(90deg,rgba(140,180,220,.35) 1px,transparent 1px);
    background-size:42px 42px;mask-image:radial-gradient(ellipse at center,black 30%,transparent 80%);
  }
  .scan{
    pointer-events:none;position:absolute;inset:0;z-index:2;opacity:.035;
    background:repeating-linear-gradient(0deg,transparent,transparent 2px,rgba(0,0,0,.55) 2px,rgba(0,0,0,.55) 4px);
  }
  .inner{position:relative;z-index:1;padding:18px 18px 16px}
  .eyebrow{
    display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:10px;
  }
  .eyebrow .tag{
    font-family:var(--display);font-size:.85rem;font-weight:700;letter-spacing:.18em;
    text-transform:uppercase;color:var(--accent,var(--pv));
    text-shadow:0 0 18px color-mix(in srgb,var(--accent,var(--pv)) 55%,transparent);
  }
  .eyebrow .live{
    display:inline-flex;align-items:center;gap:6px;padding:5px 11px;border-radius:999px;
    border:1px solid rgba(92,255,106,.35);background:rgba(92,255,106,.08);
    font-size:.78rem;letter-spacing:.1em;color:var(--load);font-weight:700;
  }
  .eyebrow .live i{
    width:7px;height:7px;border-radius:50%;background:var(--load);
    box-shadow:0 0 10px var(--load);animation:pulse 1.4s ease infinite;
  }
  @keyframes pulse{50%{opacity:.35;transform:scale(.85)}}
  .sub{color:var(--muted);font-size:1.05rem;margin:-2px 0 14px;letter-spacing:.02em}

  /* HERO */
  .hero-title{
    font-family:var(--display);font-weight:900;line-height:.95;
    font-size:clamp(2.2rem,6vw,3.4rem);letter-spacing:.06em;
    background:linear-gradient(105deg,#fff8d6 0%,#ffcc33 28%,#5cff6a 62%,#3de7ff 100%);
    -webkit-background-clip:text;background-clip:text;color:transparent;
    filter:drop-shadow(0 0 28px rgba(255,204,51,.25));
  }
  .hero-tag{
    margin-top:8px;font-family:var(--display);font-weight:700;letter-spacing:.28em;
    font-size:clamp(1rem,2.4vw,1.2rem);color:var(--load);
    text-shadow:0 0 20px rgba(92,255,106,.55);
  }
  .hero-meta{margin-top:12px;color:var(--muted);font-size:1.05rem;max-width:42rem;line-height:1.45}
  .hero-rail{
    margin-top:18px;display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px;
  }
  @media(max-width:700px){.hero-rail{grid-template-columns:repeat(2,minmax(0,1fr))}}
  .rail{
    position:relative;padding:14px 14px 13px;border-radius:14px;
    background:linear-gradient(160deg,rgba(255,255,255,.05),rgba(0,0,0,.28));
    border:1px solid var(--line);overflow:hidden;min-height:88px;
  }
  .rail::after{
    content:'';position:absolute;left:0;top:0;bottom:0;width:3px;
    background:var(--c,var(--pv));box-shadow:0 0 16px var(--c,var(--pv));
  }
  .rail .k{font-size:.9rem;letter-spacing:.12em;text-transform:uppercase;color:var(--muted);font-weight:600}
  .rail .v{margin-top:6px;font-family:var(--display);font-weight:900;font-size:1.65rem;color:var(--c,var(--text));line-height:1.1}
  .rail .u{font-size:.9rem;color:var(--muted);margin-top:2px}

  /* KPI */
  .kpi-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:12px}
  .kpi{
    position:relative;padding:16px 14px;border-radius:16px;text-align:left;
    background:linear-gradient(155deg,rgba(255,255,255,.05),rgba(0,0,0,.35));
    border:1px solid var(--line);overflow:hidden;min-height:104px;
    transition:transform .25s ease,border-color .25s ease;
  }
  .kpi:hover{transform:translateY(-2px);border-color:color-mix(in srgb,var(--c,var(--pv)) 45%,var(--line))}
  .kpi .glow{
    position:absolute;right:-20px;top:-30px;width:110px;height:110px;border-radius:50%;
    background:radial-gradient(circle,color-mix(in srgb,var(--c,var(--pv)) 35%,transparent),transparent 70%);
    opacity:.8;
  }
  .kpi .k{position:relative;font-size:.9rem;letter-spacing:.14em;text-transform:uppercase;color:var(--muted);font-weight:600}
  .kpi .v{position:relative;margin-top:10px;font-family:var(--display);font-weight:900;font-size:1.9rem;color:var(--c);line-height:1}
  .kpi .hint{position:relative;margin-top:8px;font-size:.95rem;color:var(--muted)}

  /* FLOW */
  .flow-stage{position:relative;min-height:400px;margin-top:4px}
  .flow-svg{width:100%;height:min(480px,78vw);display:block}
  .flow-node{
    fill:rgba(6,10,18,.88);stroke:var(--line);stroke-width:1.5;
    filter:drop-shadow(0 8px 18px rgba(0,0,0,.35));
  }
  .flow-node.on{stroke:var(--nc);filter:drop-shadow(0 0 12px color-mix(in srgb,var(--nc) 55%,transparent))}
  .flow-label{fill:var(--muted);font-family:var(--ui);font-size:15px;letter-spacing:.14em;text-transform:uppercase;font-weight:700}
  .flow-val{fill:var(--nc,#fff);font-family:var(--display);font-weight:700;font-size:22px}
  .flow-sub{fill:#b7c7da;font-family:var(--mono);font-size:13px;letter-spacing:.01em}
  .flow-dir{fill:var(--nc);font-family:var(--display);font-size:13px;font-weight:700;letter-spacing:.1em}
  .flow-hub{
    fill:url(#hubGrad);stroke:rgba(255,255,255,.22);stroke-width:1.5;
  }
  .beam{fill:none;stroke:var(--bc);stroke-width:2.5;stroke-linecap:round;opacity:.25}
  .beam.on{opacity:.95;stroke-dasharray:8 10;animation:dash 1.1s linear infinite;
    filter:drop-shadow(0 0 6px var(--bc))}
  .beam.rev.on{animation-direction:reverse}
  @keyframes dash{to{stroke-dashoffset:-36}}
  .flow-details{
    margin-top:14px;display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px;
  }
  @media(max-width:720px){.flow-details{grid-template-columns:repeat(2,minmax(0,1fr))}}
  .flow-chip{
    padding:12px 13px;border-radius:12px;border:1px solid var(--line);
    background:linear-gradient(160deg,rgba(255,255,255,.04),rgba(0,0,0,.28));
  }
  .flow-chip .k{font-size:.88rem;letter-spacing:.1em;text-transform:uppercase;color:var(--muted);font-weight:600}
  .flow-chip .v{margin-top:5px;font-family:var(--mono);font-size:1rem;color:var(--text);line-height:1.4}
  .flow-chip .v b{color:var(--c,var(--pv));font-family:var(--display);font-weight:700;font-size:1.15rem}

  /* BATTERY */
  .bat-layout{display:grid;grid-template-columns:minmax(180px,220px) 1fr;gap:18px;align-items:center}
  @media(max-width:640px){.bat-layout{grid-template-columns:1fr}}
  .gauge{position:relative;width:100%;max-width:220px;margin:0 auto;aspect-ratio:1}
  .gauge svg{width:100%;height:100%;transform:rotate(-135deg)}
  .gauge .mid{
    position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;
    text-align:center;pointer-events:none;
  }
  .gauge .pct{font-family:var(--display);font-weight:900;font-size:2.8rem;line-height:1;
    text-shadow:0 0 24px color-mix(in srgb,var(--ring) 50%,transparent)}
  .gauge .cap{margin-top:4px;font-size:.95rem;letter-spacing:.14em;color:var(--muted);text-transform:uppercase}
  .gauge .mode{margin-top:8px;font-family:var(--display);font-size:1rem;font-weight:700;letter-spacing:.08em}
  .bat-stats{display:grid;grid-template-columns:1fr 1fr;gap:10px}
  .stat{
    padding:12px 12px;border-radius:14px;border:1px solid var(--line);
    background:linear-gradient(160deg,rgba(255,255,255,.04),rgba(0,0,0,.3));
  }
  .stat .k{font-size:.88rem;letter-spacing:.1em;text-transform:uppercase;color:var(--muted);font-weight:600}
  .stat .v{margin-top:4px;font-family:var(--display);font-weight:700;font-size:1.35rem}
  .pills{display:flex;flex-wrap:wrap;gap:8px;margin-top:14px}
  .pill{
    padding:7px 14px;border-radius:999px;font-size:.85rem;letter-spacing:.08em;text-transform:uppercase;
    border:1px solid var(--line);color:var(--muted);font-weight:700;background:rgba(0,0,0,.25);
  }
  .pill.on{color:var(--ok);border-color:rgba(52,211,153,.45);box-shadow:0 0 14px rgba(52,211,153,.2)}
  .pill.chg{color:var(--charge);border-color:rgba(56,189,248,.45)}
  .pill.dsg{color:var(--discharge);border-color:rgba(251,146,60,.45)}
  .pill.bad{color:var(--bad);border-color:rgba(251,113,133,.5);animation:pulse .8s ease infinite}

  /* CELLS */
  .cells{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}
  @media(min-width:760px){.cells{grid-template-columns:repeat(8,minmax(0,1fr))}}
  .cell{
    padding:8px 6px;border-radius:12px;border:1px solid var(--line);text-align:center;
    background:rgba(0,0,0,.28);transition:border-color .2s,box-shadow .2s;
  }
  .cell .i{font-size:.8rem;color:var(--muted);letter-spacing:.06em;font-weight:600}
  .cell .bar{
    height:42px;margin:6px 2px;border-radius:6px;background:rgba(255,255,255,.04);
    display:flex;align-items:flex-end;overflow:hidden;
  }
  .cell .bar span{
    display:block;width:100%;border-radius:5px 5px 0 0;
    background:linear-gradient(180deg,var(--ok),rgba(52,211,153,.25));
    box-shadow:0 0 12px rgba(52,211,153,.25);transition:height .45s ease;
  }
  .cell .cv{font-family:var(--display);font-weight:700;font-size:1rem}
  .cell.hi{border-color:rgba(251,113,133,.55);box-shadow:0 0 14px rgba(251,113,133,.18)}
  .cell.hi .bar span{background:linear-gradient(180deg,var(--bad),rgba(251,113,133,.2))}
  .cell.lo{border-color:rgba(56,189,248,.5);box-shadow:0 0 14px rgba(56,189,248,.16)}
  .cell.lo .bar span{background:linear-gradient(180deg,var(--charge),rgba(56,189,248,.2))}

  /* TODAY / BREAKER */
  .btn{
    width:100%;padding:14px 16px;border-radius:14px;border:1px solid var(--line);
    background:linear-gradient(160deg,rgba(255,255,255,.05),rgba(0,0,0,.35));
    color:var(--text);font-family:var(--display);font-weight:700;letter-spacing:.12em;
    font-size:.95rem;cursor:pointer;text-transform:uppercase;
  }
  .btn.on{border-color:rgba(52,211,153,.5);color:var(--ok);box-shadow:0 0 22px rgba(52,211,153,.2)}
  .btn.off{border-color:rgba(251,113,133,.45);color:var(--bad);box-shadow:0 0 22px rgba(251,113,133,.15)}
  .phases{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:12px}
  `;

  function ensureFont() {
    if (document.getElementById("pvx-font-v2")) return;
    const l = document.createElement("link");
    l.id = "pvx-font-v2";
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
    return `${Number(v).toFixed(digits)}${unit ? " " + unit : ""}`;
  }

  function fmtW(v) {
    if (v === null || v === undefined) return "—";
    const a = Math.abs(v);
    if (a >= 1000) return `${(v / 1000).toFixed(2)} kW`;
    return `${Math.round(v)} W`;
  }

  function shell(accent, body, extraClass = "") {
    return `
      <div class="pvx ${extraClass}" style="--accent:${accent}">
        <div class="aurora"></div><div class="grid"></div><div class="scan"></div>
        <div class="inner">${body}</div>
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
      if (!this.shadowRoot) this.attachShadow({ mode: "open" });
      this._render();
    }
    _root() {
      if (!this.shadowRoot) this.attachShadow({ mode: "open" });
      return this.shadowRoot;
    }
    _paint(html) {
      this._root().innerHTML = `<style>${CSS}</style>${html}`;
    }
  }

  class PvxHeroCard extends PvxBase {
    getCardSize() {
      return 4;
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
          "#ffcc33",
          `<div class="eyebrow"><span class="tag">SOLAR COMMAND</span><span class="live"><i></i>LIVE</span></div>
           <div class="hero-title">${c.title || "ΦΩΤΟΒΟΛΤΑΪΚΟ"}</div>
           <div class="hero-tag">${c.tag || "EXTREME COCKPIT"}</div>
           <div class="hero-meta">${c.meta || "Deye inverter · Basen 30kWh · Grid · Breaker · real-time energy theatre"}</div>
           <div class="hero-rail">
             <div class="rail" style="--c:var(--pv)"><div class="k">PV</div><div class="v">${fmtW(pv)}</div></div>
             <div class="rail" style="--c:var(--load)"><div class="k">Load</div><div class="v">${fmtW(load)}</div></div>
             <div class="rail" style="--c:var(--grid)"><div class="k">Grid</div><div class="v">${fmtW(grid)}</div></div>
             <div class="rail" style="--c:var(--bat)"><div class="k">Battery</div><div class="v">${fmtW(bat)}</div></div>
           </div>`
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
      const items = [
        ["PV", c.pv_entity || "sensor.inverter_pv_power", "var(--pv)", "Solar harvest"],
        ["Load", c.load_entity || "sensor.inverter_load_power", "var(--load)", "House demand"],
        ["Grid", c.grid_entity || "sensor.inverter_grid_power", "var(--grid)", "Import / export"],
        ["Battery", c.battery_entity || "sensor.inverter_battery_power", "var(--bat)", "Charge rail"],
      ];
      const grid = items
        .map(([k, e, col, hint]) => {
          const v = num(h, e, 0);
          return `<div class="kpi" style="--c:${col}"><div class="glow"></div>
            <div class="k">${k}</div><div class="v">${fmtW(v)}</div><div class="hint">${hint}</div></div>`;
        })
        .join("");
      this._paint(
        shell(
          "#3de7ff",
          `<div class="eyebrow"><span class="tag">${c.title || "LIVE POWER"}</span></div>
           <div class="sub">${c.subtitle || "Four rails · live inverter telemetry"}</div>
           <div class="kpi-grid">${grid}</div>`
        )
      );
    }
  }

  class PvxFlowCard extends PvxBase {
    getCardSize() {
      return 7;
    }
    _render() {
      const h = this._hass;
      const c = this._config;
      const pv = num(h, c.pv_entity || "sensor.inverter_pv_power", 0) || 0;
      const load = num(h, c.load_entity || "sensor.inverter_load_power", 0) || 0;
      const grid = num(h, c.grid_entity || "sensor.inverter_grid_power", 0) || 0;
      const bat = num(h, c.battery_entity || "sensor.inverter_battery_power", 0) || 0;
      const soc = num(h, c.soc_entity || "sensor.inverter_battery", 0);
      const pv1 = num(h, c.pv1_entity || "sensor.inverter_pv1_power", 0);
      const pv2 = num(h, c.pv2_entity || "sensor.inverter_pv2_power", 0);
      const pv1v = num(h, c.pv1_v || "sensor.inverter_pv1_voltage", 1);
      const pv2v = num(h, c.pv2_v || "sensor.inverter_pv2_voltage", 1);
      const pv1a = num(h, c.pv1_a || "sensor.inverter_pv1_current", 1);
      const pv2a = num(h, c.pv2_a || "sensor.inverter_pv2_current", 1);
      const batV = num(h, c.bat_v || "sensor.inverter_battery_voltage", 1);
      const batA = num(h, c.bat_a || "sensor.inverter_battery_current", 1);
      const batT = num(h, c.bat_t || "sensor.inverter_battery_temperature", 1);
      const l1 = num(h, c.l1 || "sensor.inverter_load_l1_power", 0);
      const l2 = num(h, c.l2 || "sensor.inverter_load_l2_power", 0);
      const l3 = num(h, c.l3 || "sensor.inverter_load_l3_power", 0);
      const freq = num(h, c.freq || "sensor.inverter_output_frequency", 2);
      const invP = num(h, c.inv_p || "sensor.inverter_power", 0);
      const st = state(h, c.status || "sensor.inverter_device_state");
      const dayPv = num(h, c.day_pv || "sensor.inverter_today_production", 1);
      const dayLoad = num(h, c.day_load || "sensor.inverter_today_load_consumption", 1);
      const dayImp = num(h, c.day_imp || "sensor.inverter_today_energy_import", 1);
      const dayExp = num(h, c.day_exp || "sensor.inverter_today_energy_export", 1);
      const dayBch = num(h, c.day_bch || "sensor.inverter_today_battery_charge", 1);
      const dayBd = num(h, c.day_bd || "sensor.inverter_today_battery_discharge", 1);
      const basenSoc = num(h, c.basen_soc || "sensor.tp_bstbd_25c_2_state_of_charge", 0);
      const basenW = num(h, c.basen_w || "sensor.tp_bstbd_25c_2_power", 0);

      const on = (v) => (Math.abs(v || 0) > 25 ? "on" : "");
      const batDir = bat > 40 ? "CHARGE" : bat < -40 ? "DISCHARGE" : "IDLE";
      const gridDir = grid > 40 ? "IMPORT" : grid < -40 ? "EXPORT" : "IDLE";
      const esc = (s) => String(s ?? "—").replace(/[<>&]/g, "");

      this._paint(
        shell(
          "#5cff6a",
          `<div class="eyebrow"><span class="tag">${c.title || "POWER FLOW"}</span><span class="live"><i></i>DETAILED</span></div>
           <div class="sub">PV strings · 3-phase load · battery rails · grid direction · daily energy</div>
           <div class="flow-stage">
             <svg class="flow-svg" viewBox="0 0 720 430" preserveAspectRatio="xMidYMid meet">
               <defs>
                 <linearGradient id="hubGrad" x1="0" y1="0" x2="1" y2="1">
                   <stop offset="0%" stop-color="#1a2740"/><stop offset="100%" stop-color="#0a1220"/>
                 </linearGradient>
               </defs>
               <path class="beam ${on(pv)}" style="--bc:var(--pv)" d="M360 108 L360 168"/>
               <path class="beam ${on(bat)} ${bat < 0 ? "rev" : ""}" style="--bc:var(--bat)" d="M198 214 L278 214"/>
               <path class="beam ${on(load)}" style="--bc:var(--load)" d="M442 214 L522 214"/>
               <path class="beam ${on(grid)} ${grid < 0 ? "rev" : ""}" style="--bc:var(--grid)" d="M360 260 L360 318"/>

               <!-- SOLAR -->
               <rect class="flow-node ${on(pv)}" style="--nc:var(--pv)" x="250" y="18" width="220" height="92" rx="16"/>
               <text class="flow-label" x="360" y="40" text-anchor="middle">Solar PV</text>
               <text class="flow-val" style="--nc:var(--pv)" x="360" y="64" text-anchor="middle">${fmtW(pv)}</text>
               <text class="flow-sub" x="360" y="82" text-anchor="middle">PV1 ${fmtW(pv1)} · ${fmt(pv1v, "V", 0)} · ${fmt(pv1a, "A", 1)}</text>
               <text class="flow-sub" x="360" y="96" text-anchor="middle">PV2 ${fmtW(pv2)} · ${fmt(pv2v, "V", 0)} · ${fmt(pv2a, "A", 1)}</text>

               <!-- BATTERY -->
               <rect class="flow-node ${on(bat)}" style="--nc:var(--bat)" x="28" y="168" width="172" height="108" rx="16"/>
               <text class="flow-label" x="114" y="190" text-anchor="middle">Battery</text>
               <text class="flow-val" style="--nc:var(--bat)" x="114" y="214" text-anchor="middle">${fmtW(bat)}</text>
               <text class="flow-dir" style="--nc:var(--bat)" x="114" y="230" text-anchor="middle">${batDir}</text>
               <text class="flow-sub" x="114" y="248" text-anchor="middle">SOC ${soc ?? "—"}% · ${fmt(batV, "V", 1)}</text>
               <text class="flow-sub" x="114" y="262" text-anchor="middle">${fmt(batA, "A", 1)} · ${fmt(batT, "°C", 0)}</text>

               <!-- INVERTER HUB -->
               <rect class="flow-hub" x="286" y="168" width="148" height="108" rx="18"/>
               <text class="flow-label" x="360" y="190" text-anchor="middle" style="fill:#c9d7ea">Inverter</text>
               <text class="flow-val" style="--nc:#f4fbff" x="360" y="214" text-anchor="middle">DEYE</text>
               <text class="flow-sub" x="360" y="234" text-anchor="middle">${fmtW(invP)} · ${fmt(freq, "Hz", 2)}</text>
               <text class="flow-sub" x="360" y="252" text-anchor="middle">${esc(st)}</text>

               <!-- LOAD -->
               <rect class="flow-node ${on(load)}" style="--nc:var(--load)" x="520" y="168" width="172" height="108" rx="16"/>
               <text class="flow-label" x="606" y="190" text-anchor="middle">Load</text>
               <text class="flow-val" style="--nc:var(--load)" x="606" y="214" text-anchor="middle">${fmtW(load)}</text>
               <text class="flow-sub" x="606" y="236" text-anchor="middle">L1 ${fmtW(l1)}</text>
               <text class="flow-sub" x="606" y="250" text-anchor="middle">L2 ${fmtW(l2)} · L3 ${fmtW(l3)}</text>

               <!-- GRID -->
               <rect class="flow-node ${on(grid)}" style="--nc:var(--grid)" x="250" y="318" width="220" height="92" rx="16"/>
               <text class="flow-label" x="360" y="340" text-anchor="middle">Grid</text>
               <text class="flow-val" style="--nc:var(--grid)" x="360" y="364" text-anchor="middle">${fmtW(grid)}</text>
               <text class="flow-dir" style="--nc:var(--grid)" x="360" y="382" text-anchor="middle">${gridDir}</text>
               <text class="flow-sub" x="360" y="398" text-anchor="middle">Today import ${fmt(dayImp, "kWh", 1)} · export ${fmt(dayExp, "kWh", 1)}</text>
             </svg>
           </div>
           <div class="flow-details">
             <div class="flow-chip" style="--c:var(--pv)"><div class="k">PV today</div><div class="v"><b>${fmt(dayPv, "kWh", 1)}</b><br>strings ${fmtW(pv1)} + ${fmtW(pv2)}</div></div>
             <div class="flow-chip" style="--c:var(--load)"><div class="k">Load today</div><div class="v"><b>${fmt(dayLoad, "kWh", 1)}</b><br>L1/L2/L3 ${fmtW(l1)} / ${fmtW(l2)} / ${fmtW(l3)}</div></div>
             <div class="flow-chip" style="--c:var(--bat)"><div class="k">Battery today</div><div class="v"><b>Δ ${fmt(dayBch, "", 1)} / ${fmt(dayBd, "", 1)} kWh</b><br>Basen ${basenSoc ?? "—"}% · ${fmtW(basenW)}</div></div>
             <div class="flow-chip" style="--c:var(--grid)"><div class="k">Grid today</div><div class="v"><b>${gridDir}</b><br>imp ${fmt(dayImp, "kWh", 1)} · exp ${fmt(dayExp, "kWh", 1)}</div></div>
           </div>`
        )
      );
    }
  }

  class PvxBatteryCard extends PvxBase {
    getCardSize() {
      return 5;
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
      let ring = "#c084fc";
      let mode = "IDLE";
      let modeCol = "var(--muted)";
      if (soc > 70) ring = "#34d399";
      else if (soc > 35) ring = "#fbbf24";
      else ring = "#fb7185";
      if (chg) {
        mode = "CHARGING";
        modeCol = "var(--charge)";
      } else if (dsg) {
        mode = "DISCHARGING";
        modeCol = "var(--discharge)";
      }
      // 270° arc: circumference ~ 414 for r=88, usable 310.5
      const C = 2 * Math.PI * 88;
      const usable = C * 0.75;
      const offset = C * 0.25 + usable * (1 - Math.max(0, Math.min(100, soc)) / 100);
      this._paint(
        shell(
          "#c084fc",
          `<div class="eyebrow"><span class="tag">${c.title || "BASEN 30kWh"}</span><span class="live"><i></i>BLE · MQTT</span></div>
           <div class="sub">ESP32 Classic discovery · lithium command center</div>
           <div class="bat-layout">
             <div class="gauge" style="--ring:${ring}">
               <svg viewBox="0 0 200 200" aria-hidden="true">
                 <circle cx="100" cy="100" r="88" fill="none" stroke="rgba(255,255,255,.06)" stroke-width="12"/>
                 <circle cx="100" cy="100" r="88" fill="none" stroke="rgba(255,255,255,.1)" stroke-width="12"
                   stroke-dasharray="${usable} ${C}" stroke-dashoffset="${C * 0.25}" stroke-linecap="round"/>
                 <circle cx="100" cy="100" r="88" fill="none" stroke="${ring}" stroke-width="12"
                   stroke-dasharray="${usable} ${C}" stroke-dashoffset="${offset}" stroke-linecap="round"
                   style="filter:drop-shadow(0 0 8px ${ring});transition:stroke-dashoffset .6s ease"/>
               </svg>
               <div class="mid">
                 <div class="pct" style="color:${ring}">${soc}%</div>
                 <div class="cap">State of charge</div>
                 <div class="mode" style="color:${modeCol}">${mode}</div>
               </div>
             </div>
             <div>
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
               <div class="pills">
                 <span class="pill ${chg ? "chg on" : ""}">Charging</span>
                 <span class="pill ${dsg ? "dsg on" : ""}">Discharging</span>
                 <span class="pill ${bal ? "on" : ""}">Balancing</span>
                 <span class="pill ${prob ? "bad" : "on"}">${prob ? "Problem" : "Healthy"}</span>
               </div>
             </div>
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
      const span = hi !== null && lo !== null && hi !== lo ? hi - lo : 0.05;
      const cells = vals
        .map(({ i, v }) => {
          let cls = "";
          if (v !== null && hi !== null && v === hi && hi !== lo) cls = "hi";
          if (v !== null && lo !== null && v === lo && hi !== lo) cls = "lo";
          const pct =
            v === null || lo === null ? 40 : Math.max(12, Math.min(100, ((v - (lo - 0.01)) / (span + 0.02)) * 100));
          return `<div class="cell ${cls}"><div class="i">C${String(i).padStart(2, "0")}</div>
            <div class="bar"><span style="height:${pct}%"></span></div>
            <div class="cv">${v === null ? "—" : v.toFixed(3)}</div></div>`;
        })
        .join("");
      this._paint(
        shell(
          "#38bdf8",
          `<div class="eyebrow"><span class="tag">${c.title || "CELL MATRIX"}</span></div>
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
          "#fb7185",
          `<div class="eyebrow"><span class="tag">${c.title || "BREAKER"}</span></div>
           <div class="sub">3-phase · temperature ${fmt(temp, "°C", 1)}</div>
           <button class="btn ${on ? "on" : "off"}" data-act="toggle">${on ? "ONLINE · TAP TO TOGGLE" : "OFFLINE · TAP TO TOGGLE"}</button>
           <div class="phases">
             <div class="stat"><div class="k">Phase A</div><div class="v">${fmtW(pa)}</div></div>
             <div class="stat"><div class="k">Phase B</div><div class="v">${fmtW(pb)}</div></div>
             <div class="stat"><div class="k">Phase C</div><div class="v">${fmtW(pc)}</div></div>
           </div>`
        )
      );
      const btn = this.shadowRoot.querySelector("button[data-act=toggle]");
      if (btn) {
        btn.onclick = () => this._hass?.callService("switch", "toggle", { entity_id: sw });
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
        ["PV Today", c.pv || "sensor.inverter_today_production", "kWh", 1, "var(--pv)"],
        ["Load Today", c.load || "sensor.inverter_today_load_consumption", "kWh", 1, "var(--load)"],
        ["Import", c.imp || "sensor.inverter_today_energy_import", "kWh", 1, "var(--grid)"],
        ["Export", c.exp || "sensor.inverter_today_energy_export", "kWh", 1, "var(--grid)"],
        ["Bat Charge", c.bch || "sensor.inverter_today_battery_charge", "kWh", 1, "var(--charge)"],
        ["Bat Discharge", c.bdch || "sensor.inverter_today_battery_discharge", "kWh", 1, "var(--discharge)"],
      ];
      const grid = items
        .map(([k, e, u, d, col]) => {
          const v = num(h, e, d);
          return `<div class="kpi" style="--c:${col}"><div class="glow"></div>
            <div class="k">${k}</div><div class="v">${fmt(v, u, d)}</div></div>`;
        })
        .join("");
      this._paint(
        shell(
          "#ffcc33",
          `<div class="eyebrow"><span class="tag">${c.title || "TODAY ENERGY"}</span></div>
           <div class="sub">Daily inverter totals</div>
           <div class="kpi-grid">${grid}</div>`
        )
      );
    }
  }

  const cards = [
    ["pvx-hero-card", PvxHeroCard, "PVX Hero", "Cinematic fotovoltaiko hero"],
    ["pvx-kpi-card", PvxKpiCard, "PVX KPI", "Live power KPI tiles"],
    ["pvx-flow-card", PvxFlowCard, "PVX Power Flow", "Animated SVG energy topology"],
    ["pvx-battery-card", PvxBatteryCard, "PVX Basen Battery", "Basen SOC cockpit"],
    ["pvx-cells-card", PvxCellsCard, "PVX Cell Matrix", "16-cell voltage bars"],
    ["pvx-breaker-card", PvxBreakerCard, "PVX Breaker", "Breaker + phases"],
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
    `%c PVX Extreme ${VER} %c cinematic cockpit `,
    "background:#ffcc33;color:#111;font-weight:bold",
    "background:#0c1422;color:#5cff6a"
  );
})();
