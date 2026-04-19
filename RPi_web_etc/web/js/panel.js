/* ─── panel.js ───────────────────────────────────────────────────── */
/* Renderování hlavního panelu (detail senzoru, routeru, GW, přehled) */

import {
  sensors, routers, gateway,
  selectedId, selectedType, setSelected,
  activeTab, setActiveTab,
  currentUser, getNodeName, getNodeGroup,
  charts, history,
  globalRangeMin, syncCharts, chartRanges,
  GW_FIELDS, GW_SKIP_KEYS, COLORS,
  activeWidgetId, activeWidgetType, setActiveWidget, clearActiveWidget,
} from './state.js';
import {
  stypeInfo, rssiColor, rssiPct, rssiBarsSVG,
  battColor, battPct,
  timeAgo, fmtTime, formatSeconds, formatGwValue,
  skeletonVal, getGasInfo, formatRaw, hexToFloats, isCharging
} from './helpers.js';
import {
  sensorTileHasData, routerTileHasData, gwTileHasData,
  renderSensorSidebar, renderRouterSidebar, renderGwSidebar,
  isNodeOffline, getNodeThreshold
} from './sidebar.js';
import { destroyCharts, buildCharts, rangeButtons, showLargeChart } from './charts.js';
import { loadHistory } from './influx.js';
import { t } from './i18n.js';

/* ─── Navigace ───────────────────────────────────────────────────── */
export function showDetail(id, type) {
  setSelected(id, type);
  setActiveTab('dashboard');

  document.querySelectorAll('.node-item').forEach(el => el.classList.remove('active'));
  const ni = document.getElementById('ni_' + id);
  if (ni) ni.classList.add('active');

  destroyCharts();
  const panel = document.getElementById('panel');

  if (type === 'sensor') {
    const s = sensors[id];
    if (s) s._loaded = true;
    if (s && (!s._floats || s._floats.length === 0) && s.data) s._floats = hexToFloats(s.data);
    panel.innerHTML = buildSensorDetail(id);

    const isArchived = isNodeOffline(s);
    const range = isArchived ? 0 : globalRangeMin;
    setTimeout(() => { buildCharts(id); loadHistory(id, range); }, 50);

  } else if (type === 'router') {
    if (routers[id]) routers[id]._loaded = true;
    panel.innerHTML = buildRouterDetail(id);

  } else if (type === 'gateway') {
    panel.innerHTML = buildGatewayDetail();
    setTimeout(() => {
      import('./charts.js').then(m => m.updateGwChart());
    }, 50);
  } else if (type === 'system') {
    panel.innerHTML = buildSystemView();
    setTimeout(refreshSystemView, 50);
  }
}

export function switchTab(tab, el) {
  setActiveTab(tab);
  document.querySelectorAll('.ptab').forEach(t => t.classList.remove('active'));
  if (el) el.classList.add('active');
  const ds = document.getElementById('dashSection');
  const rs = document.getElementById('rawSection');
  if (ds) ds.style.display = tab === 'dashboard' ? 'grid' : 'none';
  if (rs) rs.style.display  = tab === 'raw'       ? 'block' : 'none';
}

