/* Tianpower / Basen BMS bitmask labels (from syssi/esphome-tianpower-bms) */
(function (global) {
  const SKIP = /^(Reserved|—)$/i;

  const VOLT_PROT = [
    'Υπερτάση κελιού (προστασία)', 'Υποτάση κελιού (προστασία)',
    'Υπερτάση πακέτου (προστασία)', 'Υποτάση πακέτου (προστασία)',
    'Υπερτάση κελιού (συναγερμός)', 'Υποτάση κελιού (συναγερμός)',
    'Υπερτάση πακέτου (συναγερμός)', 'Υποτάση πακέτου (συναγερμός)',
    'Διαφορά τάσης κελιών (συναγερμός)', null, null, null, null, null,
    'Μετάβαση σε sleep', null
  ];

  const TEMP_PROT = [
    'Υπερθέρμανση φόρτισης (προστασία)', 'Υποθερμία φόρτισης (προστασία)',
    'Υπερθέρμανση εκφόρτισης (προστασία)', 'Υποθερμία εκφόρτισης (προστασία)',
    'Υπερθέρμανση περιβάλλοντος (προστασία)', 'Υποθερμία περιβάλλοντος (προστασία)',
    'Υπερθέρμανση MOSFET (προστασία)', 'Υποθερμία MOSFET (προστασία)',
    'Υπερθέρμανση φόρτισης (συναγερμός)', 'Υποθερμία φόρτισης (συναγερμός)',
    'Υπερθέρμανση εκφόρτισης (συναγερμός)', 'Υποθερμία εκφόρτισης (συναγερμός)',
    'Υπερθέρμανση περιβάλλοντος (συναγερμός)', 'Υποθερμία περιβάλλοντος (συναγερμός)',
    'Υπερθέρμανση MOSFET (συναγερμός)', 'Υποθερμία MOSFET (συναγερμός)'
  ];

  const CURR_PROT = [
    'Υπερρεύμα φόρτισης (προστασία)', 'Βραχυκύκλωμα (προστασία)',
    'Υπερρεύμα εκφόρτισης 1 (προστασία)', 'Υπερρεύμα εκφόρτισης 2 (προστασία)',
    'Υπερρεύμα φόρτισης (συναγερμός)', 'Υπερρεύμα εκφόρτισης (συναγερμός)',
    'Gyro lock (συναγερμός)'
  ];

  const ERRORS = [
    'Διαφορά τάσης κελιών (συναγερμός)', 'Βλάβη charge MOS (συναγερμός)',
    'Βλάβη SD κάρτας (συναγερμός)', 'Βλάβη SPI (συναγερμός)',
    'Βλάβη EEPROM (συναγερμός)', 'LED alarm enable', 'Buzzer alarm enable',
    'Χαμηλή μπαταρία (συναγερμός)', 'Υπερθέρμανση MOSFET (προστασία)',
    'Υπερθέρμανση MOSFET (συναγερμός)', 'Βλάβη πλακέτας περιορισμού ρεύματος',
    'Βλάβη sampling', 'Βλάβη μπαταρίας', 'Βλάβη NTC',
    'Βλάβη charge MOS', 'Βλάβη discharge MOS'
  ];

  function decodeBitmask(mask, labels) {
    const m = Number(mask) >>> 0;
    if (!m) return [];
    const out = [];
    for (let i = 0; i < labels.length; i++) {
      const label = labels[i];
      if (!label || SKIP.test(label)) continue;
      if (m & (1 << i)) out.push(label);
    }
    return out;
  }

  function decodeAlarmMask32(mask) {
    const m = Number(mask) >>> 0;
    if (!m) return [];
    const hi = (m >>> 16) & 0xffff;
    const lo = m & 0xffff;
    const out = [];
    if (lo) out.push('Low alarm mask: ' + '0x' + lo.toString(16).toUpperCase());
    if (hi) out.push('High alarm mask: ' + '0x' + hi.toString(16).toUpperCase());
    return out;
  }

  function isBasenType(b) {
    const t = String(b && b.type || '').toLowerCase();
    return t === 'basen' || t === 'tianpower' || t === 'tp';
  }

  function buildBmsAlarmSections(b) {
    if (!b || !b.valid) return [];
    const sections = [];
    const basen = isBasenType(b);

    const add = (title, items, level) => {
      if (items && items.length) sections.push({ title, items, level: level || 'warn' });
    };

    if (basen) {
      add('Προστασία τάσης', decodeBitmask(b.voltage_prot_mask, VOLT_PROT));
      add('Προστασία ρεύματος', decodeBitmask(b.current_prot_mask, CURR_PROT));
      add('Προστασία θερμοκρασίας', decodeBitmask(b.temp_prot_mask, TEMP_PROT));
      add('Σφάλματα BMS', decodeBitmask(b.error_mask, ERRORS), 'bad');
      add('Επιπλέον συναγερμοί', decodeAlarmMask32(b.alarm_mask));
    } else {
      if (b.voltage_prot_mask) add('Προστασία τάσης', [hexMask(b.voltage_prot_mask)]);
      if (b.current_prot_mask) add('Προστασία ρεύματος', [hexMask(b.current_prot_mask)]);
      if (b.temp_prot_mask) add('Προστασία θερμοκρασίας', [hexMask(b.temp_prot_mask)]);
      if (b.error_mask) add('Σφάλματα', [hexMask(b.error_mask)], 'bad');
      if (b.alarm_mask) add('Συναγερμοί', [hexMask(b.alarm_mask, true)]);
    }

    return sections;
  }

  function hexMask(v, wide) {
    const n = Number(v) >>> 0;
    return '0x' + (wide && n > 0xffff ? n.toString(16) : (n & 0xffff).toString(16)).toUpperCase();
  }

  function renderBmsAlarmsHtml(b) {
    const sections = buildBmsAlarmSections(b);
    if (!sections.length) {
      return '<div class="small muted">Κανένας ενεργός συναγερμός ή προστασία.</div>';
    }
    return sections.map(s => `
      <div class="alarm-group ${s.level}">
        <div class="alarm-title">${s.title}</div>
        <ul>${s.items.map(i => `<li>${i}</li>`).join('')}</ul>
      </div>`).join('');
  }

  function renderCoreTempsHtml(b, f2) {
    if (!b || !b.valid) return '<div class="small muted">—</div>';
    const rows = [];
    const add = (label, key, val) => {
      const n = Number(val);
      if (Number.isFinite(n) && n > -50 && n < 150) {
        rows.push(`<div class="temp chartable" data-chart-key="${key}" data-chart-label="${label}" data-chart-unit="°C" data-chart-dec="1" title="Κλικ για διάγραμμα"><div class="n">${label}</div><div class="tv">${f2(n, 1)}°C</div></div>`);
      }
    };
    add('Πακέτο', 'bms.avg_temp', b.avg_temp);
    add('Περιβάλλον', 'bms.ambient_temp', b.ambient_temp);
    add('MOSFET', 'bms.mosfet_temp', b.mosfet_temp);
    return rows.length ? rows.join('') : '<div class="small muted">—</div>';
  }

  global.BmsDecode = {
    decodeBitmask, buildBmsAlarmSections, renderBmsAlarmsHtml, renderCoreTempsHtml, isBasenType
  };
})(typeof window !== 'undefined' ? window : globalThis);
