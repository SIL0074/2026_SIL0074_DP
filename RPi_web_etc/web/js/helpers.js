/* ─── helpers.js ─────────────────────────────────────────────────── */
/* Čisté pomocné funkce bez vedlejších efektů                        */

/**
 * Dekóduje hex-řetězec do pole float32 (Little Endian)
 */
export function hexToFloats(hex) {
  try {
    if (!hex || hex.length < 8) return [];
    const bytes = hex.match(/.{2}/g).map(h => parseInt(h, 16));
    const view  = new DataView(new Uint8Array(bytes).buffer);
    const out   = [];
    for (let i = 0; i + 4 <= bytes.length; i += 4) {
      out.push(view.getFloat32(i, true));
    }
    return out;
  } catch (e) { return []; }
}

export function isCharging(node) {
  if (!node) return false;
  return node.charging === true || node.charging === 1 || node.charging === "1";
}

/**
 * Metadata a jednotky pro různé stype
 */
export function stypeInfo(n) {
  return {
    1: { name: 'ENV',    cls: 'env',   labels: ['Teplota', 'Vlhkost'], units: ['°C', '%RH'], sparkIdx: 0 },
    2: { name: 'MOTION', cls: 'other', labels: ['Pohyb'],           units: [''],     sparkIdx: 0 },
    4: { name: 'DOOR',   cls: 'other', labels: ['Stav'],            units: [''],     sparkIdx: 0 },
    8: { name: 'BME',    cls: 'multi', labels: ['Teplota', 'Vlhkost', 'Tlak', 'Plyn'], units: ['°C', '%RH', 'hPa', 'kΩ'], sparkIdx: 0 }
  }[n] || { name: `T${n}`, cls: 'other', labels: [], units: [], sparkIdx: 0 };
}

export function rssiColor(r) {
  if (!r && r !== 0) return 'var(--muted)';
  return r >= -60 ? 'var(--green)' : r >= -75 ? 'var(--amber)' : 'var(--red)';
}

export function rssiPct(r) {
  return Math.max(0, Math.min(100, ((r + 100) / 60) * 100));
}

export function battColor(v) {
  if (!v) return 'var(--muted)';
  return v >= 3.7 ? 'var(--green)' : v >= 3.6 ? 'var(--amber)' : 'var(--red)';
}

export function battPct(v) {
  return Math.max(0, Math.min(100, ((v - 3.0) / 1.2) * 100));
}

export function timeAgo(ms) {
  if (!ms) return 'nikdy';
  const s = Math.round((Date.now() - ms) / 1000);
  if (s < 2)    return 'právě teď';
  if (s < 60)   return `${s}s`;
  if (s < 3600) return `${Math.round(s / 60)} min`;
  if (s < 86400) return `${Math.round(s / 3600)} hod`;
  return `${Math.round(s / 86400)} dní`;
}

export function formatSeconds(s) {
  if (s < 60) return `${Math.floor(s)}s`;
  const m = Math.floor(s / 60) % 60;
  const h = Math.floor(s / 3600) % 24;
  const d = Math.floor(s / 86400);
  let res = '';
  if (d) res += d + 'd ';
  if (h) res += h + 'h ';
  if (m || (!d && !h)) res += m + 'min';
  return res.trim();
}