/* ─── SENZOR DETAIL ──────────────────────────────────────────────── */
export function buildSensorDetail(id) {
  const s = sensors[id];
  if (!s) return '<div class="welcome"><div class="welcome-sub">Žádná data</div></div>';

  const info    = stypeInfo(s.stype);
  const hasRssi = sensorTileHasData(id, 'rssi', s.rssi);
  const hasBatt = sensorTileHasData(id, 'batt', s.batt);
  const hasAct  = sensorTileHasData(id, 'active_us', s.active_us);
  const hasData = sensorTileHasData(id, 'data', s.data);
  const rc      = rssiColor(s.rssi);
  const rp      = rssiPct(s.rssi);
  const bc      = battColor(s.batt);
  const bp      = battPct(s.batt);
  const isArchived = isNodeOffline(s);
  const isLost     = (Date.now() - s._ts) > (s.sleep_int ? s.sleep_int * 1200 : 60000); 

  const battWarn = (!isArchived && s.batt !== undefined && s.batt < 3.5)
    ? '<div class="detail-batt-warn">⚠️ Slabá baterie, nabijte prosím</div>'
    : '';

  const chg = isCharging(s);
  
  const floatCards = info.labels.map((lbl, i) => {
    const f    = (hasData && s._floats && s._floats[i] !== undefined) ? s._floats[i] : 0;
    const unit = info.units[i] || '';
    const prev = s._prev && s._prev[i];
    let dh = '';
    if (prev !== undefined && !isArchived) {
      const d = f - prev;
      if (Math.abs(d) > 0.01) dh = `<div class="fc-delta ${d > 0 ? 'up' : 'down'}">${d > 0 ? '▲' : '▼'} ${Math.abs(d).toFixed(2)}</div>`;
    }
    
    const isTempHum = lbl.toLowerCase().includes('teplota') || lbl.toLowerCase().includes('vlhkost') || lbl.toLowerCase().includes('temp') || lbl.toLowerCase().includes('hum');
    const inflictIcon = (chg && isTempHum) ? `<span class="chg-inflict-icon" title="${t('charging_warn')}">❗</span>` : '';

    let status = '';
    if (s.stype === 8 && i === 3) {
      const g = getGasInfo(f);
      status = `<div id="fcv_status_${id}_${i}" style="font-size:0.68rem;font-weight:600;color:${g.color};margin-top:2px;">${hasData ? g.label : ''}</div>`;
    }
    return `<div class="float-card fc${i}" id="fc_${id}_${i}">${dh}
      <div class="fc-label">${lbl}${inflictIcon}</div>
      <div class="fc-value"><span id="fcv_${id}_${i}">${hasData ? f.toFixed(2) : skeletonVal(50)}</span> <span class="fc-unit">${unit}</span></div>
      ${status}
    </div>`;
  }).join('');

  const chartSections = info.labels.map((lbl, i) => {
    const unit = info.units[i] || '';
    const rMin = syncCharts ? globalRangeMin : (chartRanges[i] ?? globalRangeMin);
    return `<div class="chart-card" onclick="App.showLargeChart('${i}','${lbl}')" style="cursor:pointer;position:relative;">
      <div class="chart-expand-btn"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/></svg></div>
      <div class="chart-topbar">
        <div class="chart-name"><div class="chart-dot" style="background:${COLORS[i % COLORS.length]}"></div>${lbl}${unit ? ' (' + unit + ')' : ''}</div>
        <div class="chart-btns" id="btns_${i}" onclick="event.stopPropagation()">${rangeButtons(rMin, i)}</div>
      </div>
      <div class="chart-wrap"><canvas id="chart_${i}"></canvas></div>
    </div>`;
  }).join('');

  return `<div class="panel-tabs">
    <div class="panel-tabs-left">
      <div class="ptab active" onclick="App.switchTab('dashboard',this)">${t('dashboard')}</div>
      <div class="ptab" onclick="App.switchTab('raw',this)">Raw JSON</div>
    </div>
    <div class="panel-tabs-right">
      <button class="export-btn" onclick="App.exportNodeCSV('${id}')">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
        CSV
      </button>
      <button class="export-btn" onclick="App.exportNodeJSON('${id}')">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
        JSON
      </button>
    </div>
  </div>
  <div class="detail" style="${isArchived ? 'opacity:0.88' : ''}">
    <div id="dashSection" style="display:grid;gap:18px;">
      ${battWarn}
      <div class="detail-header${isArchived ? ' archived' : ''}">
        <div class="detail-header-left">
          <div class="detail-node-icon sensor${isArchived ? ' archived' : ''}">
            <svg viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2"><path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5"/></svg>
          </div>
          <div>
            <div style="display:flex;align-items:baseline;gap:8px;">
              <div class="detail-id">${getNodeName(id)}</div>
              <div class="detail-id-sub">#${id}</div>
              <button class="rename-btn" onclick="App.showRenameModal('${id}','sensor')" title="${t('rename')}">
                <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>
              </button>
            </div>
            <div class="detail-badges" id="det_badges">
              <span id="det_status" class="badge ${isArchived ? 'badge-red' : (isLost ? 'badge-amber' : 'badge-green')}">${isArchived ? t('offline') : (isLost ? t('lost') : t('online'))}</span>
              <span class="badge badge-blue">${info.name}</span>
              ${getNodeGroup(id) ? `<span class="badge badge-indigo">${getNodeGroup(id)}</span>` : ''}
              ${!isArchived && (s.relay && s.relay !== '0000')
                ? `<span class="badge badge-amber">${s.hops} hop${s.hops > 1 ? 'ů' : ''}</span><span class="badge badge-indigo">Přes: ${s.relay}</span>`
                : (isArchived ? '' : `<span class="badge badge-green">${t('direct_conn')}</span>`)}
            </div>
          </div>
        </div>
        <div class="detail-meta">
          <span class="meta-time" id="det_tsago">${s._ts ? timeAgo(s._ts) : skeletonVal(60)}</span>
          <div class="meta-status">
            ${s._ts ? (isArchived
              ? '<div class="pill-dot" style="background:var(--red);width:6px;height:6px;"></div> OFFLINE'
              : '<div class="pill-dot live" style="background:var(--green);width:6px;height:6px;"></div> Live')
              : skeletonVal(40)}
          </div>
          <div>Poslední: ${s._ts ? fmtTime(s._ts) : skeletonVal(80)}</div>
        </div>
      </div>
      ${info.labels.length ? `<div><div class="section-label">${isArchived ? t('last_val') : t('current_val')}</div><div class="floats-grid" style="${isArchived ? 'opacity:0.8' : ''}">${floatCards}</div></div>` : ''}
      <div>
        <div class="section-label" style="justify-content:space-between;align-items:center;padding-right:4px;">
          <span>${t('history')}</span>
          <div class="sync-row" style="margin:0;">
            <span class="sync-label">${t('sync_label')}</span>
            <button id="syncToggleBtn" class="rng-btn ${syncCharts ? 'active' : ''}" onclick="App.toggleSync()">
              ${syncCharts ? t('sync_on') : t('sync_off')}
            </button>
          </div>
        </div>
        <div class="charts-grid">
          ${chartSections}
          <div class="chart-card" onclick="App.showLargeChart('rssi','${t('rssi')} (dBm)')" style="cursor:pointer;">
            <div class="chart-expand-btn"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/></svg></div>
            <div class="chart-topbar"><div class="chart-name"><div class="chart-dot" style="background:#ef4444"></div>${t('rssi')} (dBm)</div><div class="chart-btns" id="btns_rssi" onclick="event.stopPropagation()">${rangeButtons(syncCharts ? globalRangeMin : (chartRanges['rssi'] ?? globalRangeMin), 'rssi')}</div></div>
            <div class="chart-wrap"><canvas id="chart_rssi"></canvas></div>
          </div>
          <div class="chart-card" onclick="App.showLargeChart('active_us','${t('active_time')} (${t('ms')})')" style="cursor:pointer;">
            <div class="chart-expand-btn"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/></svg></div>
            <div class="chart-topbar"><div class="chart-name"><div class="chart-dot" style="background:#6366f1"></div>${t('active_time')} (${t('ms')})</div><div class="chart-btns" id="btns_active_us" onclick="event.stopPropagation()">${rangeButtons(syncCharts ? globalRangeMin : (chartRanges['active_us'] ?? globalRangeMin), 'active_us')}</div></div>
            <div class="chart-wrap"><canvas id="chart_active_us"></canvas></div>
          </div>
          <div class="chart-card" onclick="App.showLargeChart('batt','${t('battery')} (${t('volt')})')" style="cursor:pointer;">
            <div class="chart-expand-btn"><svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M15 3h6v6M9 21H3v-6M21 3l-7 7M3 21l7-7"/></svg></div>
            <div class="chart-topbar"><div class="chart-name"><div class="chart-dot" style="background:#f59e0b"></div>${t('battery')} (${t('volt')})</div><div class="chart-btns" id="btns_batt" onclick="event.stopPropagation()">${rangeButtons(syncCharts ? globalRangeMin : (chartRanges['batt'] ?? globalRangeMin), 'batt')}</div></div>
            <div class="chart-wrap"><canvas id="chart_batt"></canvas></div>
          </div>
        </div>
      </div>
      <div><div class="section-label">${isArchived ? t('sys_metrics') + ' (poslední)' : t('sys_metrics')}</div>
        <div class="tiles-row" style="${isArchived ? 'opacity:0.8' : ''}">
          <div class="tile">
            <div class="tile-lbl">Baterie</div>
            <div class="tile-val" style="color:${bc}" id="det_batt">${hasBatt ? s.batt?.toFixed(3) : skeletonVal(50)}</div>
            <div class="tile-unit">Volt</div>
            <div class="bar-track"><div class="bar-fill" id="det_batt_bar" style="width:${hasBatt ? bp : 0}%;background:${bc}"></div></div>
          </div>
          <div class="tile"><div class="tile-lbl">${t('rssi')}</div><div class="tile-val" style="color:${rc}" id="det_rssi">${hasRssi ? s.rssi : skeletonVal(30)}</div><div class="tile-unit">${t('dbm')} · ${hasRssi ? rssiBarsSVG(s.rssi) : ''}</div><div class="bar-track"><div class="bar-fill" id="det_rssi_bar" style="width:${hasRssi ? rp : 0}%;background:${rc}"></div></div></div>
          <div class="tile"><div class="tile-lbl">${t('active_time')}</div><div class="tile-val" id="det_act">${hasAct ? ((s.active_us || 0) / 1000).toFixed(1) : skeletonVal(40)}</div><div class="tile-unit">${t('ms')}</div></div>
          <div class="tile"><div class="tile-lbl">${t('channel')}</div><div class="tile-val" id="det_chan">${s.channel || skeletonVal(20)}</div></div>
          <div class="tile"><div class="tile-lbl">${t('interval')}</div><div class="tile-val" id="det_sleep">${s.sleep_int ? s.sleep_int + 's' : skeletonVal(30)}</div></div>
          <div class="tile"><div class="tile-lbl">${t('last_seen')}</div><div class="tile-val" id="det_last_seen" style="font-size:0.85rem;">${timeAgo(s._ts)}</div></div>
        </div>
      </div>
    </div>
    <div id="rawSection" style="display:none"><div class="section-label">${t('raw_payload')}</div><div class="raw-block" id="rawPayload">${formatRaw(s)}</div></div>
  </div>`;
}

