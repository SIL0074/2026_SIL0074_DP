#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

/* 
 *  ==========================================================================
 *  LABORATORNI ULOHA: Komunikace v mesh siti pomoci protokolu ESP-NOW
 *  Bc. Siller Pavel - Uzel A - Vysilac
 *  ==========================================================================
 *
 *  POSTUP:
 *  Ukol 1: Doplnte MAC adresu Uzlu B a zaregistrujte ho jako peera.
 *  Ukol 2: Odkomentujte radky pro simulaci ruseni (snizeni vykonu).
 *  Ukol 3: V callbacku OnDataSent doplnte presilani pres Router C.
 */

// Konfigurace GPIO pro vasi vyvojovou desku
#define BUTTON_GPIO 0  // Standardni BOOT tlacitko na ESP32 (GPIO 0)

// ---------------------------------------------------------------------------
// Spolecna datova struktura (musi byt STEJNA na vsech uzlech v laboratori!)
//
// Obsahuje dest_mac - Router C se z teto adresy dozvi, kam data poslat.
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t dest_mac[6]; // MAC adresa ciloveho Uzlu B (vyplni Uzel A)
    uint8_t stav;        // 0 = tlacitko pusteno, 1 = tlacitko stisknuto
    uint8_t hop_count;   // Pocet skoku (0 = prime spojeni, 1 = pres router)
} lab_packet_t;

// ---------------------------------------------------------------------------
// TODO UKOL 1: Doplnte MAC adresu vaseho Uzlu B (zjistite ji ze serioveho
//              monitoru Uzlu B po jeho spusteni).
// ---------------------------------------------------------------------------
uint8_t mac_prijimac_B[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// MAC adresa Routeru C - dostanete ji od cviciciho.
uint8_t mac_router_C[]   = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

lab_packet_t muj_paket;

// Callback po odeslani dat - vola se automaticky po kazdем esp_now_send()
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (memcmp(mac_addr, mac_prijimac_B, 6) == 0) {
        if (status == ESP_NOW_SEND_SUCCESS) {
            Serial.println("UZEL_A: Zprava uspesne dorucena primo k Uzlu B.");
        } else {
            Serial.println("UZEL_A: Vypadek prime cesty k B. Prepinam na Router C...");

            // --- UKOL 3: ZALOZNI CESTA PRES SMEROVAC C ---
            // Router C si z paketu sam precte dest_mac a vi, kam data preposlat.
            // Nas ukol je jen vlozit spravny dest_mac do struktury a poslat na router.
            //
            // TODO: Odkomentujte a doplnte nasledujici kod:
            //
            // muj_paket.hop_count = 1; // Inkrementujte pocitadlo skoku
            // esp_now_send(mac_router_C, (uint8_t *)&muj_paket, sizeof(muj_paket));
        }
    } 
    else if (memcmp(mac_addr, mac_router_C, 6) == 0) {
        if (status == ESP_NOW_SEND_SUCCESS) {
            Serial.println("UZEL_A: Router C uspesne prevzal data k preposilani.");
        } else {
            Serial.println("UZEL_A: Chyba - ani Router C neni dostupny!");
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("UZEL_A: System startuje...");

    // Nastaveni WiFi v rezimu Station (nutne pro ESP-NOW)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // --- UKOL 2: SIMULACE PREKAZKY (odkomentujte oba radky najednou) ---
    // Snizi vysilaci vykon a zvysi prenosovou rychlost -> drasticky zkrati dosah.
    // WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);
    // esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_54M);

    // Vypis vlastni MAC adresy (sdelte ji kolegum)
    Serial.print("UZEL_A: MOJE MAC ADRESA: ");
    Serial.println(WiFi.macAddress());

    // Inicializace ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("UZEL_A: Chyba pri inicializaci ESP-NOW!");
        return;
    }

    // Registrace callbacku pro odesilani
    esp_now_register_send_cb(OnDataSent);

    // --- UKOL 1: REGISTRACE SOUSEDU ---
    // Zaregistrujte Uzel B jako cilovy peer.
    //
    // TODO: Odkomentujte a doplnte nasledujici blok:
    //
    // esp_now_peer_info_t peerB = {};
    // memcpy(peerB.peer_addr, mac_prijimac_B, 6);
    // peerB.channel = 0; // 0 = aktualni kanal
    // peerB.encrypt = false;
    // if (esp_now_add_peer(&peerB) != ESP_OK) {
    //     Serial.println("UZEL_A: Chyba pri registraci Uzlu B!");
    // }
    //
    // Zaregistrujte take Router C (dostanete MAC od pedagoga):
    //
    // esp_now_peer_info_t peerC = {};
    // memcpy(peerC.peer_addr, mac_router_C, 6);
    // peerC.channel = 0;
    // peerC.encrypt = false;
    // esp_now_add_peer(&peerC);

    // Predpripravte dest_mac v paketu - router C z nej vi, kam preposlat
    memcpy(muj_paket.dest_mac, mac_prijimac_B, 6);
    muj_paket.hop_count = 0;

    // Konfigurace tlacitka
    pinMode(BUTTON_GPIO, INPUT_PULLUP);
    Serial.println("UZEL_A: Pripraven. Stisknete tlacitko pro odeslani dat.");
}

int last_state = HIGH;

void loop() {
    int current_state = digitalRead(BUTTON_GPIO);
    
    if (current_state != last_state) {
        last_state = current_state;
        muj_paket.stav = (current_state == LOW) ? 1 : 0;
        
        if (muj_paket.stav == 1) {
            Serial.println("UZEL_A: Tlacitko stisknuto. Odesilam na Uzel B...");
            esp_now_send(mac_prijimac_B, (uint8_t *)&muj_paket, sizeof(muj_paket));
        }
        
        delay(100);
    }
    delay(10);
}
