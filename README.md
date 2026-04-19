# ESP-NOW mesh síť

Tento repozitář obsahuje zdrojové kódy k diplomové práci, která se zabývá návrhem a realizací nízkoenergetické, hierarchické bezdrátové mesh sítě pomocí protokolu ESP-NOW.

Cílem práce bylo zkonstruovat systém schopný dlouhodobého bateriového chodu. Toho je dosaženo synchronizací spánkových režimů (deep sleep), snížením frekvence periodického probouzení, využitím mechanismu časové disperze a softwarovou EMA kompenzací driftu RC oscilátoru.

## Struktura repozitáře

- `Sensor_FW`: Firmware koncového senzorového uzlu (ESP32-C3). Provede měření I2C senzorem BME688, odešle hodnoty přes ESP-NOW a přejde do hlubokého spánku.
- `Router_FW`: Firmware směrovacího uzlu (ESP32-WROOM-32D/U). Tvoří páteř mesh sítě. Pomocí dvoujádrové asymetrické obsluhy zachytává data z downlinku a přeposílá je uplinkem s automatickým přeposláním.
- `Gateway_FW` a `Receiver_FW`: Části duální brány rozdělující komunikaci na dva mikrokontroléry přes UART. Gateway_FW zpracovává připojení do internetu přes TCP/IP (Wi-Fi 802.11b/g/n nebo sériový NB-IoT modem T-SIM7000G) potažmo MQTT s připojeným vlastním serverem, Receiver_FW přijímá ESP-NOW komunikaci.
- `Shared`: Složka se sdíleným komunikačním protokolem a knihovnami.
- `Laborka`: Sada zdrojů pro úlohu do výuky (konfigurace odesílatel, příjemce a sdílený router).
- `RPi_web_etc`: Konfigurační soubory a skripty pro nadřazený aplikační/webový server běžící na Raspberry Pi (např. logování).
- `3D`: Obsahuje 3D modely tisknutých krabiček.

## Použitý hardware

- ESP32-C3-SIL0074-S, ESP32-WROOM-32D/U, LilyGO T-SIM7000G
- Senzor Bosch BME688
- LDO stabilizátory napětí a zdroje: RT9080-33, automatický Buck-Boost TPS63020, atd.

## Autor

Bc. Pavel Šiller (2026)