export function refreshSensorDetail(id) {
  const s = sensors[id];
  if (!s) return;

  const upd = (elId, val, k, color) => {
    const el = document.getElementById(elId);
    if (el && sensorTileHasData(id, k || elId, val)) {
      const isArchived = isNodeOffline(s);
      const sv = String(val);
      if (el.textContent !== sv) {
        el.textContent = sv;
        if (!isArchived) {
          el.classList.remove('val-flash');
          void el.offsetWidth;
          el.classList.add('val-flash');
        }
      }
      if (color) el.style.color = color;
    }
  };

  upd('det_seq',  s.seq,                          'seq');
  upd('det_rssi', s.rssi,                          'rssi',      rssiColor(s.rssi));
  upd('det_batt', s.batt?.toFixed(3) ?? '—',       'batt',      battColor(s.batt));
  upd('det_tsago', timeAgo(s._ts));
  upd('det_act',  ((s.active_us || 0) / 1000).toFixed(1), 'active_us');
  upd('det_chan',  s.channel, 'channel');
  upd('det_sleep', s.sleep_int ? s.sleep_int + 's' : null, 'sleep_int');

  const rc = rssiColor(s.rssi), rp = rssiPct(s.rssi);
  const bc = battColor(s.batt), bp = battPct(s.batt);
  const rb = document.getElementById('det_rssi_bar');
  const bb = document.getElementById('det_batt_bar');
  const bValEl = document.getElementById('det_batt');

  if (rb && sensorTileHasData(id, 'rssi', s.rssi))  { rb.style.width = rp + '%'; rb.style.background = rc; }
  if (bb && sensorTileHasData(id, 'batt', s.batt))  { 
    bb.style.width = bp + '%'; 
    bb.style.background = bc; 
  }
  if (bValEl && sensorTileHasData(id, 'batt', s.batt)) {
    bValEl.style.color = bc;
    bValEl.innerHTML = (s.batt?.toFixed(3) ?? '—');
  }

  const isArchived = isNodeOffline(s);
  const isLost = (Date.now() - s._ts) > (s.sleep_int ? s.sleep_int * 1200 : 60000);
  const sEl = document.getElementById('det_status');
  if (sEl) {
    sEl.className = 'badge ' + (isArchived ? 'badge-red' : (isLost ? 'badge-amber' : 'badge-green'));
    sEl.textContent = isArchived ? 'Offline' : (isLost ? 'Ztraceno' : 'Online');
  }

  const chg = isCharging(s);
  
  const info = stypeInfo(s.stype);
  (s._floats || []).forEach((f, i) => {
    let color = null;
    const lbl = info.labels[i] || '';
    const isTempHum = lbl.toLowerCase().includes('teplota') || lbl.toLowerCase().includes('vlhkost') || lbl.toLowerCase().includes('temp') || lbl.toLowerCase().includes('hum');
    
    const cardEl = document.getElementById(`fc_${id}_${i}`);
    if (cardEl) {
      const lblEl = cardEl.querySelector('.fc-label');
      if (lblEl) {
        const iconExists = lblEl.querySelector('.chg-inflict-icon');
        if (chg && isTempHum && !iconExists) {
          lblEl.insertAdjacentHTML('beforeend', `<span class="chg-inflict-icon" title="${t('charging_warn')}">❗</span>`);
        } else if ((!chg || !isTempHum) && iconExists) {
          iconExists.remove();
        }
      }
    }

    if (s.stype === 8 && i === 3) {
      const g = getGasInfo(f);
      color = g.color;
      const se = document.getElementById(`fcv_status_${id}_${i}`);
      if (se && sensorTileHasData(id, 'data', s.data)) { se.textContent = g.label; se.style.color = g.color; }
    }
    upd(`fcv_${id}_${i}`, f.toFixed(2), 'data', color);
  });

  import('./state.js').then(({ activeTab: tab }) => {
    if (tab !== 'dashboard') return;
    Object.keys(history[id] || {}).forEach(k => {
      if (!charts[k]) return;
      const rMin   = syncCharts ? globalRangeMin : (chartRanges[k] ?? globalRangeMin);
      const cutoff = rMin > 0 ? Date.now() - rMin * 60000 : 0;
      history[id][k] = (history[id][k] || []).filter(p => p.t >= cutoff);
      const pts = history[id][k];
      charts[k].data.labels            = pts.map(p => fmtTime(p.t));
      charts[k].data.datasets[0].data  = pts.map(p => p.v);
      charts[k].update('none');

      import('./charts.js').then(({ activeLargeChartKey, default: _ }) => {
        if (activeLargeChartKey === k) {
          const lc = window._largeChart;
          if (lc) { lc.data.labels = pts.map(p => fmtTime(p.t)); lc.data.datasets[0].data = pts.map(p => p.v); lc.update('none'); }
        }
      });
    });
  });
}

