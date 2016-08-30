#pragma once
#include <stdint.h>
#include "sha1.h"
#include <stdbool.h>

#define META_NONCE_SIZE 8
typedef struct{
    uint8_t  reserved[2];
    uint8_t  factory_key_ver;
    uint8_t  fw_signature_key_ver;
    uint8_t  hw_revision;
    uint8_t  bootloader_ver;
    uint16_t factory_crc;
    uint8_t nonce[META_NONCE_SIZE];
}__attribute__((packed)) device_meta_info_t;

typedef struct{
    uint32_t device_id[2];
    uint32_t device_address[2];
    uint8_t device_aes[16];
    uint8_t ficr[256];
    uint8_t sha[SHA1_DIGEST_LENGTH];
}__attribute__((packed)) device_encrypted_info_t;

typedef struct{
    device_meta_info_t meta;
    device_encrypted_info_t factory_info;
}__attribute__((packed)) device_info_t;

void generate_new_device(device_info_t * info);
bool validate_device_fast(uint32_t address);
void decrypt_device(uint32_t address, device_info_t * out_info);

