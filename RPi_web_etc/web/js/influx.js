/* ─── influx.js ──────────────────────────────────────────────────── */
/* Veškerá komunikace s InfluxDB                                     */

import { parseInfluxCSV, getSparkKey, formatSeconds, hexToFloats } from './helpers.js';
import {
  sensors, routers, gateway, setGateway, history,
  globalRangeMin, setHistoryLoading,
  currentUser, setCurrentUserIp, selectedId
} from './state.js';
import { renderSensorSidebar, renderRouterSidebar, renderGwSidebar } from './sidebar.js';
import { addLog } from './log.js';

const ORG    = 'my-org';
const BUCKET = 'my-bucket';
const TOKEN  = 'my-super-secret-token';

const BASE_HEADERS = {
  'Authorization': `Token ${TOKEN}`,
  'Content-Type':  'application/json',
  'Accept':        'application/csv',
};

async function runQuery(query) {
  const res = await fetch(`/api/v2/query?org=${ORG}`, {
    method:  'POST',
    headers: BASE_HEADERS,
    body:    JSON.stringify({ query, dialect: { header: true } }),
  });
  if (!res.ok) throw new Error(`HTTP ${res.status}`);
  return res.text();
}

async function writePoint(line) {
  const res = await fetch(`/api/v2/write?org=${ORG}&bucket=${BUCKET}&precision=s`, {
    method:  'POST',
    headers: { 'Authorization': `Token ${TOKEN}`, 'Content-Type': 'text/plain; charset=utf-8' },
    body:    line,
  });
  if (!res.ok) {
    const txt = await res.text();
    throw new Error(`InfluxDB Write Error: ${res.status} ${txt}`);
  }
}

export async function fetchHistoricalNodes() {
  addLog('Stahuji seznam historických uzlů...', 'warn');
  const query = `from(bucket:"${BUCKET}")|>range(start:-30d)|>filter(fn:(r)=>r._measurement=="sensors" or r._measurement=="routers" or r._measurement=="gateway_info")|>last()|>map(fn:(r)=>({r with id:if exists r.id then r.id else (if r._field=="id" then string(v:r._value) else "GATEWAY")}))|>filter(fn:(r)=>r._field!="id")|>pivot(rowKey:["id","_time"],columnKey:["_field"],valueColumn:"_value")|>group()`;
  
  try {
    const csv = await runQuery(query);
    const rows = parseInfluxCSV(csv);

    if (rows.length === 0) {
      addLog('V InfluxDB nenalezena žádná historická data.', 'warn');
      return;
    }

    rows.forEach(r => {
      const id = r.id;
      if (!id) return;
      const ts = r._time ? new Date(r._time).getTime() : 0;
      const type = r.type || (r._measurement === 'routers' ? 'ROUTER' : r._measurement === 'sensors' ? 'SENSOR' : '');

      // Mapa polí
      const received = {};
      Object.keys(r).forEach(k => {
        if (!k.startsWith('_') && !['result','table','id','type','stype','host','topic'].includes(k) && r[k] !== undefined && r[k] !== '') {
          received[k] = true;
        }
      });

      if (id === 'GATEWAY' || r._measurement === 'gateway_info') {
        setGateway({ 
          id: 'GATEWAY', 
          ...r,
          _ts: Math.max(gateway?._ts || 0, ts), 
          _received: { ...gateway?._received, ...received } 
        });
      } else if (type === 'ROUTER') {
        routers[id] = { ...routers[id], ...r, id, type: 'ROUTER', _ts: Math.max(routers[id]?._ts || 0, ts), _received: received };
      } else {
        const s = { ...sensors[id], ...r, id, stype: parseInt(r.stype) || 1, type: 'SENSOR', _ts: Math.max(sensors[id]?._ts || 0, ts), _received: received };
        if (s.data && (!s._floats || s._floats.length === 0)) {
          s._floats = hexToFloats(s.data);
        }
        sensors[id] = s;
      }
    });

    const uniqueIds = new Set(rows.map(r => r.id).filter(id => id && id !== 'GATEWAY'));
    addLog(`Načteno ${uniqueIds.size} unikátních uzlů z historie.`, 'ok');
    setHistoryLoading(false);
    renderSensorSidebar(); renderRouterSidebar(); renderGwSidebar();
    if (window.App) window.App.showDetail('GATEWAY', 'gateway');
    fetchAllSparklines();
    fetchGwLinkHistory();
  } catch (e) {
    addLog('Chyba InfluxDB historie: ' + e.message, 'err');
    setHistoryLoading(false);
  }
}