/* ─── SMĚROVAČ DETAIL ──────────────────────────────────────────────── */
export function buildRouterDetail(id) {
  const r = routers[id];
  if (!r) return '';

  const now        = Date.now();
  const isArchived = isNodeOffline(r);
  const isLost     = (now - r._ts) > (r.sleep_int ? r.sleep_int * 1200 : 60000); 

  const chg = isCharging(r);

  const rc      = rssiColor(r.rssi_to_gw);
  const rp      = rssiPct(r.rssi_to_gw);
  const bc      = battColor(r.batt);
  const bp      = battPct(r.batt);

  const hasRssi   = routerTileHasData(id, 'rssi_to_gw', r.rssi_to_gw);
  const hasHops   = routerTileHasData(id, 'hops', r.hops);
  const hasUptime = routerTileHasData(id, 'uptime_us', r.uptime_us);
  const hasBatt   = routerTileHasData(id, 'batt', r.batt);

  const linkedSensors = Object.keys(sensors).filter(sid => sensors[sid].relay === id).sort();
  const sensorsHtml = linkedSensors.length
    ? linkedSensors.map(sid => {
        const s = sensors[sid], info = stypeInfo(s.stype);
        return `<div class="linked-sensor" onclick="App.showDetail('${sid}','sensor')">
          <div class="ls-icon ni-${info.cls}">${info.name.slice(0, 3)}</div>
          <div style="flex:1"><div style="font-family:var(--mono);font-weight:600;font-size:0.82rem;">${sid}</div>
          <div style="font-size:0.63rem;color:var(--muted);">${info.name} · RSSI ${s.rssi} dBm</div></div>
          <div>${rssiBarsSVG(s.rssi)}</div>
        </div>`;
      }).join('')
    : `<div style="font-size:0.78rem;color:var(--faint);padding:14px;text-align:center;background:var(--surface);border:1px dashed var(--border);border-radius:var(--radius);">Žádné připojené senzory</div>`;

  return `<div class="detail" style="${isArchived ? 'opacity:0.85' : ''}">
    <div class="detail-header${isArchived ? ' archived' : ''}">
      <div class="detail-header-left">
        <div class="detail-node-icon router${isArchived ? ' archived' : ''}">
          <svg viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2"><path d="M13 2L3 14h9l-1 8 10-12h-9l1-8z"/></svg>
        </div>
        <div>
          <div style="display:flex;align-items:baseline;gap:8px;">
            <div class="detail-id">${getNodeName(id)}</div>
            <div style="font-family:var(--mono);font-size:0.75rem;color:var(--muted);opacity:0.7;">#${id}</div>
            <button onclick="App.showRenameModal('${id}','router')" style="background:none;border:none;color:var(--blue);cursor:pointer;padding:2px;display:flex;align-items:center;" title="Přejmenovat">
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>
            </button>
          </div>
          <div class="detail-badges">
            <span id="det_status" class="badge ${isArchived ? 'badge-red' : (isLost ? 'badge-amber' : 'badge-green')}">${isArchived ? t('offline') : (isLost ? t('lost') : t('online'))}</span>
            <span class="badge badge-green">${t('router')}</span>
            ${getNodeGroup(id) ? `<span class="badge badge-indigo">${getNodeGroup(id)}</span>` : ''}
            ${!isArchived && hasHops 
              ? (r.hops === 1 
                  ? `<span class="badge badge-green">Přímé spojení</span>` 
                  : `<span class="badge badge-blue" id="det_hops_badge">${r.hops} hopy</span>`)
              : ''}
          </div>
        </div>
      </div>
      <div class="detail-meta">
        <span class="meta-time" id="det_tsago">${r._ts ? timeAgo(r._ts) : skeletonVal(60)}</span>
        <div style="display:flex;align-items:center;gap:5px;justify-content:flex-end;">
          ${r._ts ? (isArchived ? '<div class="pill-dot" style="background:var(--red);width:6px;height:6px;"></div> OFFLINE' : '<div class="pill-dot live" style="background:var(--green);width:6px;height:6px;"></div> Live') : skeletonVal(40)}
        </div>
        <div>Poslední: ${r._ts ? fmtTime(r._ts) : skeletonVal(80)}</div>
      </div>
    </div>
    <div class="tiles-row" style="${isArchived ? 'opacity:0.8' : ''}">
      <div class="tile">
        <div class="tile-lbl">Baterie</div>
        <div class="tile-val" style="color:${bc}" id="det_batt">${hasBatt ? r.batt?.toFixed(3) : skeletonVal(50)}</div>
        <div class="tile-unit">Volt</div>
        <div class="bar-track"><div class="bar-fill" id="det_batt_bar" style="width:${hasBatt ? bp : 0}%;background:${bc}"></div></div>
      </div>
      <div class="tile"><div class="tile-lbl">${isArchived ? 'Poslední RSSI' : 'RSSI → Gateway'}</div><div class="tile-val" id="det_rssi" style="color:${rc}">${hasRssi ? r.rssi_to_gw : skeletonVal(40)}</div><div class="tile-unit">dBm</div><div class="bar-track"><div class="bar-fill" id="det_rssi_bar" style="width:${hasRssi ? rp : 0}%;background:${rc}"></div></div></div>
      <div class="tile"><div class="tile-lbl">${isArchived ? 'Poslední skoky' : 'Počet skoků'}</div><div class="tile-val" id="det_hops">${hasHops ? r.hops : skeletonVal(30)}</div></div>
      <div class="tile"><div class="tile-lbl">Kanál</div><div class="tile-val" id="det_chan">${r.channel || skeletonVal(20)}</div></div>
      <div class="tile"><div class="tile-lbl">Interval</div><div class="tile-val" id="det_sleep">${r.sleep_int ? r.sleep_int + 's' : skeletonVal(30)}</div></div>
      <div class="tile"><div class="tile-lbl">${isArchived ? 'Poslední uptime' : 'Uptime'}</div><div class="tile-val" id="det_uptime">${hasUptime ? formatSeconds(r.uptime_us / 1000000) : skeletonVal(70)}</div></div>
      <div class="tile"><div class="tile-lbl">Naposledy viděn</div><div class="tile-val" id="det_last_seen" style="font-size:0.85rem;">${r._ts ? timeAgo(r._ts) : skeletonVal(60)}</div></div>
      ${r.parent ? `<div class="tile"><div class="tile-lbl">Parent</div><div class="tile-val">${r.parent}</div></div>` : ''}
    </div>
    <div><div class="section-label">Připojené senzory</div>
      <div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(240px,1fr));gap:10px;">${sensorsHtml}</div>
    </div>
    <div id="rawSection" style="margin-top:20px;"><div class="section-label">Raw JSON</div><div class="raw-block">${formatRaw(r)}</div></div>
  </div>`;
}

export function refreshRouterDetail(id) {
  const r = routers[id];
  if (!r) return;

  const upd = (elId, val, k) => {
    const el = document.getElementById(elId);
    if (el && routerTileHasData(id, k || elId, val)) {
      if (el.textContent !== String(val)) {
        el.textContent = val;
        el.classList.remove('val-flash');
        void el.offsetWidth;
        el.classList.add('val-flash');
      }
    }
  };

  const isArchived = isNodeOffline(r);
  const isLost = (Date.now() - r._ts) > (r.sleep_int ? r.sleep_int * 1200 : 60000);
  const sEl = document.getElementById('det_status');
  if (sEl) {
    sEl.className = 'badge ' + (isArchived ? 'badge-red' : (isLost ? 'badge-amber' : 'badge-green'));
    sEl.textContent = isArchived ? 'Offline' : (isLost ? 'Ztraceno' : 'Online');
  }

  upd('det_tsago',   timeAgo(r._ts));
  upd('det_rssi',    r.rssi_to_gw, 'rssi_to_gw');
  upd('det_rssi_val', r.rssi_to_gw, 'rssi_to_gw');
  upd('det_batt',    r.batt?.toFixed(3) ?? '—', 'batt');
  upd('det_hops',    r.hops,       'hops');
  upd('det_chan',    r.channel,    'channel');
  upd('det_sleep',   r.sleep_int ? r.sleep_int + 's' : null, 'sleep_int');

  const hbEl = document.getElementById('det_hops_badge');
  if (hbEl && routerTileHasData(id, 'hops', r.hops)) hbEl.textContent = r.hops + (r.hops === 1 ? ' hop' : ' hopy');
  if (routerTileHasData(id, 'uptime_us', r.uptime_us)) upd('det_uptime', formatSeconds(r.uptime_us / 1000000), 'uptime_us');

  const rc = rssiColor(r.rssi_to_gw), rp = rssiPct(r.rssi_to_gw);
  const bc = battColor(r.batt),       bp = battPct(r.batt);
  
  const rb  = document.getElementById('det_rssi_bar');
  const bb  = document.getElementById('det_batt_bar');
  const bValEl = document.getElementById('det_batt');
  const rEl = document.getElementById('det_rssi');

  if (rEl && routerTileHasData(id, 'rssi_to_gw', r.rssi_to_gw)) rEl.style.color = rc;
  if (rb  && routerTileHasData(id, 'rssi_to_gw', r.rssi_to_gw)) { rb.style.width = rp + '%'; rb.style.background = rc; }
  
  if (bb && routerTileHasData(id, 'batt', r.batt)) { bb.style.width = bp + '%'; bb.style.background = bc; }
  if (bValEl && routerTileHasData(id, 'batt', r.batt)) {
    bValEl.style.color = bc;
    bValEl.innerHTML = (r.batt?.toFixed(3) ?? '—');
  }
}

