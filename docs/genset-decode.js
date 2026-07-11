/* Cummins PS0600 / C22D5 — NFPA 110 & fault code decode (cummins_fault_bits.json) */
(function (global) {
  const RESP_EL = {
    Warning: 'Προειδοποίηση',
    Shutdown: 'Ακινητοποίηση',
    Derate: 'Μείωση ισχύος',
    Alarm: 'Συναγερμός',
    Lamp: 'Ένδειξη',
    None: '—'
  };

  const NFPA_EL = {
    'Common Alarm': 'Γενικός συναγερμός',
    'Genset Supplying Load': 'Γεννήτρια τροφοδοτεί φορτίο',
    'Genset Running': 'Γεννήτρια σε λειτουργία',
    'Not in Auto': 'Δεν είναι σε Auto',
    'High Battery Voltage': 'Υψηλή τάση μπαταρίας',
    'Low Battery Voltage': 'Χαμηλή τάση μπαταρίας',
    'Charger AC Failure': 'Βλάβη φορτιστή AC',
    'Fail to Start': 'Αποτυχία εκκίνησης',
    'Low Coolant Temperature': 'Χαμηλή θερμ. ψυκτικού',
    'Pre-high Engine Temperature': 'Προ-υψηλή θερμ. κινητήρα',
    'High Engine Temperature': 'Υψηλή θερμ. κινητήρα',
    'Pre-low Oil Pressure': 'Προ-χαμηλή πίεση λαδιού',
    'Low Oil Pressure': 'Χαμηλή πίεση λαδιού',
    'Overspeed': 'Υπερτάχυνση',
    'Low Coolant Level': 'Χαμηλό επίπεδο ψυκτικού',
    'Low Fuel Level': 'Χαμηλό επίπεδο καυσίμου'
  };

  const EXT_EL = {
    'Check Genset': 'Έλεγχος γεννήτριας',
    'Ground Fault': 'Βλάβη γείωσης',
    'High AC Voltage': 'Υψηλή τάση AC',
    'Low AC Voltage': 'Χαμηλή τάση AC',
    'Under Frequency': 'Υπο-συχνότητα',
    'Overload': 'Υπερφόρτωση',
    'Overcurrent': 'Υπερρεύμα',
    'Short Circuit': 'Βραχυκύκλωμα',
    'Reverse kW': 'Αντίστροφο kW',
    'Reverse kVAR': 'Αντίστροφο kVAR',
    'Fail to Sync': 'Αποτυχία συγχρονισμού',
    'Fail to Close': 'Αποτυχία κλεισίματος',
    'Load Demand': 'Αίτημα φορτίου',
    'Genset Circuit Breaker Tripped': 'Πτώση διακόπτη γεννήτριας',
    'Utility Circuit Breaker Tripped': 'Πτώση διακόπτη δικτύου',
    'Emergency Stop': 'Emergency Stop'
  };

  let faultDb = null;
  let codeIndex = null;

  function setFaultDb(db) {
    faultDb = db || null;
    codeIndex = new Map();
    if (!db || !db.fault_bitmap) return;
    for (const reg of Object.keys(db.fault_bitmap)) {
      for (const f of db.fault_bitmap[reg]) {
        const code = Number(f.code);
        if (!codeIndex.has(code)) codeIndex.set(code, []);
        codeIndex.get(code).push(Object.assign({}, f, { reg: Number(reg) }));
      }
    }
  }

  function lookupFaultCode(code) {
    const c = Number(code);
    if (!c || !codeIndex) return [];
    return codeIndex.get(c) || [];
  }

  function labelEl(en, map) {
    return (map && map[en]) ? map[en] + ' (' + en + ')' : en;
  }

  function decodeWordBits(word, bitMap, elMap) {
    const w = Number(word) & 0xffff;
    if (!w) return [];
    const out = [];
    for (let b = 15; b >= 0; b--) {
      if (!(w & (1 << b))) continue;
      const key = bitMap && (bitMap[String(b)] || bitMap[b]);
      out.push(key ? labelEl(key, elMap) : ('Bit ' + b));
    }
    return out;
  }

  function decodeNfpaFault(val) {
    const v = Number(val) >>> 0;
    if (!v) return { nfpa: [], ext: [], raw: 0, wordNfpa: 0, wordExt: 0 };
    const wordNfpa = (v >>> 16) & 0xffff;
    const wordExt = v & 0xffff;
    const nfpaMap = faultDb && faultDb.nfpa;
    const extMap = faultDb && faultDb.ext;
    return {
      raw: v,
      wordNfpa,
      wordExt,
      nfpa: decodeWordBits(wordNfpa, nfpaMap, NFPA_EL),
      ext: decodeWordBits(wordExt, extMap, EXT_EL)
    };
  }

  function respLevel(resp, faultType) {
    if (Number(faultType) === 2) return 'bad';
    const r = String(resp || '');
    if (r === 'Shutdown') return 'bad';
    if (r === 'Derate' || r === 'Warning' || r === 'Alarm') return 'warn';
    return 'warn';
  }

  function buildGensetAlarmSections(g) {
    if (!g) return [];
    const sections = [];
    const code = Number(g.active_fault);

    if (code > 0) {
      const matches = lookupFaultCode(code);
      const primary = matches[0];
      const items = [];
      if (primary) {
        items.push('Modbus register: ' + primary.reg + ', bit ' + primary.bit);
        items.push('Απόκριση controller: ' + (RESP_EL[primary.resp] || primary.resp));
        if (matches.length > 1) {
          items.push('Σχετικά registers: ' + matches.map(m => m.reg).join(', '));
        }
      } else {
        items.push('Άγνωστος κωδικός — δεν βρέθηκε στο fault bitmap.');
      }
      if (g.fault_type_label) {
        items.push('Κατάσταση panel: ' + g.fault_type_label + ' (fault_type=' + (g.fault_type ?? '—') + ')');
      }
      sections.push({
        title: primary ? ('Σφάλμα #' + code + ' — ' + primary.name) : ('Σφάλμα #' + code),
        items,
        level: respLevel(primary && primary.resp, g.fault_type)
      });
    }

    if (g.nfpa_fault) {
      const d = decodeNfpaFault(g.nfpa_fault);
      if (d.nfpa.length) {
        sections.push({
          title: 'NFPA 110 — register 40016 (0x' + d.wordNfpa.toString(16).toUpperCase() + ')',
          items: d.nfpa,
          level: 'warn'
        });
      }
      if (d.ext.length) {
        sections.push({
          title: 'Extended alarms — register 40017 (0x' + d.wordExt.toString(16).toUpperCase() + ')',
          items: d.ext,
          level: 'warn'
        });
      }
      if (!d.nfpa.length && !d.ext.length) {
        sections.push({
          title: 'NFPA / Extended raw',
          items: ['0x' + d.raw.toString(16).toUpperCase()],
          level: 'warn'
        });
      }
    }

    return sections;
  }

  function renderAlarmHtml(sections) {
    if (!sections || !sections.length) {
      return '<div class="small muted">Κανένας ενεργός συναγερμός.</div>';
    }
    return sections.map(s => {
      const cls = s.level === 'bad' ? 'bad' : (s.level === 'warn' ? 'warn' : '');
      const lis = (s.items || []).map(it => '<li>' + it + '</li>').join('');
      return '<div class="alarm-group ' + cls + '"><div class="alarm-title">' + s.title +
        '</div><ul>' + lis + '</ul></div>';
    }).join('');
  }

  function formatActiveFaultShort(g) {
    const code = Number(g && g.active_fault);
    if (!code) return 'Κανένα';
    const primary = lookupFaultCode(code)[0];
    let name = primary ? primary.name : 'Άγνωστο';
    if (name.length > 42) name = name.slice(0, 40) + '…';
    const ft = g.fault_type_label ? ' · ' + g.fault_type_label : '';
    return '#' + code + ft + '\n' + name;
  }

  function nfpaListItems(g) {
    if (!g || !g.nfpa_fault) return [];
    const d = decodeNfpaFault(g.nfpa_fault);
    const out = [];
    d.nfpa.forEach(x => out.push('NFPA: ' + x));
    d.ext.forEach(x => out.push('EXT: ' + x));
    return out;
  }

  function overviewFaultHint(g) {
    const code = Number(g && g.active_fault);
    if (!code) return null;
    const primary = lookupFaultCode(code)[0];
    if (!primary) return 'Σφάλμα γεννήτριας #' + code;
    let n = primary.name;
    if (n.length > 48) n = n.slice(0, 46) + '…';
    return '⚠ #' + code + ' — ' + n;
  }

  global.GensetDecode = {
    setFaultDb,
    lookupFaultCode,
    decodeNfpaFault,
    buildGensetAlarmSections,
    renderAlarmHtml,
    formatActiveFaultShort,
    nfpaListItems,
    overviewFaultHint,
    RESP_EL
  };
})(typeof window !== 'undefined' ? window : global);
