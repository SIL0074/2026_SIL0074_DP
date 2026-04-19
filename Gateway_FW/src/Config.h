#pragma once

// --- UART Bridge (spojení s receiverem) ---
#define BRIDGE_UART_PORT UART_NUM_2
#define BRIDGE_TX_PIN    12  // volný pin na LilyGO
#define BRIDGE_RX_PIN    13  // volný pin na LilyGO

// --- SIM7000G modem (HW fixní piny na T-SIM7000G) ---
#define MODEM_UART_PORT  UART_NUM_1
#define MODEM_TX_PIN     27
#define MODEM_RX_PIN     26
#define MODEM_PWR_PIN    4
#define MODEM_APN        "lpwa.vodafone.com"

// --- RGB LED status ---
#define LED_R_PIN        21
#define LED_G_PIN        22
#define LED_B_PIN        32

// --- ostatní věci ---
#define MAX_SENSORS     50
#define ESP_NOW_CHANNEL 11