/* ─── BRÁNA DETAIL ─────────────────────────────────────────────── */
export function buildGatewayDetail() {
  if (!gateway) return '<div class="welcome"><div class="welcome-sub">Načítám bránu...</div></div>';
  const g       = gateway;
  const isArchived = isNodeOffline(g);
  const isLost     = (Date.now() - g._ts) > (g.pub_interval_s ? g.pub_interval_s * 1200 : 60000);
  const knownKeys = new Set(GW_FIELDS.map(f => f.key));
  const extraKeys = Object.keys(g).filter(k => 
    !GW_SKIP_KEYS.has(k) && 
    !knownKeys.has(k) && 
    !k.startsWith('_') &&
    !['result','table','id','type','host','topic','stype','relay','data','plen','ts'].includes(k)
  );

  const allTiles = [
    ...GW_FIELDS,
    ...extraKeys.map(k => ({ key: k, label: k.replace(/_/g, ' '), wide: String(g[k] || '').length > 20, fmt: 'auto' })),
  ];

  const tilesHtml = allTiles.map(({ key, label, wide, fmt }) => {
    const hasData = gwTileHasData(key, g[key]);
    const isWide  = wide || (typeof g[key] === 'string' && g[key].length > 18);
    let displayHtml;
    if (!hasData) {
      displayHtml = skeletonVal(key === 'ip' || key === 'apn' ? 120 : 70);
    } else {
      const val = g[key];
      if (fmt === 'auto') {
        const n = parseFloat(val);
        displayHtml = (!isNaN(n) && typeof val === 'number')
          ? (Number.isInteger(n) ? String(n) : n.toFixed(2))
          : String(val ?? '—');
      } else {
        displayHtml = formatGwValue(key, val, fmt);
      }
    }
    return `<div class="gw-tile${isWide ? ' wide' : ''}" id="gwt_${key}">
      <div class="gw-tile-lbl">${label}</div>
      <div class="gw-tile-val" id="gw_${key}">${displayHtml}</div>
    </div>`;
  }).join('');

  return `<div class="detail" style="${isArchived ? 'opacity:0.85' : ''}">
    <div class="detail-header${isArchived ? ' archived' : ''}">
      <div class="detail-node-icon gateway${isArchived ? ' archived' : ''}">
        <svg viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2"><path d="M5 12h14M12 5l7 7-7 7"/></svg>
      </div>
      <div class="detail-id">Brána</div>
      <div class="detail-badges">
        <span id="det_status" class="badge ${isLost ? 'badge-red' : 'badge-green'}">${isLost ? 'Ztraceno' : 'Online'}</span>
        <span class="badge badge-red">Brána</span>
        ${g.ip ? `<span class="badge badge-blue">${g.ip}</span>` : ''}
      </div>
    </div>
    <div class="detail-meta">
      <span class="meta-time" id="det_tsago">${g._ts ? timeAgo(g._ts) : skeletonVal(60)}</span>
      <div style="display:flex;align-items:center;gap:5px;justify-content:flex-end;">
        ${g._ts ? (isArchived ? '<div class="pill-dot" style="background:var(--red);width:6px;height:6px;"></div> OFFLINE' : '<div class="pill-dot live" style="background:var(--green);width:6px;height:6px;"></div> Live') : skeletonVal(40)}
      </div>
      <div>Poslední: ${g._ts ? fmtTime(g._ts) : skeletonVal(80)}</div>
    </div>
    <div class="section-label">Konektivita (Link)</div>
    <div class="chart-card" style="margin-bottom:20px; height:160px;">
      <div class="chart-wrap"><canvas id="chart_gw_link" style="cursor:pointer;"></canvas></div>
    </div>
    <div class="section-label">Systémové informace</div>
    <div class="gw-tiles-grid">${tilesHtml}</div>
    <div class="section-label">Všechna data</div><div class="raw-block">${formatRaw(g)}</div>
  </div>`;
}

export function refreshGatewayDetail() {
  if (!gateway) return;
  const g = gateway;
  
  // Aktualizace grafu konektivity
  import('./charts.js').then(m => {
    if (typeof m.updateGwChart === 'function') m.updateGwChart();
  });

  const isArchived = isNodeOffline(g);
  const isLost = (Date.now() - g._ts) > (g.pub_interval_s ? g.pub_interval_s * 1200 : 60000);

  const sEl = document.getElementById('det_status');
  if (sEl) {
    sEl.className = 'badge ' + (isArchived ? 'badge-red' : (isLost ? 'badge-amber' : 'badge-green'));
    sEl.textContent = isArchived ? 'Offline' : (isLost ? 'Ztraceno' : 'Online');
  }

  const tsEl = document.getElementById('det_tsago');
  if (tsEl) tsEl.textContent = timeAgo(g._ts);

  const allFields = [
    ...GW_FIELDS,
    ...Object.keys(g).filter(k => !GW_SKIP_KEYS.has(k) && !GW_FIELDS.find(f => f.key === k)).map(k => ({ key: k, fmt: 'auto' })),
  ];

  GW_SKIP_KEYS.forEach(k => {
    const el = document.getElementById('gwt_' + k);
    if (el) el.style.display = 'none';
  });

  allFields.forEach(({ key, fmt }) => {
    const el     = document.getElementById('gw_' + key);
    const tileEl = document.getElementById('gwt_' + key);
    if (tileEl) tileEl.style.display = '';
    if (!el) return;

    const val = g[key];
    if (!gwTileHasData(key, val)) return;

    let displayHtml;
    if (fmt === 'auto' || !fmt) {
      const n = parseFloat(val);
      displayHtml = (!isNaN(n) && typeof val === 'number') ? (Number.isInteger(n) ? String(n) : n.toFixed(2)) : String(val);
    } else {
      displayHtml = formatGwValue(key, val, fmt);
    }

    if (el.innerHTML.trim() !== String(displayHtml).trim()) {
      el.innerHTML = displayHtml;
      el.classList.remove('val-flash');
      void el.offsetWidth;
      el.classList.add('val-flash');
    }
  });

  const rawEl = document.querySelector('.raw-block');
  if (rawEl && selectedId === 'GATEWAY') rawEl.innerHTML = formatRaw(g);
}

