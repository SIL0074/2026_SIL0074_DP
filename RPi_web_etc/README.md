# ESP-NOW dashboard

Tato složka obsahuje zdrojové kódy a konfigurační soubory pro systém monitorování senzorů a směrovačů.

## Obsah složky
- `web/` - Zdrojové kódy webového rozhraní (HTML, CSS, JS).
- `telegraf/` - Konfigurace Telegrafu pro sběr dat.
- `docker-compose.yml` - Definice dockerovského prostředí (InfluxDB, Telegraf, Nginx, Mosquitto).
- `mosquitto.conf` - Konfigurace MQTT brokera.
- `charging_detector.py` - Skript pro detekci nabíjení uzlů.
- `fix_sparklines.py` a `check.js` - Pomocné skripty pro údržbu rozhraní.

## Instalace a spuštění
1. Ujistěte se, že máte nainstalovaný **Docker** a **Docker Compose**.
2. Upravte konfigurační soubory (zejména `docker-compose.yml` a `telegraf/telegraf.conf`), kde nahradíte zástupné texty (`YOUR_PASSWORD`, `YOUR_TOKEN` atd.) reálnými údaji.
3. Spusťte systém příkazem:
   ```bash
   docker-compose up -d
   ```
4. Webové rozhraní bude dostupné na portu 80 (nebo dle nastavení v `docker-compose.yml`).

## Poznámky
Před nasazením je nutné doplnit přístupové údaje a IP adresy dle reálného prostředí.
