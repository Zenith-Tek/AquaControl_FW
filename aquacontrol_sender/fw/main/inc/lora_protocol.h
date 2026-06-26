#ifndef __LORA_PROTOCOL_H__
#define __LORA_PROTOCOL_H__

#include <stdint.h>

// Plaintext data that gets encrypted (6 bytes)
typedef struct __attribute__((packed)) {
    float distance_cm;
    uint8_t battery_percent;
    uint8_t solar_voltage_x10;
    uint8_t flags;
} secure_payload_t;

// The complete physical LoRa packet structure (50 bytes)
typedef struct __attribute__((packed)) {
    // --- Plaintext Header / AAD (16 bytes) ---
    uint8_t src_mac[6];
    uint8_t dest_mac[6];
    uint32_t seq_num;

    // --- Crypto Metadata (28 bytes) ---
    uint8_t iv[12];
    uint8_t tag[16];

    // --- Ciphertext (6 bytes) ---
    uint8_t ciphertext[sizeof(secure_payload_t)];
} secure_lora_packet_t;

// ACK payload sent from Receiver to Sender (3 bytes)
typedef struct __attribute__((packed)) {
    uint8_t motor_state;            // 0 = OFF, 1 = ON
    uint8_t auto_control_enabled;   // 0 = OFF, 1 = ON
    uint8_t flags;                  // Bit 0: BLE Enable Request
} secure_ack_payload_t;

// The complete physical ACK packet structure (46 bytes)
typedef struct __attribute__((packed)) {
    // --- Plaintext Header / AAD (16 bytes) ---
    uint8_t src_mac[6];
    uint8_t dest_mac[6];
    uint32_t seq_num;

    // --- Crypto Metadata (28 bytes) ---
    uint8_t iv[12];
    uint8_t tag[16];

    // --- Ciphertext (2 bytes) ---
    uint8_t ciphertext[sizeof(secure_ack_payload_t)];
} secure_ack_packet_t;

#endif
