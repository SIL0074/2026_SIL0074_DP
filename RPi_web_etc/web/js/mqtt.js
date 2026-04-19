/* ─── mqtt.js ────────────────────────────────────────────────────── */
/* MQTT připojení a zpracování zpráv                                 */

import {
  sensors, routers, gateway, setGateway,
  history, msgTimestamps, msgCount, historyLoading,
  incMsgCount, addTimestamp, pruneTimestamps,
  setMqttClient, mqttClient,
  selectedId, selectedType,
  activeWidgetId, chargingStates
} from './state.js';
import * as State from './state.js';
import { hexToFloats } from './helpers.js';
import { addLog } from './log.js';
import {
  renderGwSidebar, renderSensorSidebar, renderRouterSidebar,
  markFresh, isNodeOffline,
} from './sidebar.js';
import { loadHistory } from './influx.js';
import { globalRangeMin } from './state.js';

/** Navazuje spojení – automaticky se znovupřipojuje */
export function connect() {
  if (mqttClient && mqttClient.isConnected()) return;

  const host   = window.location.hostname || 'localhost';
  const port   = window.location.port ? parseInt(window.location.port) : (window.location.protocol === 'https:' ? 443 : 80);
  const path   = '/mqtt';
  const useSSL = window.location.protocol === 'https:';
  const cid    = 'monitor_' + Math.random().toString(16).slice(2, 8);

  const c = new Paho.MQTT.Client(host, port, path, cid);
  setMqttClient(c);

  c.onConnectionLost = r => {
    addLog('MQTT Odpojeno: ' + r.errorMessage, 'err');
    setTimeout(connect, 5000);
  };

  c.onMessageArrived = handleMessage;

  c.connect({
    onSuccess: () => {
      addLog('MQTT Připojeno.', 'ok');
      c.subscribe('esp-now/#');
    },
    onFailure: e => {
      addLog('MQTT Chyba: ' + e.errorMessage, 'err');
      setTimeout(connect, 5000);
    },
    useSSL,
    timeout:          10,
    keepAliveInterval: 30,
  });
}