/* ─── PŘEHLED SÍTĚ (Topologie) ───────────────────────────── */
export function buildNetworkOverview() {
  const sids = Object.keys(sensors).filter(id => !isNodeOffline(sensors[id])).sort();
  const rids = Object.keys(routers).filter(id => !isNodeOffline(routers[id])).sort();

  const groups = {};
  const addToGroup = (id, type) => {
    const g = getNodeGroup(id) || 'Bez skupiny';
    if (!groups[g]) groups[g] = { sensors: [], routers: [] };
    if (type === 'sensor') groups[g].sensors.push(id);
    else groups[g].routers.push(id);
  };

  sids.forEach(id => addToGroup(id, 'sensor'));
  rids.forEach(id => addToGroup(id, 'router'));

  const sortedGroupNames = Object.keys(groups).sort((a, b) => {
    if (a === 'Bez skupiny') return 1;
    if (b === 'Bez skupiny') return -1;
    return a.localeCompare(b);
  });

  const groupsHtml = sortedGroupNames.map(gName => {
    const g = groups[gName];
    const sCards = g.sensors.map(id => {
      const s = sensors[id], info = stypeInfo(s.stype), rc = rssiColor(s.rssi);
      const hasData = sensorTileHasData(id, 'data', s.data);
      const hasRssi = sensorTileHasData(id, 'rssi', s.rssi);
      const mainVal = hasData && s._floats && s._floats[0] !== undefined
        ? `${s._floats[0].toFixed(1)} ${info.units[0] || ''}`.trim()
        : skeletonVal(50);
      return `<div class="ov-card" id="ovc_${id}" onclick="App.showNodeWidget('${id}','sensor')">
        <div class="ov-card-type">${info.name}</div>
        <div class="ov-card-id">${getNodeName(id)}</div>
        <div class="ov-card-vals" id="ovv_${id}" style="color:var(--text);">${mainVal}<br>${rssiBarsSVG(s.rssi)} <span style="color:${rc}">${hasRssi ? s.rssi + ' dBm' : skeletonVal(30)}</span></div>
        <div class="ov-bottom-bar" id="ovb_${id}" style="background:${rc};opacity:0.25"></div>
      </div>`;
    }).join('');

    const rCards = g.routers.map(id => {
      const r = routers[id], rc = rssiColor(r.rssi_to_gw);
      const hasRssi = routerTileHasData(id, 'rssi_to_gw', r.rssi_to_gw);
      const hasHops = routerTileHasData(id, 'hops', r.hops);
      return `<div class="ov-card" id="ovc_${id}" onclick="App.showNodeWidget('${id}','router')">
        <div class="ov-card-type" style="color:var(--green)">${t('router').toUpperCase()}</div>
        <div class="ov-card-id">${getNodeName(id)}</div>
        <div class="ov-card-vals" id="ovv_${id}" style="color:var(--text);">${t('hops')}: ${hasHops ? r.hops : skeletonVal(20)}<br>${rssiBarsSVG(r.rssi_to_gw)} <span style="color:${rc}">${hasRssi ? r.rssi_to_gw + ' dBm' : skeletonVal(30)}</span></div>
        <div class="ov-bottom-bar" id="ovb_${id}" style="background:var(--green);opacity:0.25"></div>
      </div>`;
    }).join('');

    return `
      <div class="ov-group-section">
        <div class="ov-group-title">${gName} <span class="ov-group-count">${g.sensors.length + g.routers.length}</span></div>
        <div class="ov-grid">${rCards}${sCards}</div>
      </div>`;
  }).join('');

  return `<div class="detail">
    <div class="detail-header">
      <div><div class="detail-id">Přehled sítě</div>
        <div class="detail-badges">
          <span class="badge badge-blue" id="ov_total">${sids.length + rids.length} uzlů</span>
          <span class="badge badge-green" id="ov_sensors">${sids.length} senzorů</span>
          <span class="badge badge-indigo" id="ov_routers">${rids.length} směrovačů</span>
        </div>
      </div>
    </div>
    <div><div class="section-label">Logická topologie sítě</div>
      <div class="topo-card">
        <div style="position:relative;overflow:hidden;background:var(--surface2);border-radius:12px;min-height:450px;" id="topoWrap">
          <div id="cy" style="width:100%;height:450px;"></div>
          <!-- Legenda -->
          <div style="position:absolute; bottom:15px; right:15px; background:var(--surface); padding:10px; border-radius:8px; border:1px solid var(--border); font-size:0.65rem; display:grid; gap:8px; z-index:100; pointer-events:none; box-shadow: var(--shadow-sm);">
            <div style="display:flex; align-items:center; gap:8px;">
              <div style="width:12px; height:12px; background:#ef4444; clip-path: polygon(50% 0%, 100% 50%, 50% 100%, 0% 50%);"></div>
              <span style="font-weight:700; color:var(--hi);">Brána</span>
            </div>
            <div style="display:flex; align-items:center; gap:8px;">
              <div style="width:12px; height:12px; background:#10b981; border-radius:50%;"></div>
              <span style="font-weight:700; color:var(--hi);">Směrovač</span>
            </div>
            <div style="display:flex; align-items:center; gap:8px;">
              <div style="width:12px; height:12px; background:#2563eb; border-radius:50%;"></div>
              <span style="font-weight:700; color:var(--hi);">Senzor</span>
            </div>
            <div style="margin-top:4px; padding-top:4px; border-top:1px solid var(--border2); display:grid; gap:4px;">
              <div style="display:flex; align-items:center; gap:8px;">
                <div style="width:10px; height:10px; background:#f59e0b; border-radius:50%; box-shadow: 0 0 5px #f59e0b;"></div>
                <span style="color:var(--muted);">Průchod paketu</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
    <div><div class="section-label">Správa skupin & uzlů</div>
      <div id="ov_groups_mgmt" class="raw-block" style="max-height:400px;overflow-y:auto;font-size:0.75rem;background:var(--surface);padding:0;margin-bottom:20px;">
        Načítám správu...
      </div>
    </div>
    <div><div class="section-label">Uzly v síti</div>
      <div id="ovGroupsWrap">${groupsHtml || '<div style="text-align:center;padding:40px;opacity:0.5;">Žádné aktivní uzly</div>'}</div>
    </div>
  </div>`;
}

export function refreshNetworkOverview() {
  if (selectedType !== 'overview') return;
  
  const sids = Object.keys(sensors).filter(id => !isNodeOffline(sensors[id])).sort();
  const rids = Object.keys(routers).filter(id => !isNodeOffline(routers[id])).sort();

  const ot = document.getElementById('ov_total');
  const os = document.getElementById('ov_sensors');
  const or = document.getElementById('ov_routers');
  if (ot) ot.textContent = `${sids.length + rids.length} uzlů`;
  if (os) os.textContent = `${sids.length} senzorů`;
  if (or) or.textContent = `${rids.length} směrovačů`;

  // 1. Správa skupin a uzlů
  const mgmtEl = document.getElementById('ov_groups_mgmt');
  if (mgmtEl) {
    const allIds = [...new Set([...Object.keys(sensors), ...Object.keys(routers)])].sort();
    const isShowingLoading = mgmtEl.textContent.includes('Načítám');
    const currentRows = mgmtEl.querySelectorAll('tbody tr').length;
    
    if (isShowingLoading || currentRows !== allIds.length) {
      mgmtEl.innerHTML = `
        <table style="width:100%;border-collapse:collapse;font-size:0.7rem;">
          <thead style="background:var(--surface2);position:sticky;top:0;z-index:1;">
            <tr>
              <th style="text-align:left;padding:8px 12px;border-bottom:1px solid var(--border);">ID</th>
              <th style="text-align:left;padding:8px 12px;border-bottom:1px solid var(--border);">Název / Alias</th>
              <th style="text-align:left;padding:8px 12px;border-bottom:1px solid var(--border);">Skupina</th>
              <th style="text-align:right;padding:8px 12px;border-bottom:1px solid var(--border);">Akce</th>
            </tr>
          </thead>
          <tbody>
            ${allIds.map(id => `
              <tr style="border-bottom:1px solid var(--surface2);">
                <td style="padding:6px 12px;font-family:var(--mono);opacity:0.6;">${id}</td>
                <td style="padding:6px 12px;font-weight:600;">${getNodeName(id)}</td>
                <td style="padding:6px 12px;">${getNodeGroup(id) || '<span style="opacity:0.3;">–</span>'}</td>
                <td style="padding:6px 12px;text-align:right;">
                  <button onclick="App.showRenameModal('${id}')" style="background:none;border:none;color:var(--blue);cursor:pointer;padding:4px;" title="Upravit">
                    <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/></svg>
                  </button>
                </td>
              </tr>
            `).join('')}
          </tbody>
        </table>`;
    }
  }

  // 2. Aktualizace hodnot v kartách
  [...sids, ...rids].forEach(id => {
    const isSensor = !!sensors[id];
    const node = isSensor ? sensors[id] : routers[id];
    const valEl = document.getElementById('ovv_' + id);
    const barEl = document.getElementById('ovb_' + id);
    if (!valEl) return;

    let html = '';
    if (isSensor) {
      const info = stypeInfo(node.stype), rc = rssiColor(node.rssi);
      const hasData = sensorTileHasData(id, 'data', node.data);
      const hasRssi = sensorTileHasData(id, 'rssi', node.rssi);
      const mainVal = hasData && node._floats && node._floats[0] !== undefined
        ? `${node._floats[0].toFixed(1)} ${info.units[0] || ''}`.trim()
        : skeletonVal(50);
      html = `${mainVal}<br>${rssiBarsSVG(node.rssi)} <span style="color:${rc}">${hasRssi ? node.rssi + ' dBm' : skeletonVal(30)}</span>`;
      if (barEl) barEl.style.background = rc;
    } else {
      const rc = rssiColor(node.rssi_to_gw);
      const hasRssi = routerTileHasData(id, 'rssi_to_gw', node.rssi_to_gw);
      const hasHops = routerTileHasData(id, 'hops', node.hops);
      html = `${t('hops')}: ${hasHops ? node.hops : skeletonVal(20)}<br>${rssiBarsSVG(node.rssi_to_gw)} <span style="color:${rc}">${hasRssi ? node.rssi_to_gw + ' dBm' : skeletonVal(30)}</span>`;
      if (barEl) barEl.style.background = 'var(--green)';
    }

    if (valEl.innerHTML !== html) {
      valEl.innerHTML = html;
      valEl.classList.remove('val-flash');
      void valEl.offsetWidth;
      valEl.classList.add('val-flash');
    }
  });

  drawTopologySVG();
}

