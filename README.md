# ESP-NOW mesh síť 🇨🇿 🇸🇰

Tento repozitář obsahuje zdrojové kódy k diplomové práci, která se zabývá návrhem a realizací nízkoenergetické, hierarchické bezdrátové mesh sítě pomocí protokolu ESP-NOW.

Cílem práce bylo zkonstruovat systém schopný dlouhodobého bateriového chodu. Toho je dosaženo synchronizací spánkových režimů (deep sleep), snížením frekvence periodického probouzení, využitím mechanismu časové disperze a softwarovou EMA kompenzací driftu RC oscilátoru.

## Struktura repozitáře

- `Sensor_FW`: Firmware koncového senzorového uzlu (ESP32-C3). Provede měření I2C senzorem BME688, odešle hodnoty přes ESP-NOW a přejde do hlubokého spánku.
- `Router_FW`: Firmware směrovacího uzlu (ESP32-WROOM-32D/U). Tvoří páteř mesh sítě. Pomocí dvoujádrové asymetrické obsluhy zachytává data z downlinku a přeposílá je uplinkem s automatickým přeposláním.
- `Gateway_FW` a `Receiver_FW`: Části duální brány rozdělující komunikaci na dva mikrokontroléry přes UART. Gateway_FW zpracovává připojení do internetu přes TCP/IP (Wi-Fi 802.11b/g/n nebo sériový NB-IoT modem T-SIM7000G) potažmo MQTT s připojeným vlastním serverem, Receiver_FW přijímá ESP-NOW komunikaci.
- `Shared`: Složka se sdíleným komunikačním protokolem a knihovnami.
- `Laborka`: Sada zdrojů pro úlohu do výuky (konfigurace odesílatel, příjemce a sdílený router).
- `RPi_web_etc`: Konfigurační soubory a skripty pro nadřazený aplikační/webový server běžící na Raspberry Pi (např. logování).
- `3D`: Obsahuje 3D modely tisknutých krabiček ve formátu `.stl`

## Použitý hardware

- ESP32-C3-SIL0074-S, ESP32-WROOM-32D/U, LilyGO T-SIM7000G
- Senzor Bosch BME688
- LDO stabilizátory napětí a zdroje: RT9080-33, automatický Buck-Boost TPS63020, atd.

## Autor

Bc. Pavel Šiller (2026)

**Usnesení:** Tento projekt byl vytvořen primárně pro účely vysokoškolské diplomové práce. Zdrojové kódy a hardwarové návrhy jsou poskytovány „tak jak jsou“ bez záruky vhodnosti pro produkční využití nebo průmyslové nasazení. Autor nenese odpovědnost za případné škody způsobené jejich použitím. Uvedená řešení mohou sloužit jako vzdělávací demonstrace a inspirace pro vývoj IoT systémů.

---

# ESP-NOW mesh network 🇬🇧 🇺🇸

This repository contains the source code for a master's thesis focused on the design and implementation of a low-power, hierarchical wireless mesh network using the ESP-NOW protocol.

The system is engineered for autonomous, long-term battery operation. This is achieved by synchronizing deep sleep modes, reducing wakeup multi-node packet collision through time dispersion, and employing a software-based EMA (Exponential Moving Average) compensation for hardware RC oscillator drift.

## Repository Structure

- `Sensor_FW`: End sensor node firmware (ESP32-C3). Reads environmental data via a BME688 I2C sensor, transmits it over ESP-NOW, and returns to deep sleep.
- `Router_FW`: Router node firmware (ESP32-WROOM-32D/U). Acts as the network's backbone, asynchronously capturing uplink data and forwarding it to the gateway layer.
- `Gateway_FW` & `Receiver_FW`: A dual-MCU gateway architecture connected via UART. `Receiver_FW` handles low-level ESP-NOW reception, while `Gateway_FW` manages the TCP/IP uplink (via Wi-Fi or a T-SIM7000G NB-IoT modem) translating data to MQTT.
- `Shared`: Common communication protocol structures and C libraries.
- `Laborka`: Educational resources and mini-projects intended for university laboratory classes.
- `RPi_web_etc`: Server configuration scripts for a Raspberry Pi host.
- `3D`: 3D printable `.stl` models for the custom hardware enclosures.

## Used hardware

- ESP32-C3-SIL0074-S, ESP32-WROOM-32D/U, LilyGO T-SIM7000G
- Sensor Bosch BME688
- LDO and add. HW: RT9080-33, Buck-Boost TPS63020, etc.

## Author

Bc. Pavel Siller (2026)

**Disclaimer:** This project was created primarily for the purposes of a university master's thesis. Source codes and hardware designs are provided "as is" without warranty of fitness for production use or industrial deployment. The author takes no responsibility for any damages caused by their use. The provided solutions serve as educational demonstrations and inspiration for developing IoT systems.
