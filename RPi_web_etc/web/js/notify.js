/* ─── notify.js ──────────────────────────────────────────────────── */
/* Toast notifikace + sledování stavu uzlů                          */

import { sensors, routers } from './state.js';
import { isNodeOffline } from './sidebar.js';

const nodeStatusCache = {};
let toastId = 0;
const appStartTime = Date.now();

const ICONS = {
  warn: `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>`,
  err:  `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><circle cx="12" cy="12" r="10"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/></svg>`,
  ok:   `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"/><polyline points="22 4 12 14.01 9 11.01"/></svg>`,
  info: `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>`,
};

/**
 * Zobrazí toast notifikaci.
 * @param {string} title - Titulek
 * @param {string} msg   - Zpráva
 * @param {'ok'|'warn'|'err'|'info'} type
 * @param {number} duration - ms (0 = trvalý)
 */
export function showToast(title, msg = '', type = 'info', duration = 5000) {
  const container = document.getElementById('toastContainer');
  if (!container) return;

  const id = ++toastId;
  const el = document.createElement('div');
  el.className = 'toast';
  el.id = 'toast_' + id;
  el.innerHTML = `
    <div class="toast-icon ${type}">${ICONS[type] || ICONS.info}</div>
    <div class="toast-body">
      <div class="toast-title">${title}</div>
      ${msg ? `<div class="toast-msg">${msg}</div>` : ''}
    </div>
    <button class="toast-close" onclick="window._removeToast(${id})">
      <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>
    </button>`;

  container.appendChild(el);

  if (duration > 0) {
    setTimeout(() => removeToast(id), duration);
  }
  return id;
}

function removeToast(id) {
  const el = document.getElementById('toast_' + id);
  if (!el) return;
  el.classList.add('removing');
  setTimeout(() => el.remove(), 300);
}

window._removeToast = removeToast;

/**
 * Sleduje změny stavu uzlů a zobrazuje notifikace.
 * Voláno každých ~5s z main.js ticku.
 */
export function checkNodeStatusChanges() {
  const allNodes = [
    ...Object.entries(sensors).map(([id, n]) => ({ id, node: n, type: 'sensor' })),
    ...Object.entries(routers).map(([id, n]) => ({ id, node: n, type: 'router' })),
  ];

  for (const { id, node, type } of allNodes) {
    const nowOffline = isNodeOffline(node);
    const wasOffline = nodeStatusCache[id];

    if (wasOffline === undefined) {
      // První setkání – uloží stav bez notifikace
      nodeStatusCache[id] = nowOffline;
      continue;
    }

    if (!wasOffline && nowOffline) {
      // Uzel právě odpadl
      import('./i18n.js').then(({ t }) => {
        showToast(t('node_offline_title'), id, 'warn', 8000);
      });
      nodeStatusCache[id] = true;
    } else if (wasOffline && !nowOffline) {
      // Uzel se stal znovu online
      import('./i18n.js').then(({ t }) => {
        showToast(t('node_online_title'), id, 'ok', 4000);
      });
      nodeStatusCache[id] = false;
    }

    // Počkáme 15s od startu aplikace, aby se stihly načíst stavy a neobtěžovalo to hned po refreshi
    if (type === 'sensor' && !nowOffline && node.batt !== undefined && node.batt < 3.5) {
      if (Date.now() - appStartTime < 15000) continue; 

      const warnKey = 'batt_' + id;
      if (!nodeStatusCache[warnKey]) {
        import('./i18n.js').then(({ t }) => {
          showToast(t('batt_warn_title'), `${id} – ${node.batt?.toFixed(2)}V, ${t('batt_warn_msg')}`, 'warn', 10000);
        });
        nodeStatusCache[warnKey] = true;
        // reset po hodině
        setTimeout(() => { delete nodeStatusCache[warnKey]; }, 3600000);
      }
    }
  }
}
