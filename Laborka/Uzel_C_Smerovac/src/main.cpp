#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <string.h>

/* 
 *  ==========================================================================
 *  LABORATORNI ULOHA: Komunikace v mesh siti pomoci protokolu ESP-NOW
 *  Bc. Siller Pavel - Uzel C - Smerovac (spravuje cvicici)
 *  ==========================================================================
 *
 *  Tento uzel je PRED-NAFLASHOVANY cvicicim a studenti ho neprogramuji.
 *
 *  Princip: Router nevi dopredu MAC adresu zadneho Uzlu B. Misto toho
 *  ji precte z pole dest_mac v prijatém paketu od Uzlu A a dynamicky
 *  si cilovy peer zaregistruje. Funguje pro libovolny pocet skupin
 *  bez nutnosti reflashovani.
 */

// ---------------------------------------------------------------------------
// Spolecna datova struktura (musi byt STEJNA na vsech uzlech v laboratori!)
// ---------------------------------------------------------------------------
typedef struct {
    uint8_t dest_mac[6]; // MAC adresa ciloveho Uzlu B (router ji precte a preposle)
    uint8_t stav;        // 0 = tlacitko pusteno, 1 = tlacitko stisknuto
    uint8_t hop_count;   // Pocet skoku - router ho inkrementuje pred preposlanim
} lab_packet_t;

// Pomocna funkce: zkontroluje, zda je peer uz zaregistrovan, a pokud ne, prida ho
void ensure_peer(const uint8_t *mac) {
    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peer = {};
        memcpy(peer.peer_addr, mac, 6);
        peer.channel = 0;      // 0 = aktualni kanal
        peer.encrypt = false;
        esp_err_t result = esp_now_add_peer(&peer);
        if (result == ESP_OK) {
            Serial.printf("UZEL_C: Novy peer zaregistrovan: %02X:%02X:%02X:%02X:%02X:%02X\n",
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            Serial.printf("UZEL_C: Chyba pri registraci peera (kod: %d)\n", result);
        }
    }
}

// Callback pri prijmu dat od Uzlu A
void OnDataRecv(const uint8_t *mac_sender, const uint8_t *incomingData, int len) {
    if (len != sizeof(lab_packet_t)) {
        Serial.printf("UZEL_C: Prijat paket neznameho formatu (%d B), ignoruji.\n", len);
        return;
    }

    // Rozbal paket a precti cilovou MAC adresu
    lab_packet_t pkt;
    memcpy(&pkt, incomingData, sizeof(pkt));

    Serial.printf("UZEL_C: Zprava zachycena od %02X:%02X:%02X:%02X:%02X:%02X\n",
                  mac_sender[0], mac_sender[1], mac_sender[2],
                  mac_sender[3], mac_sender[4], mac_sender[5]);
    Serial.printf("UZEL_C: Cilovy Uzel B: %02X:%02X:%02X:%02X:%02X:%02X | hop_count: %d\n",
                  pkt.dest_mac[0], pkt.dest_mac[1], pkt.dest_mac[2],
                  pkt.dest_mac[3], pkt.dest_mac[4], pkt.dest_mac[5],
                  pkt.hop_count);

    // Dynamicky zaregistruj cilovy Uzel B (pokud ho jeste nezname)
    ensure_peer(pkt.dest_mac);

    // Inkrementuj pocitadlo skoku
    pkt.hop_count++;

    // Prepošli paket na cilovy Uzel B
    esp_err_t result = esp_now_send(pkt.dest_mac, (uint8_t *)&pkt, sizeof(pkt));
    if (result == ESP_OK) {
        Serial.println("UZEL_C: Paket uspesne preposlan na Uzel B.");
    } else {
        Serial.printf("UZEL_C: Chyba pri preposilani (kod: %d)\n", result);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("UZEL_C: Router startuje...");

    // Nastaveni WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    // Vypis vlastni MAC adresy - pedagog si ji zapise a nasdili studentum
    Serial.print("UZEL_C: MOJE MAC ADRESA (sdelte studentum): ");
    Serial.println(WiFi.macAddress());

    // Inicializace ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("UZEL_C: Chyba pri inicializaci ESP-NOW!");
        return;
    }

    // Registrace callbacku - zadna dalsi konfigurace neni potreba
    // Cílový Uzel B se zaregistruje automaticky pri prijmu prvniho paketu
    esp_now_register_recv_cb(OnDataRecv);

    Serial.println("UZEL_C: Router je aktivni. Ceka na pakety k preposilani...");
    Serial.println("UZEL_C: Cilovy peer bude automaticky zjisten z payloadu paketu.");
}

void loop() {
    delay(10000);
}
