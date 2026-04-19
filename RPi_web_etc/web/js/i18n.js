/* ─── i18n.js ────────────────────────────────────────────────────── */
/* Internacionalizace - Překladač, nakonec jedu jen CZ                     */

export const translations = {
  cs: {
    // Header
    hdr_nodes: 'Uzlů', hdr_total: 'Celkem',
    // Sidebar
    general: 'Všeobecné', all_nodes: 'Všechny uzly', system: 'Systém & Logy',
    diagnostics: 'Diagnostika', sensors: 'Senzory', routers: 'Směrovače',
    archive: 'Archiv (Neaktivní)', visit_history: 'Historie návštěv',
    login: 'Přihlásit', logout: 'Odhlásit',
    guest: 'Host', admin: 'Admin',
    // Panel
    select_node: 'Vyberte uzel ze sidebaru',
    connect_mqtt: 'nebo se nejdříve připojte k MQTT brokeru',
    full_detail: 'Plný detail',
    chart_detail: 'Graf detail',
    // Auth
    login_title: 'Přihlášení', username: 'Uživatel', password: 'Heslo',
    sign_in: 'Přihlásit se', invalid_credentials: 'Neplatné přihlašovací údaje',
    // Detail labels
    online: 'Online', offline: 'Offline', lost: 'Ztraceno',
    last_seen: 'Naposledy viděn', last_val: 'Poslední hodnoty',
    current_val: 'Aktuální hodnoty', history: 'Historie',
    sys_metrics: 'Systémové metriky', sync_on: 'ZAPNUTÁ', sync_off: 'VYPNUTÁ',
    sync_label: 'Synchronizace:', direct_conn: 'Přímé spojení',
    raw_payload: 'Raw payload', all_data: 'Všechna data',
    sys_info: 'Systémové informace',
    conn_sensors: 'Připojené senzory', no_conn_sensors: 'Žádné připojené senzory',
    rename: 'Přejmenovat', rename_prompt: 'Zadejte název pro uzel',
    export_csv: 'Export CSV', export_json: 'Export JSON',
    // Gateway
    gateway: 'Brána', router: 'Směrovač', sensor: 'Senzor',
    // Network overview
    net_overview: 'Přehled sítě', topo_title: 'ESP-NOW síťový graf',
    topo_hover: 'Hover pro detail uzlu', net_nodes: 'uzlů',
    net_sensors: 'senzorů', net_routers: 'směrovačů',
    // Notifications
    node_offline_title: 'Uzel offline',
    node_online_title: 'Uzel online',
    batt_warn_title: 'Slabá baterie',
    batt_warn_msg: 'nabijte prosím',
    charging_warn: '⚠️ POZOR: Uzel se nabíjí! Naměřená teplota a vlhkost jsou nyní zcela NEVYPOVÍDAJÍCÍ kvůli zahřívání při nabíjení.',
    charging: 'Nabíjení',
    // System
    sys_title: 'Systém & Logy', sys_admin: 'Administrace',
    sys_msg_count: 'zpráv', sys_visits: 'Poslední návštěvy (Web)',
    sys_events: 'Systémové události', sys_delete: 'Smazat',
    no_records: 'Žádné záznamy.',
    // Dashboard
    dashboard: 'Dashboard',
    battery: 'Baterie', rssi: 'RSSI', active_time: 'Aktivní čas',
    channel: 'Kanál', interval: 'Interval',
    uptime: 'Uptime', hops: 'Počet skoků', parent: 'Parent',
    rssi_to_gw: 'RSSI → Brána', last_rssi: 'Poslední RSSI',
    last_hops: 'Poslední skoky', last_uptime: 'Poslední uptime',
    volt: 'Volt', ms: 'ms', dbm: 'dBm',
  }
};

export function getLang() { return 'cs'; }

export function t(key) {
  return translations.cs[key] || key;
}

export function toggleLang() {
  return 'cs';
}

export function applyI18n() {
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.dataset.i18n;
    el.textContent = t(key);
  });
  document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
    el.placeholder = t(el.dataset.i18nPlaceholder);
  });
  document.documentElement.lang = 'cs';
}

// Init
applyI18n();