/** Zpracuje příchozí MQTT zprávu */
function handleMessage(msg) {
  incMsgCount();
  addTimestamp(Date.now());

  let p;
  try { p = JSON.parse(msg.payloadString); }
  catch { return; }

  // Charging State z backendu
  if (msg.destinationName.startsWith('esp-now/state/charging/')) {
    const id = msg.destinationName.split('/').pop();
    const isChg = p.charging === 1;
    chargingStates[id] = isChg; 
    
    if (sensors[id]) sensors[id].charging = isChg;
    if (routers[id]) routers[id].charging = isChg;

    renderSensorSidebar();
    renderRouterSidebar();

    if (selectedId === id && window.App) {
      if (sensors[id] && typeof window.App.refreshSensorDetail === 'function') {
        window.App.refreshSensorDetail(id);
      } else if (routers[id] && typeof window.App.refreshRouterDetail === 'function') {
        window.App.refreshRouterDetail(id);
      }
    }
    return;
  }

  // Gateway
  if (msg.destinationName.includes('gateway_info') || p.id === 'GATEWAY') {
    if (!gateway._received) gateway._received = {};
    Object.keys(p).forEach(k => { if (!k.startsWith('_')) gateway._received[k] = true; });
    setGateway({ ...p, _ts: Date.now() });

    // Historie stavu linku (0=WiFi, 1=NB-IoT)
    if (p.link !== undefined) {
      if (!history['GATEWAY']) history['GATEWAY'] = {};
      if (!history['GATEWAY']['link']) history['GATEWAY']['link'] = [];
      const linkVal = (String(p.link).toLowerCase().includes('wifi') || p.link === 0 || p.link === '0') ? 1 : 0;
      history['GATEWAY']['link'].push({ t: Date.now(), v: linkVal });
      if (history['GATEWAY']['link'].length > 200) history['GATEWAY']['link'].shift();
    }

    renderGwSidebar();
    if (window.App) window.App.triggerPacketAnimation('GW');
    if (selectedId === 'GATEWAY' && window.App) window.App.refreshGatewayDetail();
  }
  // Směrovač
  else if (p.type === 'ROUTER') {
    if (!routers[p.id])           routers[p.id] = { id: p.id };
    if (!routers[p.id]._received) routers[p.id]._received = {};
    Object.keys(p).forEach(k => { if (!k.startsWith('_')) routers[p.id]._received[k] = true; });

    routers[p.id] = { ...routers[p.id], ...p, _ts: Date.now(), charging: chargingStates[p.id] || false };
    renderRouterSidebar();
    if (selectedId === p.id && window.App)         window.App.refreshRouterDetail(p.id);
    if (selectedType === 'overview' && window.App) window.App.refreshNetworkOverview();
    if (activeWidgetId === p.id && window.App)     window.App.refreshNodeWidget();
  }
  // Senzor
  else if (p.id) {
    if (!sensors[p.id])           sensors[p.id] = { id: p.id };
    if (!sensors[p.id]._received) sensors[p.id]._received = {};
    Object.keys(p).forEach(k => { if (!k.startsWith('_')) sensors[p.id]._received[k] = true; });

    const floats = p.data ? hexToFloats(p.data) : [];
    const isNew  = !sensors[p.id]._floats;

    sensors[p.id] = {
      ...sensors[p.id],
      ...p,
      _ts:     Date.now(),
      _floats: floats.length ? floats : (sensors[p.id]?._floats || []),
      _prev:   sensors[p.id]?._floats,
      charging: chargingStates[p.id] || false 
    };

    if (!history[p.id]) history[p.id] = {};

    floats.forEach((v, i) => {
      if (!history[p.id][i]) history[p.id][i] = [];
      history[p.id][i].push({ t: Date.now(), v });
      if (history[p.id][i].length > 200) history[p.id][i].shift();
    });

    ['rssi', 'batt', 'active_us'].forEach(k => {
      if (p[k] === undefined) return;
      if (!history[p.id][k]) history[p.id][k] = [];
      history[p.id][k].push({ t: Date.now(), v: k === 'active_us' ? p[k] / 1000 : p[k] });
      if (history[p.id][k].length > 200) history[p.id][k].shift();
    });

    if (isNew) loadHistory(p.id, globalRangeMin);
    renderSensorSidebar();
    markFresh(p.id);
    if (window.App) window.App.triggerPacketAnimation(p.id);

    if (selectedId === p.id && window.App)         window.App.refreshSensorDetail(p.id);
    if (selectedType === 'overview' && window.App) window.App.refreshNetworkOverview();
    if (activeWidgetId === p.id && window.App)     window.App.refreshNodeWidget();
  }

  _updateHeaderStats();
}

function _updateHeaderStats() {
  pruneTimestamps();
  
  const activeSensors = Object.keys(sensors).filter(id => !isNodeOffline(sensors[id]));
  const activeRouters = Object.keys(routers).filter(id => !isNodeOffline(routers[id]));
  const n = activeSensors.length + activeRouters.length;

  const nodes = document.getElementById('hdrNodes');
  const sub   = document.getElementById('netSub');

  const totalVal = (State.initialMsgCountTotal || 0) + msgCount;
  const todayVal = (State.initialMsgCountToday || 0) + msgCount;

  const totalEl = document.getElementById('hdrTotal');
  const todayEl = document.getElementById('hdrTodayCount');

  // Vypíše hodnoty, i když je influx pomalý (dělalo to problémy)
  if (totalEl) totalEl.textContent = totalVal;
  if (todayEl) todayEl.textContent = todayVal;

  document.getElementById('hdrRate').textContent  = msgTimestamps.length;

  if (historyLoading && n === 0) {
    if (nodes) nodes.innerHTML = '<span class="skeleton" style="width:20px;height:0.9em;">&nbsp;</span>';
    if (sub)   sub.innerHTML   = '<span class="skeleton" style="width:40px;height:0.9em;">&nbsp;</span>';
  } else {
    if (nodes) nodes.textContent = n;
    if (sub)   sub.textContent   = `${n} uzlů`;
  }
}
