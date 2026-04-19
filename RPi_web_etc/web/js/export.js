/* ─── export.js ──────────────────────────────────────────────────── */
/* Export dat senzorů jako CSV nebo JSON                             */

import { history, sensors, routers, gateway } from './state.js';
import { stypeInfo } from './helpers.js';

/**
 * Exportuje historii senzoru jako CSV.
 * @param {string} nodeId
 */
export function exportNodeCSV(nodeId) {
  const h = history[nodeId];
  if (!h || Object.keys(h).length === 0) {
    alert('Žádná historická data k exportu.');
    return;
  }

  const s    = sensors[nodeId];
  const info = s ? stypeInfo(s.stype) : { labels: [], units: [] };

  const allTs = new Set();
  Object.values(h).forEach(arr => arr.forEach(p => allTs.add(p.t)));
  const sortedTs = [...allTs].sort((a, b) => a - b);

  const keys = Object.keys(h);
  const header = ['timestamp', 'datetime', ...keys].join(',');

  const rows = sortedTs.map(t => {
    const dt = new Date(t).toISOString();
    const vals = keys.map(k => {
      const pt = (h[k] || []).find(p => p.t === t);
      return pt !== undefined ? pt.v : '';
    });
    return [t, dt, ...vals].join(',');
  });

  downloadText(`${nodeId}_history.csv`, [header, ...rows].join('\n'));
}

/**
 * Exportuje historii senzoru jako JSON.
 * @param {string} nodeId
 */
export function exportNodeJSON(nodeId) {
  const h = history[nodeId];
  const node = sensors[nodeId] || routers[nodeId];
  if (!node) { alert('Uzel nenalezen.'); return; }

  const payload = {
    nodeId,
    exportedAt: new Date().toISOString(),
    node: { ...node },
    history: h || {},
  };
  delete payload.node._ts; // interní pole

  downloadText(`${nodeId}_export.json`, JSON.stringify(payload, null, 2));
}

/**
 * Exportuje aktuální snapshot všech uzlů.
 */
export function exportAllJSON() {
  const payload = {
    exportedAt: new Date().toISOString(),
    gateway: { ...gateway },
    sensors: { ...sensors },
    routers: { ...routers },
  };
  downloadText(`espnow_snapshot_${Date.now()}.json`, JSON.stringify(payload, null, 2));
}

function downloadText(filename, text) {
  const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement('a');
  a.href     = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}