export async function fetchAllSparklines() {
  const range = globalRangeMin > 0 ? `-${globalRangeMin}m` : '-30d';
  let window = '2m'; if (globalRangeMin >= 10080) window = '1h'; else if (globalRangeMin >= 1440) window = '10m'; else if (globalRangeMin >= 360) window = '5m';
  Object.keys(history).forEach(id => { const k = getSparkKey(sensors[id]?.stype || 1); if (history[id][k]) history[id][k] = []; });
  const query = `from(bucket:"${BUCKET}")|>range(start:${range})|>filter(fn:(r)=>r._measurement=="sensors" and (r._field=="temperature_c" or r._field=="temperature" or r._field=="temp" or r._field=="humidity_rh" or r._field=="humidity" or r._field=="pressure_hpa" or r._field=="gas_kohm"))|>aggregateWindow(every:${window},fn:mean,createEmpty:false)|>group(columns:["id","_field"])`;
  try {
    const csv = await runQuery(query);
    const rows = parseInfluxCSV(csv);
    const fMap = { temperature_c: 0, temperature: 0, temp: 0, humidity_rh: 1, humidity: 1, pressure_hpa: 2, gas_kohm: 3 };
    rows.forEach(r => {
      const id = r.id; if (!id || !history[id]) return;
      const ts = new Date(r._time).getTime(), val = parseFloat(r._value), s = sensors[id], sk = getSparkKey(s?.stype || 1);
      const fn = Object.keys(r).find(k => fMap[k] !== undefined && r[k] !== undefined);
      const idx = fn ? fMap[fn] : 0;
      if (idx === sk) { if (!history[id][sk]) history[id][sk] = []; history[id][sk].push({ t: ts, v: val }); }
    });
    Object.keys(history).forEach(id => { const k = getSparkKey(sensors[id]?.stype || 1); if (history[id][k]) history[id][k].sort((a, b) => a.t - b.t); });
    renderSensorSidebar();
  } catch (e) { console.error('Sparklines error:', e); }
}

/** Načte historii stavu linku (WiFi/NB-IoT) pro bránu */
export async function fetchGwLinkHistory() {
  const query = `from(bucket:"${BUCKET}")|>range(start:-24h)|>filter(fn:(r)=>r._measurement=="gateway_info" and r._field=="link")|>sort(columns:["_time"])`;
  try {
    const csv = await runQuery(query);
    const rows = parseInfluxCSV(csv);
    if (!history['GATEWAY']) history['GATEWAY'] = {};
    history['GATEWAY']['link'] = rows.map(r => ({
      t: new Date(r._time).getTime(),
      v: (String(r._value).toLowerCase().includes('wifi') || r._value === 0 || r._value === '0') ? 1 : 0
    }));
    if (window.App && (selectedId === 'GATEWAY' || selectedId === 'SYSTEM')) {
      import('./charts.js').then(m => m.updateGwChart());
    }
  } catch (e) { console.error('fetchGwLinkHistory error:', e); }
}

