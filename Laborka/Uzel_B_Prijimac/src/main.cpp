#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

/* 
 *  ==========================================================================
 *  LABORATORNI ULOHA: Komunikace v mesh siti pomoci protokolu ESP-NOW
 *  Bc. Siller Pavel - Uzel B - Prijimac
 *  ==========================================================================
 *
 *  POSTUP:
 *  Ukol 1: Spustte uzel, zjistete svoji MAC adresu a sdelte ji kolegovi s Uzlem A.
 *  Ukol 3: Doplnte kod v OnDataRecv pro signalizaci LED podle poctu skoku (hops).
 */

// Konfigurace GPIO pro LED signalizaci
#define LED_GPIO 2  // Standardni LED na ESP32 (GPIO 2 - Active High)

// ---------------------------------------------------------------------------
// Spolecna datova struktura (musi byt STEJNA na vsech uzlech v laboratori!)
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t dest_mac[6]; // MAC adresa tohoto uzlu (vyplneno Uzlem A, nas nepouziva)
    uint8_t stav;        // 0 = tlacitko pusteno, 1 = tlacitko stisknuto
    uint8_t hop_count;   // Pocet skoku (0 = prime spojeni, 1 = pres router)
} lab_packet_t;

// Pomocna funkce pro blikani LED
void blink(int count, int ms) {
    for (int i = 0; i < count; i++) {
        digitalWrite(LED_GPIO, HIGH);
        delay(ms);
        digitalWrite(LED_GPIO, LOW);
        delay(ms);
    }
}

// Callback pri prijmu dat
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (len == sizeof(lab_packet_t)) {
        lab_packet_t *pkt = (lab_packet_t *)incomingData;
        
        Serial.printf("UZEL_B: Zprava prijata! Stav: %d, Pocet skoku: %d\n",
                      pkt->stav, pkt->hop_count);

        if (pkt->stav == 1) {
            // --- UKOL 3: SIGNALIZACE CESTY PAKETU LED ---
            // Bliknete ruznymi pocty podle toho, kudy paket prisel:
            //   - Prime spojeni (hop_count == 0) -> 1 bliknuti
            //   - Pres router   (hop_count == 1) -> 2 bliknuti
            //
            // TODO: Doplnte podminkovy blok if/else a volani blink():
            //
            // if (pkt->hop_count == 0) {
            //     Serial.println("UZEL_B: Prime spojeni - 1 bliknuti.");
            //     blink(1, 200);
            // } else {
            //     Serial.println("UZEL_B: Smerovane pres Router C - 2 bliknuti.");
            //     blink(2, 200);
            // }
        }
    } else {
        Serial.printf("UZEL_B: Prijat paket neznameho formatu (delka: %d B).\n", len);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("UZEL_B: System startuje...");

    // Nastaveni LED (Active High)
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LOW);

    // Nastaveni WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Vypis vlastni MAC adresy - sdelte ji kolegovi s Uzlem A!
    Serial.print("UZEL_B: MOJE MAC ADRESA: ");
    Serial.println(WiFi.macAddress());

    // Inicializace ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("UZEL_B: Chyba pri inicializaci ESP-NOW!");
        return;
    }

    // Registrace callbacku pro prijem - zadna jina konfigurace neni nutna
    esp_now_register_recv_cb(OnDataRecv);

    Serial.println("UZEL_B: Uzel je aktivni a ceka na prichozi data...");
}

void loop() {
    delay(1000);
}