let cy = null;

export function initTopologyGraph() {
  const container = document.getElementById('cy');
  if (!container) return;

  const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
  const labelColor = isDark ? '#f0f6fc' : '#1e293b';
  const lineColor = isDark ? 'rgba(255,255,255,0.2)' : 'rgba(0,0,0,0.15)';

  cy = cytoscape({
    container: container,
    style: [
      {
        selector: 'node',
        style: {
          'label': 'data(label)',
          'text-valign': 'bottom',
          'text-halign': 'center',
          'text-margin-y': 8,
          'color': labelColor,
          'font-family': 'var(--mono)',
          'font-size': '10px',
          'font-weight': 'bold',
          'background-color': 'data(color)',
          'width': 'data(size)',
          'height': 'data(size)',
          'border-width': 3,
          'border-color': '#fff',
          'overlay-padding': '6px',
          'z-index': 10
        }
      },
      {
        selector: 'edge',
        style: {
          'width': 2,
          'line-color': lineColor,
          'target-arrow-color': lineColor,
          'target-arrow-shape': 'triangle',
          'curve-style': 'bezier',
          'line-style': 'data(style)',
          'opacity': 0.6
        }
      },
      {
        selector: 'node[type="gateway"]',
        style: { 'shape': 'diamond', 'border-color': '#fff' }
      },
      {
        selector: '.packet',
        style: {
          'background-color': '#f59e0b',
          'line-color': '#f59e0b',
          'target-arrow-color': '#f59e0b',
          'width': (node) => node.data('size') * 1.5,
          'height': (node) => node.data('size') * 1.5,
          'transition-property': 'background-color, line-color, width, height',
          'transition-duration': '0.2s'
        }
      }
    ],
    layout: { name: 'breadthfirst', directed: true, padding: 40 },
    userZoomingEnabled: true,
    userPanningEnabled: true
  });

  // Eventy
  cy.on('tap', 'node', function(evt) {
    const node = evt.target;
    App.hideTopoTip();
    App.showDetail(node.id(), node.data('type'));
  });

  cy.on('mouseover', 'node', function(evt) {
    const node = evt.target;
    const containerRect = container.getBoundingClientRect();
    const pos = node.renderedPosition();
    
    // Simulace eventu pro tooltip
    const fakeEl = {
      getBoundingClientRect: () => ({
        left: containerRect.left + pos.x - 10,
        top: containerRect.top + pos.y - 10,
        width: 20,
        height: 20
      })
    };
    
    App.showTopoTip(fakeEl, null, node.data('tooltip'));
  });

  cy.on('mouseout', 'node', function() {
    App.hideTopoTip();
  });

  drawTopologySVG(); // Prvotní naplnění daty
}

export function drawTopologySVG() {
  if (!cy) return;

  const activeRouters = Object.keys(routers).filter(id => !isNodeOffline(routers[id])).sort();
  const activeSensors = Object.keys(sensors).filter(id => !isNodeOffline(sensors[id])).sort();

  const elements = [];

  // Gateway
  elements.push({
    data: { 
      id: 'GW', 
      label: 'BRÁNA', 
      color: '#ef4444', 
      size: 32, 
      type: 'gateway',
      tooltip: `Brána|${gateway.ip || 'ESP32'}|-|-`
    }
  });

  // Směrovače
  activeRouters.forEach(id => {
    const r = routers[id];
    let parentId = 'GW';
    if (r.hops > 0 && r.parent && routers[r.parent] && !isNodeOffline(routers[r.parent])) {
      parentId = r.parent;
    }
    
    const rssi = r.rssi_to_gw || '?';
    const batt = r.batt ? r.batt.toFixed(2) + 'V' : '-';
    const alias = getNodeName(id);

    elements.push({
      data: { 
        id: id, 
        label: alias, 
        color: '#10b981', 
        size: 24, 
        type: 'router',
        tooltip: `${alias}|${alias !== id ? id : '-'}|${rssi} dBm|${batt}`
      }
    });
    elements.push({ data: { id: `e_${id}`, source: id, target: parentId, style: 'solid' } });
  });

  // Senzory
  activeSensors.forEach(id => {
    const s = sensors[id];
    let parentId = 'GW';
    if (s.hops > 0 && s.relay && s.relay !== '0000' && routers[s.relay] && !isNodeOffline(routers[s.relay])) {
      parentId = s.relay;
    }

    const rssi = s.rssi || '?';
    const batt = s.batt ? s.batt.toFixed(2) + 'V' : '-';
    const alias = getNodeName(id);

    elements.push({
      data: { 
        id: id, 
        label: alias, 
        color: '#2563eb', 
        size: 18, 
        type: 'sensor',
        tooltip: `${alias}|${alias !== id ? id : '-'}|${rssi} dBm|${batt}`
      }
    });
    elements.push({ data: { id: `e_${id}`, source: id, target: parentId, style: 'dashed' } });
  });

  cy.json({ elements: elements });
  
  // Přepočítat layout jen pokud se změnil počet prvků (nebo vynuceně)
  cy.layout({ name: 'breadthfirst', directed: true, padding: 40, animate: true, animationDuration: 500 }).run();
}

/** Spustí vizuální efekt průchodu paketu */
export function triggerPacketAnimation(nodeId) {
  if (!cy) return;
  const node = cy.getElementById(nodeId);
  if (!node || node.length === 0) return;

  // Najdeme hranu vedoucí od tohoto uzlu
  const edge = node.connectedEdges().filter(e => e.source().id() === nodeId);

  // Přidá třídu pro efekt
  node.addClass('packet');
  edge.addClass('packet');

  // Po chvíli efekt odstraní
  setTimeout(() => {
    node.removeClass('packet');
    edge.removeClass('packet');
  }, 400);
}

export function showTopoTip(el, e, data) {
  const tip = document.getElementById('topoTip');
  const txt = document.getElementById('topoTipText');
  if (!tip || !txt) return;

  const [name, id, rssi, batt] = data.split('|');
  const hasBatt = batt !== '-';
  
  txt.innerHTML = `
    <div style="font-weight:700;color:var(--blue);font-size:0.85rem;">${name}</div>
    ${id !== '-' ? `<div style="font-size:0.65rem;opacity:0.7;font-family:var(--mono);">${id}</div>` : ''}
    <div style="margin-top:6px;display:flex;gap:12px;font-size:0.7rem;font-weight:700;">
      ${hasBatt ? `<div style="display:flex;align-items:center;gap:4px;"><span style="color:var(--indigo)">⚡</span> ${batt}</div>` : ''}
      ${rssi !== '-' ? `<div style="display:flex;align-items:center;gap:4px;"><span style="color:var(--green)">📶</span> ${rssi}</div>` : ''}
    </div>
  `;
  
  const rect = el.getBoundingClientRect();
  tip.style.left = (rect.left + rect.width / 2) + 'px';
  tip.style.top  = (rect.top - 10) + 'px';
  
  tip.classList.add('visible');
}

export function hideTopoTip() {
  const tip = document.getElementById('topoTip');
  if (tip) tip.classList.remove('visible');
}