export async function loadHistory(id, rangeMin, specificField = null) {
  const range = rangeMin > 0 ? `-${rangeMin}m` : '-2y';
  let agg = '';
  if (rangeMin === 0)         agg = '|> aggregateWindow(every:10m, fn:mean, createEmpty:false)';
  else if (rangeMin >= 43200) agg = '|> aggregateWindow(every:1h,  fn:mean, createEmpty:false)';
  else if (rangeMin >= 1440)  agg = '|> aggregateWindow(every:10m, fn:mean, createEmpty:false)';
  else if (rangeMin >= 360)   agg = '|> aggregateWindow(every:1m,  fn:mean, createEmpty:false)';

  const fieldMap = {
    0: ['temperature_c', 'temperature', 'temp'],
    1: ['humidity_rh', 'humidity', 'hum'],
    2: ['pressure_hpa', 'pressure'],
    3: ['gas_kohm', 'gas'],
    rssi: ['rssi'], batt: ['batt'], active_us: ['active_us']
  };

  let fieldQuery = '';
  if (specificField !== null) {
    fieldQuery = (fieldMap[specificField] || []).map(n => `r._field=="${n}"`).join(' or ');
  } else {
    fieldQuery = 'r._field=="temperature_c" or r._field=="temperature" or r._field=="temp" or r._field=="humidity_rh" or r._field=="humidity" or r._field=="hum" or r._field=="pressure_hpa" or r._field=="pressure" or r._field=="gas_kohm" or r._field=="gas" or r._field=="rssi" or r._field=="batt" or r._field=="active_us"';
  }

  const query = `from(bucket:"${BUCKET}")|>range(start:${range})|>filter(fn:(r)=>r._measurement=="sensors" and r.id=="${id}" and (${fieldQuery}))${agg}|>pivot(rowKey:["_time"],columnKey:["_field"],valueColumn:"_value")|>sort(columns:["_time"])`;
  try {
    const csv = await runQuery(query);
    const rows = parseInfluxCSV(csv);
    if (rows.length === 0 && rangeMin !== 0) return loadHistory(id, 0, specificField);
    if (!history[id]) history[id] = {};
    const revMap = {}; Object.keys(fieldMap).forEach(k => fieldMap[k].forEach(n => revMap[n] = k));
    const merged = {};
    rows.forEach(r => {
      const ts = new Date(r._time).getTime();
      Object.keys(revMap).forEach(f => {
        if (r[f] !== undefined && r[f] !== '' && !isNaN(r[f])) {
          const k = revMap[f]; if (!merged[k]) merged[k] = [];
          let v = parseFloat(r[f]); if (f === 'active_us') v /= 1000;
          merged[k].push({ t: ts, v: v });
        }
      });
    });
    Object.keys(merged).forEach(k => {
      const hist = merged[k].sort((a, b) => a.t - b.t), live = history[id][k] || [], lastT = hist.length ? hist[hist.length - 1].t : 0;
      history[id][k] = [...hist, ...live.filter(p => p.t > lastT)];
    });
    renderSensorSidebar(); 
    if (window.App) window.App.refreshSensorDetail(id);
  } catch (e) { console.error('loadHistory error:', e); }
}

export async function logVisit() {
  if (isLoggingVisit) return; isLoggingVisit = true;
  try {
    let ip = 'local-ip';
    try {
      const ipRes = await fetch('https://api.ipify.org?format=json', { mode: 'cors' });
      const data = await ipRes.json();
      ip = data.ip;
    } catch (e) {
      console.warn("CORS/Tracking blocked ipify.org, using fallback info.");
      ip = window.location.hostname || 'unknown';
    }
    setCurrentUserIp(ip);
    await writePoint(`web_visits,ip=${ip},role=${currentUser} value=1`);
    if (currentUser === 'admin') fetchVisits();
  } catch (e) { } finally { isLoggingVisit = false; }
}

export async function fetchVisits() {
  try { const csv = await runQuery(`from(bucket:"${BUCKET}")|>range(start:-30d)|>filter(fn:(r)=>r._measurement=="web_visits")|>group(columns:["ip"])|>last()|>group()|>sort(columns:["_time"],desc:true)|>limit(n:15)`); return parseInfluxCSV(csv); } catch (e) { return []; }
}

export async function fetchIpHistory(ip) {
  const filter = ip ? `and r.ip == "${ip}"` : '';
  try { const csv = await runQuery(`from(bucket:"${BUCKET}")|>range(start:-30d)|>filter(fn:(r)=>r._measurement=="web_visits" ${filter})|>sort(columns:["_time"],desc:true)|>limit(n:50)`); return parseInfluxCSV(csv); } catch (e) { return []; }
}

/** Načte globální konfiguraci uzlů (jména a skupiny) */
export async function fetchNodeConfigs() {
  const query = `from(bucket:"${BUCKET}") |> range(start: -30d) |> filter(fn:(r) => r._measurement == "node_config") |> last() |> pivot(rowKey:["id"], columnKey: ["_field"], valueColumn: "_value")`;
  try {
    const csv = await runQuery(query);
    const rows = parseInfluxCSV(csv);
    const configs = {};
    rows.forEach(r => { 
      if (r.id) configs[r.id] = { alias: r.alias || '', group: r.group || '' }; 
    });
    return configs;
  } catch (e) { return {}; }
}

/** Uloží konfiguraci uzlu globálně */
export async function saveNodeConfigGlobal(id, alias, group) {
  try {
    const line1 = `node_config,id=${id} alias="${alias}"`;
    const line2 = `node_config,id=${id} group="${group}"`;
    await writePoint(line1);
    await writePoint(line2);
    return true;
  } catch (e) { return false; }
}

