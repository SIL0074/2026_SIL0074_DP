const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="cs">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
    <title>DIPLOMOVÁ PRÁCE // BRÁNA</title>
    <style>
      @import url("https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;500;700;800&display=swap");
      :root {
        --bg: #09090b;
        --bg-grad: radial-gradient(circle at top right, #1a1b26, #09090b 70%);
        --panel: rgba(24, 24, 27, 0.6);
        --panel-hdr: rgba(255, 255, 255, 0.03);
        --border: rgba(255, 255, 255, 0.08);
        --text: #e4e4e7;
        --text-dim: #a1a1aa;
        --accent: #3b82f6;
        --accent-glow: rgba(59, 130, 246, 0.5);
        --success: #10b981;
        --warn: #f59e0b;
        --danger: #ef4444;
      }
      * {
        box-sizing: border-box;
        font-family: "Outfit", sans-serif;
      }
      body {
        background: var(--bg-grad);
        background-attachment: fixed;
        color: var(--text);
        margin: 0;
        padding: 2vw 5vw;
        font-size: 14px;
        min-height: 100vh;
      }
      h1,
      h2,
      h3,
      h4 {
        color: #fff;
        margin-top: 0;
        font-weight: 700;
      }
      .hdr {
        display: flex;
        justify-content: space-between;
        align-items: center;
        border-bottom: 1px solid var(--border);
        padding-bottom: 20px;
        margin-bottom: 30px;
      }
      .logo {
        font-size: 26px;
        font-weight: 800;
        color: #fff;
        text-shadow: 0 0 20px rgba(255, 255, 255, 0.2);
        letter-spacing: 1px;
      }
      .logo span {
        color: var(--accent);
        font-weight: 500;
      }

      .stat-grid {
        display: grid;
        grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
        gap: 20px;
        margin-bottom: 30px;
      }
      .stat-card {
        background: var(--panel);
        border: 1px solid var(--border);
        border-radius: 12px;
        padding: 20px;
        backdrop-filter: blur(12px);
        -webkit-backdrop-filter: blur(12px);
        box-shadow: 0 4px 24px -6px rgba(0, 0, 0, 0.5);
        transition:
          transform 0.2s,
          box-shadow 0.2s;
      }
      .stat-card:hover {
        transform: translateY(-2px);
        box-shadow: 0 10px 30px -10px rgba(0, 0, 0, 0.8);
        border-color: rgba(255, 255, 255, 0.15);
      }
      .stat-lbl {
        color: var(--text-dim);
        font-size: 12px;
        text-transform: uppercase;
        letter-spacing: 1px;
        margin-bottom: 8px;
        font-weight: 500;
      }
      .stat-val {
        font-size: 28px;
        font-weight: 700;
        color: #fff;
      }

      .panel {
        background: var(--panel);
        border: 1px solid var(--border);
        border-radius: 12px;
        margin-bottom: 25px;
        overflow: hidden;
        backdrop-filter: blur(12px);
        -webkit-backdrop-filter: blur(12px);
        box-shadow: 0 4px 24px -10px rgba(0, 0, 0, 0.5);
      }
      .panel-hdr {
        background: var(--panel-hdr);
        padding: 16px 20px;
        border-bottom: 1px solid var(--border);
        font-weight: 700;
        letter-spacing: 0.5px;
        display: flex;
        justify-content: space-between;
        align-items: center;
        color: #fff;
      }
      .panel-body {
        padding: 20px;
        overflow-x: auto;
      }

      table {
        width: 100%;
        border-collapse: separate;
        border-spacing: 0;
        text-align: left;
      }
      th,
      td {
        padding: 12px 15px;
        border-bottom: 1px solid var(--border);
      }
      th {
        color: var(--text-dim);
        font-size: 12px;
        font-weight: 600;
        text-transform: uppercase;
        letter-spacing: 0.5px;
      }
      tr:last-child td {
        border-bottom: none;
      }
      tbody tr {
        transition: background 0.2s;
      }
      tbody tr:hover {
        background: rgba(255, 255, 255, 0.02);
      }

      .badge {
        padding: 4px 10px;
        border-radius: 6px;
        font-size: 11px;
        font-weight: 700;
        letter-spacing: 0.5px;
        background: rgba(255, 255, 255, 0.05);
        border: 1px solid var(--border);
        backdrop-filter: blur(4px);
      }
      .badge.b-green {
        color: var(--success);
        border-color: rgba(16, 185, 129, 0.3);
        background: rgba(16, 185, 129, 0.1);
        box-shadow: 0 0 10px rgba(16, 185, 129, 0.1);
      }
      .badge.b-red {
        color: var(--danger);
        border-color: rgba(239, 68, 68, 0.3);
        background: rgba(239, 68, 68, 0.1);
        box-shadow: 0 0 10px rgba(239, 68, 68, 0.1);
      }

      .form-group {
        margin-bottom: 20px;
      }
      label {
        display: block;
        margin-bottom: 8px;
        color: var(--text-dim);
        font-size: 12px;
        font-weight: 500;
        letter-spacing: 0.5px;
      }
      input,
      select {
        width: 100%;
        padding: 10px 14px;
        background: rgba(0, 0, 0, 0.3);
        border: 1px solid var(--border);
        color: #fff;
        border-radius: 8px;
        font-size: 14px;
        transition:
          border-color 0.2s,
          box-shadow 0.2s;
        font-family: "Outfit", sans-serif;
      }
      input:focus,
      select:focus {
        outline: none;
        border-color: var(--accent);
        box-shadow: 0 0 0 3px rgba(59, 130, 246, 0.2);
      }

      button {
        background: var(--accent);
        color: #fff;
        border: none;
        padding: 10px 20px;
        border-radius: 8px;
        cursor: pointer;
        font-weight: 700;
        letter-spacing: 0.5px;
        transition: all 0.2s;
        box-shadow: 0 0 15px rgba(59, 130, 246, 0.3);
        font-family: "Outfit", sans-serif;
      }
      button:hover {
        background: #60a5fa;
        box-shadow: 0 0 20px rgba(96, 165, 250, 0.5);
        transform: translateY(-1px);
      }
      button:active {
        transform: translateY(1px);
      }

      .btn-sm {
        padding: 6px 12px;
        font-size: 11px;
        box-shadow: none;
        border: 1px solid transparent;
      }
      .btn-danger {
        background: transparent;
        border: 1px solid var(--danger);
        color: var(--danger);
        box-shadow: none;
      }
      .btn-danger:hover {
        background: var(--danger);
        color: #fff;
        box-shadow: 0 0 15px rgba(239, 68, 68, 0.4);
      }

      .nav-tabs {
        display: flex;
        gap: 10px;
        margin-bottom: 30px;
        border-bottom: 1px solid var(--border);
        padding-bottom: 15px;
        overflow-x: auto;
      }
      .tab-btn {
        background: transparent;
        color: var(--text-dim);
        border: 1px solid transparent;
        box-shadow: none;
        padding: 8px 16px;
        border-radius: 8px;
        font-size: 13px;
      }
      .tab-btn:hover {
        background: rgba(255, 255, 255, 0.05);
        color: #fff;
        transform: none;
      }
      .tab-btn.active {
        background: var(--accent);
        color: #fff;
        border-color: var(--accent);
        box-shadow: 0 0 15px rgba(59, 130, 246, 0.3);
      }
      .tab-content {
        display: none;
        animation: fadeIn 0.3s ease;
      }
      .tab-content.active {
        display: block;
      }
      @keyframes fadeIn {
        from {
          opacity: 0;
          transform: translateY(5px);
        }
        to {
          opacity: 1;
          transform: translateY(0);
        }
      }

      #logs {
        width: 100%;
        height: 400px;
        background: rgba(0, 0, 0, 0.5);
        color: #10b981;
        font-family: "Consolas", monospace;
        padding: 15px;
        border: none;
        border-radius: 0 0 12px 12px;
        overflow-y: auto;
        line-height: 1.5;
        font-size: 13px;
      }

      .grid-2 {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 25px;
      }
      @media (max-width: 900px) {
        .grid-2 {
          grid-template-columns: 1fr;
        }
        body {
          padding: 20px;
        }
      }

      /* Glowing scrollbars */
      ::-webkit-scrollbar {
        width: 8px;
        height: 8px;
      }
      ::-webkit-scrollbar-track {
        background: rgba(0, 0, 0, 0.2);
        border-radius: 4px;
      }
      ::-webkit-scrollbar-thumb {
        background: var(--border);
        border-radius: 4px;
      }
      ::-webkit-scrollbar-thumb:hover {
        background: var(--accent);
      }

      /* Status Dots */
      .status-dot {
        display: inline-block;
        width: 10px;
        height: 10px;
        border-radius: 50%;
        margin-right: 10px;
        vertical-align: middle;
      }
      .dot-online {
        background-color: var(--success);
        box-shadow: 0 0 10px var(--success);
        animation: pulse-green 2s infinite;
      }
      .dot-missed {
        background-color: var(--warn);
        box-shadow: 0 0 10px var(--warn);
        animation: pulse-orange 2s infinite;
      }
      @keyframes pulse-green {
        0% {
          box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.7);
        }
        70% {
          box-shadow: 0 0 0 8px rgba(16, 185, 129, 0);
        }
        100% {
          box-shadow: 0 0 0 0 rgba(16, 185, 129, 0);
        }
      }
      @keyframes pulse-orange {
        0% {
          box-shadow: 0 0 0 0 rgba(245, 158, 11, 0.7);
        }
        70% {
          box-shadow: 0 0 0 8px rgba(245, 158, 11, 0);
        }
        100% {
          box-shadow: 0 0 0 0 rgba(245, 158, 11, 0);
        }
      }
    </style>
  </head>
  <body>
    <div class="hdr">
      <div class="logo">
        BRÁNA
        <span style="color: var(--text-dim); font-weight: normal"
          >TERMINÁL</span
        >
      </div>
      <div id="live-ind" class="badge b-green">ŽIVĚ</div>
    </div>

    <div class="nav-tabs">
      <button class="tab-btn active" onclick="switchTab('dashboard')">
        PŘEHLED (DASHBOARD)
      </button>
      <button class="tab-btn" onclick="switchTab('network')">
        SÍŤ A NASTAVENÍ
      </button>
      <button class="tab-btn" onclick="switchTab('ota')">
        MESH OTA (KONFIGURACE)
      </button>
      <button class="tab-btn" onclick="switchTab('console')">
        SYSTÉMOVÝ LOG
      </button>
    </div>

    <!-- TAB: DASHBOARD -->
    <div id="dashboard" class="tab-content active">
      <div class="stat-grid">
        <div class="stat-card">
          <div class="stat-lbl">DOBA BĚHU (UPTIME)</div>
          <div class="stat-val" id="st-uptime">--:--:--</div>
        </div>
        <div class="stat-card">
          <div class="stat-lbl">MQTT PŘIPOJENÍ</div>
          <div class="stat-val" id="st-mqtt" style="color: var(--danger)">
            ODPOJENO
          </div>
        </div>
        <div class="stat-card">
          <div class="stat-lbl">NB-IOT MODEM</div>
          <div class="stat-val" id="st-nbiot" style="color: var(--warn)">
            INICIALIZACE...
          </div>
        </div>
        <div class="stat-card">
          <div class="stat-lbl">IP ADRESA (STA)</div>
          <div class="stat-val" id="st-ip" style="font-size: 22px">0.0.0.0</div>
        </div>
        <div class="stat-card">
          <div class="stat-lbl">PŘIJÍMACÍ MOST (BRIDGE)</div>
          <div class="stat-val" id="st-bridge" style="color: var(--warn)">
            OFFLINE
          </div>
        </div>
      </div>
      <!-- SENSORS PANEL -->
      <div class="panel">
        <div class="panel-hdr">
          <span>MESH SENZORY (SENSORS)</span>
          <div style="display: flex; gap: 10px; align-items: center">
            <button
              class="btn-sm btn-danger"
              onclick="resetStats()"
              title="Vynuluje počítadla ztrát pro nové měření"
            >
              VYNULOVAT STATISTIKY
            </button>
            <span id="st-scount" class="badge">0 AKTIVNÍCH</span>
          </div>
        </div>
        <div class="panel-body">
          <table id="sensor-table">
            <thead>
              <tr>
                <th>ID UZLU</th>
                <th>UPLINK</th>
                <th>SPOLEHLIVOST (PDR)</th>
                <th>RSSI</th>
                <th>BATERIE</th>
                <th>NAPOSLEDY VIDĚN</th>
                <th>KONFIGURACE (INT / SAFE / CH)</th>
                <th>DATA #1</th>
                <th>DATA #2</th>
              </tr>
            </thead>
            <tbody id="sensor-tbody">
              <tr>
                <td
                  colspan="9"
                  style="text-align: center; color: var(--text-dim)"
                >
                  Čekám na data...
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- ROUTERS PANEL -->
      <div class="panel">
        <div class="panel-hdr">
          <span>MESH SMĚROVAČE (ROUTERS)</span>
          <span id="st-rcount" class="badge">0 AKTIVNÍCH</span>
        </div>
        <div class="panel-body">
          <table id="router-table">
            <thead>
              <tr>
                <th>ID UZLU</th>
                <th>UPLINK</th>
                <th>POTOMCI (SENZORY)</th>
                <th>SPOLEHLIVOST</th>
                <th>RSSI</th>
                <th>BATERIE</th>
                <th>NAPOSLEDY VIDĚN</th>
                <th>KONFIGURACE (INT / SAFE / CH)</th>
              </tr>
            </thead>
            <tbody id="router-tbody">
              <tr>
                <td
                  colspan="8"
                  style="text-align: center; color: var(--text-dim)"
                >
                  Čekám na data...
                </td>
              </tr>
            </tbody>
          </table>

          <!-- Smart Optimizer Tips Container -->
          <div
            id="router-tips"
            style="
              margin-top: 15px;
              display: flex;
              flex-direction: column;
              gap: 8px;
            "
          ></div>
        </div>
      </div>
    </div>

    <!-- TAB: NETWORK -->
    <div id="network" class="tab-content">
      <div class="grid-2">
        <!-- WIFI CONF -->
        <div class="panel">
          <div class="panel-hdr">
            <span>WIFI PROFILY (STA)</span>
            <button class="btn-sm" onclick="startScan()" id="btn-scan">
              SKENOVAT SÍTĚ
            </button>
          </div>
          <div class="panel-body">
            <div
              id="scan-res"
              style="
                display: none;
                margin-bottom: 15px;
                max-height: 150px;
                overflow-y: auto;
                border: 1px solid var(--border);
                padding: 5px;
                background: #000;
              "
            ></div>

            <table style="margin-bottom: 20px">
              <tbody id="wifi-list"></tbody>
            </table>
            <hr
              style="
                border: 0;
                border-top: 1px solid var(--border);
                margin: 15px 0;
              "
            />
            <form onsubmit="saveWifi(event)">
              <div class="form-group">
                <label>SSID (NÁZEV SÍTĚ)</label>
                <input type="text" id="w-ssid" required />
              </div>
              <div class="form-group">
                <label>HESLO</label>
                <input type="password" id="w-pass" />
              </div>
              <button type="submit">ULOŽIT WIFI PROFIL</button>
            </form>
          </div>
        </div>

        <!-- MQTT CONF -->
        <div class="panel">
          <div class="panel-hdr">MQTT BROKER (SERVER)</div>
          <div class="panel-body">
            <table style="margin-bottom: 20px">
              <tbody id="mqtt-list"></tbody>
            </table>
            <hr
              style="
                border: 0;
                border-top: 1px solid var(--border);
                margin: 15px 0;
              "
            />
            <form onsubmit="saveMqtt(event)">
              <div class="form-group">
                <label>URI BROKERA (např. mqtt://192.168.1.100)</label>
                <input type="text" id="m-uri" required />
              </div>
              <div class="form-group">
                <label>UŽIVATELSKÉ JMÉNO</label>
                <input type="text" id="m-user" />
              </div>
              <div class="form-group">
                <label>HESLO</label>
                <input type="password" id="m-pass" />
              </div>
              <button type="submit">PŘIDAT BROKERA</button>
            </form>
            <hr
              style="
                border: 0;
                border-top: 1px solid var(--border);
                margin: 15px 0;
              "
            />
            <form onsubmit="savePubInt(event)">
              <div class="form-group">
                <label>INTERVAL PUBLIKACE DAT BRÁNY (VTEŘINY)</label>
                <div style="display: flex; gap: 10px">
                  <input type="number" id="m-pub-int" min="5" style="flex: 1" />
                  <button type="submit">POUŽÍT</button>
                </div>
              </div>
            </form>
          </div>
        </div>
      </div>
    </div>

    <!-- TAB: MESH OTA (PAGER) -->
    <div id="ota" class="tab-content">
      <div class="stat-grid" style="grid-template-columns: 1fr; margin-bottom: 20px;">
        <div class="stat-card" style="display: flex; justify-content: space-between; align-items: center; border-left: 4px solid var(--accent);">
          <div>
            <div class="stat-lbl" style="color: var(--accent)">STATUS ODESÍLÁNÍ KONFIGURACÍ</div>
            <div class="stat-val" id="ota-status-text" style="font-size: 20px">---</div>
          </div>
          <button id="btn-ota-toggle" onclick="toggleOta()" style="width: auto; padding: 10px 20px;">...</button>
        </div>
      </div>
      <div class="stat-grid" style="grid-template-columns: 1fr; margin-bottom: 20px;">
        <div
          class="stat-card"
          style="
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-left: 4px solid var(--warn);
          "
        >
          <div>
            <div class="stat-lbl" style="color: var(--warn)">
              HROMADNÁ SYNCHRONIZACE SÍTĚ
            </div>
            <div class="stat-val" style="font-size: 20px">
              AKTUÁLNÍ KANÁL BRÁNY:
              <span id="st-mch" style="color: var(--warn)">?</span>
            </div>
          </div>
          <form
            onsubmit="setGlobalChannel(event)"
            style="margin: 0; text-align: right"
          >
            <label
              style="color: var(--text-dim); display: block; margin-bottom: 5px"
              >PŘEMIGROVAT CELOU MESH SÍŤ NA NOVÝ KANÁL</label
            >
            <div style="display: flex; gap: 10px; align-items: center">
              <select
                id="o-ch"
                required
                style="width: auto; background: rgba(0, 0, 0, 0.5)"
              >
                <option value="" disabled selected>Vyberte kanál...</option>
                <option value="1">KANÁL 1</option>
                <option value="2">KANÁL 2</option>
                <option value="3">KANÁL 3</option>
                <option value="4">KANÁL 4</option>
                <option value="5">KANÁL 5</option>
                <option value="6">KANÁL 6</option>
                <option value="7">KANÁL 7</option>
                <option value="8">KANÁL 8</option>
                <option value="9">KANÁL 9</option>
                <option value="10">KANÁL 10</option>
                <option value="11">KANÁL 11</option>
              </select>
              <button
                type="submit"
                class="btn-danger"
                style="white-space: nowrap"
              >
                MIGROVAT VŠE
              </button>
              <button
                type="button"
                class="btn-sm btn-danger"
                onclick="removeOta('FFFF')"
                style="padding: 10px"
              >
                ZRUŠIT
              </button>
            </div>
          </form>
        </div>
      </div>

      <div class="panel">
        <div class="panel-hdr">INDIVIDUÁLNÍ KONFIGURACE UZLU (OTA FRONTA)</div>
        <div class="panel-body" style="padding: 0">
          <table>
            <thead>
              <tr>
                <th>ID UZLU</th>
                <th>ROLE</th>
                <th>AKTUÁLNÍ STAV</th>
                <th style="width: 30%">NOVÁ KONFIGURACE</th>
                <th style="width: 15%">AKCE</th>
              </tr>
            </thead>
            <tbody id="ota-nodes-tbody">
              <tr>
                <td colspan="5" style="text-align: center; padding: 30px; color: var(--text-dim);">
                  Čekám na síťová data...
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>
    </div>

    <!-- TAB: CONSOLE -->
    <div id="console" class="tab-content">
      <div class="panel">
        <div class="panel-hdr">ŽIVÝ VÝPIS UARTu</div>
        <div class="panel-body" style="padding: 0">
          <div id="logs"></div>
        </div>
      </div>
    </div>

    <script>
      function switchTab(t) {
        document
          .querySelectorAll(".tab-content")
          .forEach((c) => c.classList.remove("active"));
        document
          .querySelectorAll(".tab-btn")
          .forEach((b) => b.classList.remove("active"));
        document.getElementById(t).classList.add("active");
        event.target.classList.add("active");
      }

      function fmtTime(s) {
        let h = Math.floor(s / 3600),
          m = Math.floor((s % 3600) / 60),
          sec = s % 60;
        return `${h.toString().padStart(2, "0")}:${m.toString().padStart(2, "0")}:${sec.toString().padStart(2, "0")}`;
      }

      function parsePayload(stype, hex) {
        if (!hex) return ["N/A", "N/A"];
        let buf = new Uint8Array(
          hex.match(/[\da-f]{2}/gi).map((h) => parseInt(h, 16)),
        );
        let dv = new DataView(buf.buffer);
        try {
          if (stype === 1 && buf.length >= 8)
            return [
              `Teplota: ${dv.getFloat32(0, true).toFixed(1)} °C`,
              `Vlhkost: ${dv.getFloat32(4, true).toFixed(1)} %`,
            ];
          if (stype === 8 && buf.length >= 16)
            return [
              `T: ${dv.getFloat32(0, true).toFixed(1)} °C / V: ${dv.getFloat32(4, true).toFixed(1)} %`,
              `Tlak: ${dv.getFloat32(8, true).toFixed(0)} hPa / Odp.: ${dv.getFloat32(12, true).toFixed(0)} kΩ`,
            ];
          if (stype === 4 && buf.length >= 5)
            return [
              `Kouř: ${dv.getFloat32(0, true).toFixed(1)} ppm`,
              dv.getUint8(4)
                ? '<span style="color:var(--danger)">ALARM</span>'
                : '<span style="color:var(--success)">V POŘÁDKU</span>',
            ];
          if (stype === 5 && buf.length >= 5)
            return [
              `Pohyb: ${dv.getUint8(0) ? "ANO" : "NE"}`,
              `Počet: ${dv.getUint32(1, true)}`,
            ];
          if (stype === 6 && buf.length >= 8)
            return [
              `CO2: ${dv.getFloat32(0, true).toFixed(0)} ppm`,
              `VOC: ${dv.getFloat32(4, true).toFixed(0)} ppb`,
            ];
        } catch (e) {}
        return ["DATA", hex.substring(0, 16) + "..."];
      }

      async function updateData() {
        try {
          const res = await fetch("/data");
          const d = await res.json();
          globalOtaState = d.ota_en;

          document.getElementById("st-uptime").innerText = fmtTime(d.uptime_s);
          document.getElementById("st-ip").innerText = d.ip;

          if (document.activeElement !== document.getElementById("m-pub-int")) {
            document.getElementById("m-pub-int").value = d.pub_int || 10;
          }

          const mq = document.getElementById("st-mqtt");
          mq.innerText = d.mqtt ? "PŘIPOJENO" : "ODPOJENO";
          mq.style.color = d.mqtt ? "var(--success)" : "var(--danger)";

          const nb = document.getElementById("st-nbiot");
          if (nb) {
            nb.innerText = d.modem_ready ? "PŘIPRAVENO" : "OFFLINE / INIT";
            nb.style.color = d.modem_ready ? "var(--success)" : "var(--warn)";
          }

          const br = document.getElementById("st-bridge");
          br.innerText = d.bridge_ok ? "ONLINE" : "OFFLINE";
          br.style.color = d.bridge_ok ? "var(--success)" : "var(--danger)";

          if (d.mch) {
            document.getElementById("st-mch").innerText = d.mch;
          }

          const otaTxt = document.getElementById("ota-status-text");
          const otaBtn = document.getElementById("btn-ota-toggle");
          if (otaTxt && otaBtn) {
            otaTxt.innerText = d.ota_en ? "POVOLENO (AKTIVNÍ)" : "ZAKÁZÁNO (KLID)";
            otaTxt.style.color = d.ota_en ? "var(--accent)" : "var(--warn)";
            otaBtn.innerText = d.ota_en ? "VYPNOUT ODESÍLÁNÍ" : "POVOLIT ODESÍLÁNÍ";
            otaBtn.className = d.ota_en ? "btn-danger" : ""; 
            if (!d.ota_en) otaBtn.style.background = "var(--success)"; else otaBtn.style.background = "";
          }

          let otaData = null;
          try {
            const resOta = await fetch("/api/ota_queue");
            otaData = await resOta.json();
          } catch (e) {}

          let globalPending =
            otaData &&
            otaData.queue &&
            otaData.queue.find((q) => q.id === "FFFF");
          if (globalPending && d.sensors) {
            const activeNodes = d.sensors.filter((s) => s.role !== 1);
            const switched = activeNodes.filter(
              (s) => s.channel === globalPending.ch,
            ).length;
            document.getElementById("st-mch").innerHTML =
              `<span style="color:var(--danger)">MIGRUJI NA KANÁL ${globalPending.ch} (${switched}/${activeNodes.length} HOTOVO)</span>`;
          }

          if (d.sensors && d.sensors.length > 0) {
            let sCount = d.sensors.filter((s) => s.role === 3).length;
            let rCount = d.sensors.filter((s) => s.role === 2).length;

            document.getElementById("st-scount").innerText =
              sCount + " AKTIVNÍCH";
            if (document.getElementById("st-rcount"))
              document.getElementById("st-rcount").innerText =
                rCount + " AKTIVNÍCH";

            let htmlSensor = "";
            let htmlRouter = "";
            let otaHtml = "";

            d.sensors
              .sort((a, b) => {
                if (a.role !== b.role) return a.role - b.role;
                return a.id.localeCompare(b.id);
              })
              .forEach((s) => {
                let role =
                  s.role === 1 ? "BRÁNA" : s.role === 2 ? "SMĚROVAČ" : "SENZOR";
                let badge =
                  s.role === 2 ? "b-red" : s.role === 1 ? "b-green" : "";
                // --- LOGIKA STAVOVÉ TEČKY ---
                let interval = s.sleep_int || 60;
                let limit = interval * 1.2; 
                let isMissed = s.ago > limit;
                let dotClass = isMissed ? "dot-missed" : "dot-online";
                let statusDot = `<span class="status-dot ${dotClass}"></span>`;
                
                let agoStyle = s.ago > limit ? "color:var(--warn); font-weight:bold;" : "";
                let data = parsePayload(s.stype, s.pay);

                // RELIABILITY (PDR) CALCULATION
                let relPercent = 100;
                let totalExpected = s.r + s.l; // Použití zkrácených klíčů z JSONu
                if (totalExpected > 0) {
                  relPercent = (100 * s.r) / totalExpected;
                }
                let relColor = "var(--success)";
                if (relPercent < 98) relColor = "var(--warn)";
                if (relPercent < 90) relColor = "var(--danger)";
                let relHtml = `<div style="color:${relColor}; font-weight:bold; font-size:13px;">${relPercent.toFixed(1)}%</div>
                               <div style="font-size:10px; color:var(--text-dim)">${s.r} / ${s.l}</div>`;

                let isPending =
                  otaData &&
                  otaData.queue &&
                  otaData.queue.find((q) => q.id === s.id);
                let bgStyle = isPending
                  ? "background: rgba(245, 158, 11, 0.05);"
                  : "";
                let htmlSleep = s.sleep_int > 0 ? s.sleep_int + "s" : "?";
                let htmlSafe = s.safe > 0 ? s.safe + "s" : "?";
                let htmlCh = s.channel > 0 ? "CH " + s.channel : "?";

                let optHtml = "";
                if (s.opt_needs) {
                  optHtml = `<div style="margin-top:4px; font-size:11px; color:var(--warn); display:flex; align-items:center; gap:5px;">
                    <span title="Doporučený interval pro synchronizaci s dětmi">Tip: ${s.opt_rec}s</span>
                    <button onclick="applyOpt('${s.id}', ${s.opt_rec})" style="padding:1px 4px; font-size:9px; background:var(--warn); color:black; border:none; border-radius:3px; cursor:pointer; font-weight:bold;">SYNC</button>
                  </div>`;
                }

                let rssiColor =
                  s.rssi > -70
                    ? "var(--success)"
                    : s.rssi > -85
                      ? "var(--warn)"
                      : "var(--danger)";

                if (s.role === 2) {
                  // ROUTER
                  const childrenArr = d.sensors.filter(
                    (child) => child.role === 3 && child.relay === s.id,
                  );
                  let childrenStr =
                    '<span style="color:var(--text-dim)">Žádní</span>';
                  if (childrenArr.length > 0) {
                    childrenStr = childrenArr
                      .map(
                        (c) =>
                          `<strong style="color:var(--text-light); font-size:13px;">${c.id}</strong>`,
                      )
                      .join(", ");
                  }
                  htmlRouter += `<tr style="${bgStyle}">
                        <td>${statusDot}<span class="badge ${badge}">${s.id}</span></td>
                        <td>${s.relay === "0000" || !s.relay ? "PŘÍMO" : s.relay} (Hopy ${s.hops})</td>
                        <td>${childrenStr}</td>
                        <td>${relHtml}</td>
                        <td><span style="color:${rssiColor}; font-weight:500;">${s.rssi} dBm</span></td>
                        <td>${s.batt ? s.batt.toFixed(2) + " V" : "N/A"}</td>
                        <td style="${agoStyle}">před ${s.ago}s</td>
                        <td style="color:${isPending ? "var(--warn)" : "var(--text-dim)"}">
                          <div>${htmlSleep} / ${htmlSafe} / ${htmlCh}</div>
                          ${optHtml}
                        </td>
                    </tr>`;
                } else if (s.role === 3) {
                  // SENSOR
                  let sensorRssiHtml = `<span style="color:${rssiColor}; font-weight:500;">${s.rssi} dBm</span>`;
                  if (s.hops > 0) {
                    sensorRssiHtml += `<div style="font-size:10px; color:var(--text-dim); margin-top:2px;">link k bráně: ${s.lrssi} dBm</div>`;
                  }
                  htmlSensor += `<tr style="${bgStyle}">
                        <td>${statusDot}<span class="badge ${badge}">${s.id}</span></td>
                        <td>${s.relay === "0000" || !s.relay ? "PŘÍMO" : s.relay} (Hopy ${s.hops})</td>
                        <td>${relHtml}</td>
                        <td>${sensorRssiHtml}</td>
                        <td>${s.batt.toFixed(2)} V</td>
                        <td style="${agoStyle}">před ${s.ago}s</td>
                        <td style="color:${isPending ? "var(--warn)" : "var(--text-dim)"}">${htmlSleep} / ${htmlSafe} / ${htmlCh}</td>
                        <td style="color:var(--accent)">${data[0]}</td>
                        <td style="color:var(--accent)">${data[1]}</td>
                    </tr>`;
                }

                // OTA TAB ROW
                if (s.role !== 1) {
                  if (isPending) {
                    otaHtml += `<tr style="${bgStyle}">
                            <td><span class="badge ${badge}">${s.id}</span></td>
                            <td>${role}</td>
                            <td style="color:var(--text-dim)">${htmlSleep} / ${htmlSafe} / ${htmlCh}</td>
                            <td>
                                <strong style="color:var(--warn)">ČEKÁM NA SYNCHRO...</strong><br>
                                <span style="font-size:11px; color:var(--text-dim)">CÍL: ${isPending.sleep || s.sleep_int}s / ${isPending.safe || s.safe}s / CH ${isPending.ch || s.channel}</span>
                            </td>
                            <td><button class="btn-sm btn-danger" onclick="removeOta('${s.id}')">ZRUŠIT</button></td>
                        </tr>`;
                  } else {
                    otaHtml += `<tr>
                            <td><span class="badge ${badge}">${s.id}</span></td>
                            <td>${role}</td>
                            <td style="color:var(--success); font-weight:bold;">${htmlSleep} / ${htmlSafe} / ${htmlCh}</td>
                            <td>
                                <div style="display:flex; gap:5px;">
                                    <input type="number" id="oi-${s.id}" placeholder="Sec" style="width:70px; padding:6px;" value="${s.sleep_int}">
                                    <input type="number" id="os-${s.id}" placeholder="Safe" style="width:70px; padding:6px;" value="${s.safe}">
                                    <select id="oc-${s.id}" style="width:70px; padding:6px;">
                                        ${[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11].map((c) => `<option value="${c}" ${c === s.channel ? "selected" : ""}>CH ${c}</option>`).join("")}
                                    </select>
                                </div>
                            </td>
                            <td><button class="btn-sm" onclick="setOtaNode('${s.id}')" style="background:var(--panel); border:1px solid var(--border); color:var(--text);">DO FRONTY</button></td>
                        </tr>`;
                  }
                }
              });

            document.getElementById("sensor-tbody").innerHTML =
              htmlSensor ||
              '<tr><td colspan="9" style="text-align:center; padding:20px; color:var(--text-dim)">Nebyly detekovány žádné aktivní senzory.</td></tr>';
            if (document.getElementById("router-tbody")) {
              document.getElementById("router-tbody").innerHTML =
                htmlRouter ||
                '<tr><td colspan="8" style="text-align:center; padding:20px; color:var(--text-dim)">Nebyly detekovány žádné aktivní směrovače.</td></tr>';
            }

            // UX Tweak: Nepřepisovat OTA tabulku, pokud je uživatel uvnitř inputů
            let active = document.activeElement
              ? document.activeElement.id
              : "";
            let isTyping =
              active &&
              (active.startsWith("oi-") ||
                active.startsWith("os-") ||
                active.startsWith("oc-") ||
                active === "o-ch");

            if (!isTyping) {
              document.getElementById("ota-nodes-tbody").innerHTML =
                otaHtml ||
                '<tr><td colspan="5" style="text-align:center; padding:20px; color:var(--text-dim)">Žádné vzdálené uzly ke konfiguraci.</td></tr>';
            }
          } else {
            document.getElementById("st-scount").innerText = "0 AKTIVNÍCH";
            if (document.getElementById("st-rcount"))
              document.getElementById("st-rcount").innerText = "0 AKTIVNÍCH";
            document.getElementById("sensor-tbody").innerHTML =
              '<tr><td colspan="9" style="text-align:center; padding:20px; color:var(--text-dim)">Nebyly detekovány žádné aktivní senzory.</td></tr>';
            if (document.getElementById("router-tbody"))
              document.getElementById("router-tbody").innerHTML =
                '<tr><td colspan="8" style="text-align:center; padding:20px; color:var(--text-dim)">Nebyly detekovány žádné aktivní směrovače.</td></tr>';
            document.getElementById("ota-nodes-tbody").innerHTML =
              '<tr><td colspan="5" style="text-align:center; padding:20px; color:var(--text-dim)">Nebyly detekovány žádné aktivní uzly.</td></tr>';
          } // Lists Update
          if (d.nvs) {
            let wHtml = "";
            d.nvs.wifi.forEach((w, i) => {
              wHtml += `<tr>
                <td><strong>${w.ssid}</strong></td>
                <td style="text-align:right">
                  ${i > 0 ? `<button class="btn-sm" style="background:var(--panel);color:var(--text);border:1px solid var(--border)" onclick="reorderWifi(${i}, ${i - 1})">&uarr;</button>` : ""}
                  ${i < d.nvs.wifi.length - 1 ? `<button class="btn-sm" style="background:var(--panel);color:var(--text);border:1px solid var(--border)" onclick="reorderWifi(${i}, ${i + 1})">&darr;</button>` : ""}
                  <button class="btn-sm btn-danger" style="margin-left:5px" onclick="delWifi('${encodeURIComponent(w.ssid)}')">SMAZAT</button>
                </td>
              </tr>`;
            });
            document.getElementById("wifi-list").innerHTML =
              wHtml ||
              '<tr><td style="color:var(--text-dim)">Žádné Wi-Fi profily</td></tr>';

            let mHtml = "";
            d.nvs.mqtt.forEach((m, i) => {
              mHtml += `<tr>
                <td><strong>${m.uri}</strong></td>
                <td style="text-align:right">
                  ${i > 0 ? `<button class="btn-sm" style="background:var(--panel);color:var(--text);border:1px solid var(--border)" onclick="reorderMqtt(${i}, ${i - 1})">&uarr;</button>` : ""}
                  ${i < d.nvs.mqtt.length - 1 ? `<button class="btn-sm" style="background:var(--panel);color:var(--text);border:1px solid var(--border)" onclick="reorderMqtt(${i}, ${i + 1})">&darr;</button>` : ""}
                  <button class="btn-sm btn-danger" style="margin-left:5px" onclick="delMqtt(${i})">SMAZAT</button>
                </td>
              </tr>`;
            });
            document.getElementById("mqtt-list").innerHTML =
              mHtml ||
              '<tr><td style="color:var(--text-dim)">Žádné MQTT profily</td></tr>';
          }

          // --- SMART ROUTER OPTIMIZER (GCD Linter) ---
          const routers = (d.sensors || []).filter((s) => s.role === 2);
          const tipsContainer = document.getElementById("router-tips");
          if (tipsContainer) {
            let tipsHtml = "";
            routers.forEach((r) => {
              const childSensors = (d.sensors || []).filter(
                (s) => s.role === 3 && s.relay === r.id,
              );
              if (childSensors.length > 0) {
                const intervals = childSensors
                  .map((s) => s.sleep_int)
                  .filter((i) => i > 0);
                if (intervals.length > 0) {
                  const rGcd = intervals.reduce((a, b) => {
                    let x = a,
                      y = b;
                    while (y) {
                      let t = y;
                      y = x % y;
                      x = t;
                    }
                    return x;
                  });
                  if (rGcd % r.sleep_int !== 0) {
                    const uniqueIntervals = [...new Set(intervals)]
                      .sort((a, b) => a - b)
                      .join("s, ") + "s";
                    tipsHtml += `
                                  <div style="background: rgba(46, 204, 113, 0.08); border-left: 4px solid var(--accent); padding: 12px 16px; border-radius: 6px; color: var(--text-dim); display: flex; align-items: center; gap: 15px; box-shadow: 0 4px 15px rgba(0,0,0,0.1); margin-bottom: 10px;">
                                      <div style="font-size: 20px; text-shadow: 0 0 10px rgba(46,204,113,0.4);">💡</div>
                                      <div>
                                          <strong style="color: var(--text-light); letter-spacing: 0.5px;">CHYTRÝ TIP PRO SMĚROVAČ ${r.id}</strong><br>
                                          <span style="font-size: 13px;">
                                              Senzory na tomto směrovači usínají po: <b>${uniqueIntervals}</b>.<br>
                                              Změňte OTA interval směrovače z <b>${r.sleep_int}s</b> na společný dělitel <b><span style="color:var(--accent); font-weight:bold;">${rGcd}s</span></b> pro maximální prodloužení výdrže jeho baterie!
                                          </span>
                                      </div>
                                  </div>
                              `;
                  }
                }
              }
            });
            tipsContainer.innerHTML = tipsHtml;
          }

          document.getElementById("live-ind").style.opacity = "1";
          setTimeout(
            () => (document.getElementById("live-ind").style.opacity = "0.5"),
            500,
          );
        } catch (e) {
          console.error(e);
        }
      }

      async function updateLogs() {
        if (document.getElementById("console").classList.contains("active")) {
          try {
            const res = await fetch("/logs");
            const d = await res.json();
            let div = document.getElementById("logs");
            div.innerHTML = (d.logs || [])
              .map((l) => `<div>> ${l}</div>`)
              .join("");
            div.scrollTop = div.scrollHeight;
          } catch (e) {}
        }
      }

      async function saveWifi(e) {
        e.preventDefault();
        await fetch("/save_wifi", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `ssid=${encodeURIComponent(document.getElementById("w-ssid").value)}&pass=${encodeURIComponent(document.getElementById("w-pass").value)}`,
        });
        document.getElementById("w-ssid").value = "";
        document.getElementById("w-pass").value = "";
        updateData();
      }

      async function delWifi(ssid) {
        if (!confirm("Smazat Wi-Fi profil?")) return;
        await fetch("/delete_wifi", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `ssid=${ssid}`,
        });
        updateData();
      }

      async function reorderWifi(f, t) {
        await fetch("/reorder_wifi", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `f=${f}&t=${t}`,
        });
        updateData();
      }

      async function saveMqtt(e) {
        e.preventDefault();
        await fetch("/save_mqtt", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `uri=${encodeURIComponent(document.getElementById("m-uri").value)}&user=${encodeURIComponent(document.getElementById("m-user").value)}&pass=${encodeURIComponent(document.getElementById("m-pass").value)}`,
        });
        document.getElementById("m-uri").value = "";
        document.getElementById("m-user").value = "";
        document.getElementById("m-pass").value = "";
        updateData();
      }

      async function delMqtt(idx) {
        if (!confirm("Smazat MQTT brokera?")) return;
        await fetch("/delete_mqtt", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `index=${idx}`,
        });
        updateData();
      }

      async function reorderMqtt(f, t) {
        await fetch("/reorder_mqtt", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `f=${f}&t=${t}`,
        });
        updateData();
      }

      async function savePubInt(e) {
        e.preventDefault();
        await fetch("/api/set_pub_int", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `interval=${document.getElementById("m-pub-int").value}`,
        });
        updateData();
      }

      async function startScan() {
        const btn = document.getElementById("btn-scan");
        btn.innerText = "SKENUJI...";
        btn.disabled = true;
        document.getElementById("scan-res").style.display = "block";
        document.getElementById("scan-res").innerHTML =
          "<div style='color:var(--text-dim)'>Skenuji okolní Wi-Fi sítě...</div>";
        await fetch("/scan");
        setTimeout(pollScan, 2000);
      }

      async function pollScan() {
        try {
          const res = await fetch("/scan_res");
          const d = await res.json();
          if (!d.ready) {
            setTimeout(pollScan, 1000);
            return;
          }
          let html = "<table style='width:100%;font-size:12px;'>";
          if (d.networks && d.networks.length > 0) {
            d.networks
              .sort((a, b) => b.rssi - a.rssi)
              .forEach((n) => {
                html += `<tr>
                <td><strong>${n.ssid}</strong></td>
                <td style="text-align:right;color:${n.rssi > -70 ? "var(--success)" : "var(--warn)"}">${n.rssi} dBm</td>
                <td style="width:40px;text-align:right"><button class="btn-sm" onclick="document.getElementById('w-ssid').value='${n.ssid}'">POUŽÍT</button></td>
              </tr>`;
              });
          } else {
            html += "<tr><td colspan='3'>Nebyly nalezeny žádné sítě.</td></tr>";
          }
          html += "</table>";
          document.getElementById("scan-res").innerHTML = html;
          const btn = document.getElementById("btn-scan");
          btn.innerText = "SKENOVAT SÍTĚ";
          btn.disabled = false;
        } catch (e) {
          setTimeout(pollScan, 1000);
        }
      }

      async function setOtaNode(id) {
        const interval = document.getElementById("oi-" + id).value;
        const safe = document.getElementById("os-" + id).value;
        const channel = document.getElementById("oc-" + id).value;
        await fetch("/api/set_ota", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `id=${encodeURIComponent(id)}&interval=${encodeURIComponent(interval)}&safe=${encodeURIComponent(safe)}&channel=${encodeURIComponent(channel)}`,
        });
        updateData();
      }

      async function setGlobalChannel(e) {
        e.preventDefault();
        const channel = document.getElementById("o-ch").value;
        if (
          !confirm(
            `VAROVÁNÍ: Tato akce změní kanál pro VŠECHNY známé uzly v síti na kanál ${channel}.\n\nBěhem přechodu může být síť nestabilní. Jste si naprosto jisti?`,
          )
        )
          return;

        await fetch("/api/set_ota", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `id=FFFF&interval=0&safe=0&channel=${encodeURIComponent(channel)}`,
        });
        document.getElementById("o-ch").value = "";
        updateData();
      }

      function applyOpt(id, val) {
        if (!confirm("Přejete si optimalizovat interval routeru " + id + " na " + val + "s?")) return;
        fetch("/ota", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: "id=" + id + "&interval=" + val,
        }).then((r) => {
          if (r.ok) {
            alert("Optimalizace zařazena do fronty");
            updateData();
          } else {
            alert("Chyba při odesílání konfigurace");
          }
        });
      }

      async function removeOta(id) {
        if (
          !confirm(
            `Opravdu chcete odstranit čekající konfiguraci pro uzel ${id}?`,
          )
        )
          return;
        await fetch("/api/remove_ota", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `id=${encodeURIComponent(id)}`,
        });
        updateData();
      }

      async function resetStats() {
        if (
          !confirm(
            "Opravdu chcete vynulovat veškeré statistiky doručení (PDR)?",
          )
        )
          return;
        await fetch("/api/reset_stats", { method: "POST" });
        showToast("Statistiky byly vynulovány.");
        updateData();
      }

      async function setPubInt() {
        const val = document.getElementById("m-pub-int").value;
        await fetch("/api/set_pub_int", {
          method: "POST",
          headers: { "Content-Type": "application/x-www-form-urlencoded" },
          body: `interval=${encodeURIComponent(val)}`,
        });
        showToast("Interval odesílání uložen.");
      }

      var globalOtaState = true;
      async function toggleOta() {
          const newState = !globalOtaState;
          await fetch("/api/ota_control", {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify({ enabled: newState })
          });
          updateData();
      }

      setInterval(updateData, 2500);
      setInterval(updateLogs, 2500);
      updateData();
    </script>
  </body>
</html>
)rawliteral";