export function buildSystemView() {
  return `<div class="detail">
    <div class="detail-header">
      <div class="detail-header-left">
        <div class="detail-node-icon system">
          <svg viewBox="0 0 24 24" fill="none" stroke="white" stroke-width="2"><path d="M12 20V10M18 20V4M6 20v-4"/></svg>
        </div>
        <div>
          <div class="detail-id">Systém & Logy</div>
          <div class="detail-badges">
            <span class="badge badge-blue">Veřejný režim</span>
            <span class="badge badge-indigo" id="sys_log_count">0 zpráv</span>
          </div>
        </div>
      </div>
    </div>

    <div style="display:grid;grid-template-columns:1fr;gap:20px;">
      <div>
        <div class="section-label">Poslední návštěvy (Web)</div>
        <div id="sys_visits" class="raw-block" style="height:400px;overflow-y:auto;font-size:0.75rem;background:var(--surface);">
          Načítám...
        </div>
      </div>
    </div>
    <div style="margin-top:20px;">
      <div class="section-label" style="justify-content:space-between;display:flex;">
        <span>Systémové události</span>
        <button onclick="App.clearLog()" style="font-size:0.6rem;padding:2px 8px;cursor:pointer;background:none;border:1px solid var(--red);color:var(--red);border-radius:4px;">Smazat</button>
      </div>
      <div id="sys_logs" class="raw-block" style="height:400px;overflow-y:auto;font-size:0.75rem;background:var(--surface);">
        Načítám...
      </div>
    </div>
  </div>`;
}

export async function refreshSystemView() {
  if (selectedId !== 'SYSTEM') return;

  import('./log.js').then(m => {
    const logEl = document.getElementById('sys_logs');
    if (logEl) {
      logEl.innerHTML = m.getLogHTML();
      logEl.scrollTop = logEl.scrollHeight;
    }
    const cntEl = document.getElementById('sys_log_count');
    if (cntEl) cntEl.textContent = m.getLogCount() + ' zpráv';
  });

  import('./influx.js').then(async m => {
    const visitEl = document.getElementById('sys_visits');
    if (visitEl) {
      try {
        const rows = await m.fetchIpHistory('');
        import('./state.js').then(({ currentUserIp }) => {
          const uniqueIps = [];
          const seen = new Set();
          rows.forEach(r => {
            if (!seen.has(r.ip)) {
              seen.add(r.ip);
              uniqueIps.push(r);
            }
          });

          visitEl.innerHTML = uniqueIps.map(r => {
            const isMe = r.ip === currentUserIp;
            const bg = isMe ? 'background:var(--blue-lt);border-left:3px solid var(--blue);' : 'border-bottom:1px solid var(--surface2);';
            return `<div onclick="App.showIpHistory('${r.ip}')" 
              style="display:flex;justify-content:space-between;padding:8px 10px;cursor:pointer;${bg}" class="node-item-hover">
              <span style="font-family:var(--mono);font-weight:600;color:var(--hi);">
                ${r.ip}${isMe ? ' <small style="color:var(--blue);font-weight:800;margin-left:4px;">(VY)</small>' : ''}
              </span>
              <div style="display:flex;gap:8px;align-items:center;">
                <span style="font-size:0.6rem;opacity:0.6;">${timeAgo(new Date(r._time).getTime())}</span>
                <span class="badge badge-blue" style="font-size:0.6rem;">${r.role || 'guest'}</span>
              </div>
            </div>`;
          }).join('') || '<div style="padding:10px;opacity:0.5;">Žádné záznamy.</div>';
        });
      } catch (e) {
        visitEl.textContent = 'Chyba při načítání: ' + e.message;
      }
    }
  });
}

/* ─── WIDGET (popup v přehledu) ──────────────────────────────────── */
export function showNodeWidget(id, type) {
  setActiveWidget(id, type);
  const fullBtn = document.getElementById('modalFullBtn');
  if (fullBtn) {
    fullBtn.style.display = 'inline-flex';
    fullBtn.onclick = () => { hideNodeModal(); showDetail(id, type); };
  }
  refreshNodeWidget();
  document.getElementById('nodeModal').classList.add('visible');
}

export function refreshNodeWidget() {
  const id    = activeWidgetId;
  const type  = activeWidgetType;
  if (!id) return;

  const title = document.getElementById('modalTitle');
  const body  = document.getElementById('modalBody');
  if (!body) return;

  const displayName = getNodeName(id);
  title.textContent = `${type.toUpperCase()}: ${displayName}${displayName !== id ? ' (' + id + ')' : ''}`;

  let html = '';
  if (type === 'sensor') {
    const s    = sensors[id];
    if (!s) return;
    const info    = stypeInfo(s.stype);
    const hasData = sensorTileHasData(id, 'data', s.data);
    const hasRssi = sensorTileHasData(id, 'rssi', s.rssi);
    const rows    = info.labels.map((l, i) => {
      const val = (hasData && s._floats) ? s._floats[i].toFixed(2) : skeletonVal(40);
      return `<div style="display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--surface2);">
        <span style="color:var(--muted);font-weight:500;">${l}</span>
        <span style="font-family:var(--mono);font-weight:600;">${val} ${info.units[i] || ''}</span>
      </div>`;
    }).join('');
    html = `<div style="display:grid;gap:10px;">
      <div style="display:flex;gap:10px;margin-bottom:10px;"><div class="badge badge-blue">${info.name}</div></div>
      ${rows}
      <div style="display:flex;justify-content:space-between;padding:8px 0;margin-top:10px;"><span style="color:var(--muted);">RSSI</span><span style="font-weight:600;">${hasRssi ? s.rssi + ' dBm' : skeletonVal(30)}</span></div>
      <div style="display:flex;justify-content:space-between;padding:8px 0;opacity:0.8;font-size:0.75rem;"><span>Naposledy viděn</span><span id="mod_ts">${timeAgo(s._ts)}</span></div>
    </div>`;
  } else {
    const r      = routers[id];
    if (!r) return;
    const hasRssi   = routerTileHasData(id, 'rssi_to_gw', r.rssi_to_gw);
    const hasHops   = routerTileHasData(id, 'hops', r.hops);
    const hasUptime = routerTileHasData(id, 'uptime_us', r.uptime_us);
    html = `<div style="display:grid;gap:10px;">
      <div class="badge badge-green">${t('router').toUpperCase()}</div>
      <div style="display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--surface2);"><span style="color:var(--muted);font-weight:500;">RSSI k GW</span><span style="font-family:var(--mono);font-weight:600;">${hasRssi ? r.rssi_to_gw + ' dBm' : skeletonVal(30)}</span></div>
      <div style="display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--surface2);"><span style="color:var(--muted);font-weight:500;">Skoky</span><span style="font-family:var(--mono);font-weight:600;">${hasHops ? r.hops : skeletonVal(20)}</span></div>
      <div style="display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid var(--surface2);"><span style="color:var(--muted);font-weight:500;">Uptime</span><span style="font-family:var(--mono);font-weight:600;">${hasUptime ? formatSeconds(r.uptime_us / 1000000) : skeletonVal(60)}</span></div>
      <div style="display:flex;justify-content:space-between;padding:8px 0;opacity:0.8;font-size:0.75rem;"><span>Naposledy viděn</span><span id="mod_ts">${timeAgo(r._ts)}</span></div>
    </div>`;
  }
  body.innerHTML = html;
}

export function hideNodeModal() {
  const modal = document.getElementById('nodeModal');
  if (modal) modal.classList.remove('visible');
  clearActiveWidget();
}