/** Načte seznam uživatelů z InfluxDB */
export async function fetchTotalStats() {
  // Půlnoc UTC
  const d = new Date();
  d.setUTCHours(0,0,0,0);
  const startOfDay = d.toISOString();

  // Celkem: Sečte počty unikátních zpráv (podle rssi) přes všechny série za poslední rok
  const qTotal = `from(bucket:"${BUCKET}") 
    |> range(start: -1y) 
    |> filter(fn: (r) => (r._measurement == "sensors" or r._measurement == "routers") and r._field == "rssi") 
    |> count() 
    |> group() 
    |> sum()`;

  // Dnes: To samé, ale od dnešní půlnoci UTC
  const qToday = `from(bucket:"${BUCKET}") 
    |> range(start: ${startOfDay}) 
    |> filter(fn: (r) => (r._measurement == "sensors" or r._measurement == "routers") and r._field == "rssi") 
    |> count() 
    |> group() 
    |> sum()`;

  try {
    const [totalRes, todayRes] = await Promise.all([
      runQuery(qTotal),
      runQuery(qToday)
    ]);

    const parseCount = (csv, label) => {
      const rows = parseInfluxCSV(csv);
      if (rows.length === 0) return 0;
      return parseInt(rows[0]._value) || 0;
    };
    const total = parseCount(totalRes, "TOTAL");
    const today = parseCount(todayRes, "TODAY");

    return { total, today };
  } catch (e) {
    console.error("fetchTotalStats error", e);
    return { total: 0, today: 0 };
  }
}

/** Načte seznam uživatelů z InfluxDB */
export async function fetchUsers() {
  const query = `from(bucket:"${BUCKET}") |> range(start: -1y) |> filter(fn:(r) => r._measurement == "web_users") |> last() |> pivot(rowKey:["user"], columnKey: ["_field"], valueColumn: "_value")`;
  try {
    const csv = await runQuery(query);
    return parseInfluxCSV(csv);
  } catch (e) {
    console.error("fetchUsers error:", e);
    return [];
  }
}

/** Uloží uživatele globálně */
export async function saveUserGlobal(user, hash, role) {
  try {
    // Escapování mezer v tagu 'user' pro Line Protocol
    const escapedUser = user.replace(/ /g, '\\ ').replace(/,/g, '\\,');
    const line = `web_users,user=${escapedUser} hash="${hash}",role="${role}"`;
    await writePoint(line);
    return true;
  } catch (e) {
    console.error("saveUserGlobal error:", e);
    return false;
  }
}

/** Načte globální statistiky zpráv (celkem a za poslední minutu) */
export async function fetchGlobalStats() {
  // Spočítá unikátní zprávy (podle času a ID) za poslední rok a minutu
  const query = `
    total = from(bucket:"${BUCKET}") |> range(start: -1y) |> filter(fn:(r) => r._measurement == "sensors" or r._measurement == "routers") |> group() |> count() |> yield(name: "total")
    lastMin = from(bucket:"${BUCKET}") |> range(start: -1m) |> filter(fn:(r) => r._measurement == "sensors" or r._measurement == "routers") |> group() |> count() |> yield(name: "lastMin")
  `;
  try {
    const csv = await runQuery(query);
    const rows = parseInfluxCSV(csv);
    const total = rows.find(r => r.result === 'total')?._value || 0;
    const lastMin = rows.find(r => r.result === 'lastMin')?._value || 0;
    return { total, lastMin };
  } catch (e) { 
    console.error("fetchGlobalStats error:", e);
    return { total: 0, lastMin: 0 }; 
  }
}

/** Smaže uživatele z InfluxDB */
export async function deleteUserGlobal(user) {
  try {
    const now = new Date().toISOString();
    const body = {
      start: "2020-01-01T00:00:00Z",
      stop: now,
      predicate: `_measurement="web_users" AND user="${user}"`
    };
    const res = await fetch(`/api/v2/delete?org=${ORG}&bucket=${BUCKET}`, {
      method: 'POST',
      headers: { 
        'Authorization': `Token ${TOKEN}`,
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(body)
    });
    if (!res.ok) {
      const txt = await res.text();
      throw new Error(`Delete error: ${txt}`);
    }
    return true;
  } catch (e) {
    console.error("deleteUserGlobal error:", e);
    return false;
  }
}

let isLoggingVisit = false;