export function fmtTime(ms) {
  if (!ms) return '–';
  const date = new Date(ms);
  const now  = new Date();
  const isToday = date.toDateString() === now.toDateString();
  if (isToday) {
    return date.toLocaleTimeString('cs-CZ', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
  }
  return date.toLocaleDateString('cs-CZ', { day: 'numeric', month: 'numeric' }) + ' ' + 
         date.toLocaleTimeString('cs-CZ', { hour: '2-digit', minute: '2-digit' });
}

export function rssiSignal(r) {
  if (!r && r !== 0) return 0;
  return r >= -55 ? 4 : r >= -65 ? 3 : r >= -75 ? 2 : r >= -85 ? 1 : 0;
}

export function rssiBarsSVG(rssi) {
  const bars  = rssiSignal(rssi);
  const color = rssi >= -60 ? 'green' : rssi >= -75 ? 'amber' : 'red';
  const heights = [5, 8, 11, 14];
  return `<span class="sig-bars">
    ${heights.map((h, i) => `<span class="sig-bar${i < bars ? ' lit-' + color : ''}" style="height:${h}px"></span>`).join('')}
  </span>`;
}

export function formatGwValue(key, val, fmt) {
  if (fmt === 'uptime') return isNaN(parseFloat(val)) ? String(val) : formatSeconds(parseFloat(val));
  if (fmt === 'heap')   return isNaN(parseFloat(val)) ? String(val) : Math.round(parseFloat(val) / 1024) + ' kB';
  if (fmt === 'bool')   return val ? '<span style="color:var(--green);font-weight:700;">● ON</span>' : '<span style="color:var(--red);font-weight:700;">● OFF</span>';
  if (fmt === 'num')    { const n = parseFloat(val); return isNaN(n) ? String(val) : (Number.isInteger(n) ? n : n.toFixed(2)); }
  if (fmt === 'pub_interval') return val + ' s';
  return String(val ?? '—');
}

export function skeletonVal(width = 60) {
  return `<span class="skeleton" style="width:${width}px;height:0.9em;vertical-align:middle;">&nbsp;</span>`;
}

export function getGasInfo(kohm) {
  if (kohm > 150) return { label: 'Výborný', color: 'var(--green)' };
  if (kohm > 50)  return { label: 'Dobrý',   color: 'var(--blue)' };
  if (kohm > 10)  return { label: 'Zhoršený', color: 'var(--amber)' };
  return { label: 'Špatný', color: 'var(--red)' };
}

export function formatRaw(obj) {
  return Object.keys(obj)
    .filter(k => !k.startsWith('_') && !['host', 'topic', 'stype', 'type'].includes(k) && obj[k] !== '' && obj[k] !== undefined)
    .map(k => {
      const v = obj[k];
      const cls = typeof v === 'string' ? 'rs' : 'rn';
      return `<span class="rk">"${k}":</span> <span class="${cls}">${JSON.stringify(v)}</span>,`;
    }).join('<br>');
}

/**
 * InfluxDB CSV parser
 */
export function parseInfluxCSV(csv) {
  const lines = csv.split('\n').map(l => l.trim()).filter(l => l.length > 0);
  if (lines.length < 2) return [];
  const data = [];
  let headers = [];
  for (let line of lines) {
    if (line.startsWith('#')) continue;
    const parts = line.split(',').map(p => p.replace(/^"|"$/g, '').trim());
    
    // Nové záhlaví tabulky
    if (parts.includes('result') && parts.includes('table')) {
      headers = parts;
      continue;
    }
    
    if (headers.length > 0 && parts.length >= headers.length) {
      const obj = {};
      headers.forEach((h, idx) => {
        if (!h || ['', 'result', 'table'].includes(h)) return;
        const raw = parts[idx];
        if (raw === '') return;
        
        // Robustnější detekce čísla: pouze pokud celý řetězec odpovídá číslu
        const isNum = /^-?\d+(\.\d+)?$/.test(raw);
        obj[h] = isNum ? parseFloat(raw) : raw;
      });
      // Akceptuje řádek, pokud má aspoň nějaká relevantní data
      if (obj.id || obj._time || obj.user || obj._field || obj.measurement || obj._measurement || obj._value !== undefined) {
        data.push(obj);
      }
    }
  }
  return data;
}

export function getSparkKey(stype) { return stype === 8 ? 2 : 0; }

/** 
 * SHA-256 implementace (včetně fallbacku pro non-secure context)
 */
export async function sha256(message) {
  if (crypto.subtle) {
    const msgUint8   = new TextEncoder().encode(message);
    const hashBuffer = await crypto.subtle.digest('SHA-256', msgUint8);
    return Array.from(new Uint8Array(hashBuffer)).map(b => b.toString(16).padStart(2, '0')).join('');
  }
  
  // Fallback pro HTTP (např. IP adresa bez SSL)
  console.warn("Kryptografie probíhá v softwarovém režimu (non-secure context).");
  
  // Velmi stručná JS implementace SHA-256
  function rotr(n, s) { return (n >>> s) | (n << (32 - s)); }
  function sha256_sw(ascii) {
    const mathPow = Math.pow;
    const maxWord = mathPow(2, 32);
    const result = [];
    const words = [];
    const asciiLength = ascii.length;
    const hash = [], k = [];
    let primeCounter = 0;
    const isPrime = num => {
      for (let i = 2; i <= Math.sqrt(num); i++) if (num % i === 0) return false;
      return true;
    };
    while (primeCounter < 64) {
      if (isPrime(++primeCounter + 1)) {
        if (hash.length < 8) hash.push((mathPow(primeCounter + 1, 1/2) * maxWord) | 0);
        k.push((mathPow(primeCounter + 1, 1/3) * maxWord) | 0);
      }
    }
    ascii += '\x80';
    while (ascii.length % 64 - 56) ascii += '\x00';
    for (let i = 0; i < ascii.length; i++) {
      const j = ascii.charCodeAt(i);
      if (j >> 8) return; 
      words[i >> 2] |= j << ((3 - i) % 4 << 3);
    }
    words[words.length] = ((asciiLength * 8) / maxWord) | 0;
    words[words.length] = (asciiLength * 8) | 0;
    for (let j = 0; j < words.length; j += 16) {
      const w = words.slice(j, j + 16);
      const oldHash = hash.slice(0);
      for (let i = 0; i < 64; i++) {
        const w15 = w[i - 15], w2 = w[i - 2];
        const s0 = rotr(w15, 7) ^ rotr(w15, 18) ^ (w15 >>> 3);
        const s1 = rotr(w2, 17) ^ rotr(w2, 19) ^ (w2 >>> 10);
        const ch = (hash[4] & hash[5]) ^ (~hash[4] & hash[6]);
        const maj = (hash[0] & hash[1]) ^ (hash[0] & hash[2]) ^ (hash[1] & hash[2]);
        const temp1 = hash[7] + (rotr(hash[4], 6) ^ rotr(hash[4], 11) ^ rotr(hash[4], 25)) + ch + k[i] + (w[i] = (i < 16) ? w[i] : (w[i - 16] + s0 + w[i - 7] + s1) | 0);
        const temp2 = (rotr(hash[0], 2) ^ rotr(hash[0], 13) ^ rotr(hash[0], 22)) + maj;
        hash.splice(0, 8, (temp1 + temp2) | 0, hash[0], hash[1], hash[2], (hash[3] + temp1) | 0, hash[4], hash[5], hash[6]);
      }
      for (let i = 0; i < 8; i++) hash[i] = (hash[i] + oldHash[i]) | 0;
    }
    for (let i = 0; i < 8; i++) {
      for (let j = 3; j + 1; j--) {
        const b = (hash[i] >> (j * 8)) & 255;
        result.push((b < 16 ? '0' : '') + b.toString(16));
      }
    }
    return result.join('');
  }
  return sha256_sw(message);
}
