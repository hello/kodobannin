#include "device_info.h"
#include "app.h"
#include <string.h>
#include "crypto.h"
#include "crc16.h"
#include <nrf_soc.h>
#include "platform.h"
#include "hlo_keys.h"
#include "util.h"

void generate_new_device(device_info_t * info){
    volatile uint8_t aes[16] = HLO_FACTORY_AES;
    device_meta_info_t * meta = &(info->meta);
    device_encrypted_info_t * einfo = &(info->factory_info);

    RNG_custom_init( (uint8_t*)NRF_FICR->ER, 16);

#ifdef FIRMWARE_VERSION_8BIT
    meta->bootloader_ver = FIRMWARE_VERSION_8BIT;
#else
    meta->bootloader_ver = 0;
#endif
    meta->hw_revision = HW_REVISION;
    memset(meta->reserved, 0xA5, 4);
    get_random(META_NONCE_SIZE, meta->nonce);

    memcpy(einfo->device_id,(const uint8_t*)NRF_FICR->DEVICEID, sizeof(einfo->device_id));
    memcpy(einfo->device_address,(const uint8_t*)NRF_FICR->DEVICEADDR, sizeof(einfo->device_address));
    /**
     * IMPORTANT
     * GENERATE IDENTITY TO BE REPORTED TO HLO HQ
     */
    get_random(16, einfo->device_aes);
    memcpy(einfo->ficr, NRF_FICR, sizeof(einfo->ficr));
    sha1_calc(einfo, sizeof(*einfo) - SHA1_DIGEST_LENGTH, einfo->sha);
 
    aes128_ctr_encrypt_inplace((uint8_t*)einfo, sizeof(*einfo), (const uint8_t*)aes, meta->nonce);

    meta->factory_crc = crc16_compute((uint8_t*)einfo, sizeof(*einfo), NULL);
}

bool validate_device_fast(uint32_t address){
    device_info_t * head = (device_info_t*)address;
    if(head->meta.factory_crc == crc16_compute((uint8_t*)&head->factory_info, sizeof(head->factory_info),NULL)){
        return true;
    }
    return false;
}
void decrypt_device(uint32_t address, device_info_t * out_info){
    volatile uint8_t aes[16] = HLO_FACTORY_AES;
    device_info_t * in = (device_info_t*)address;
    memcpy(out_info, in, sizeof(*out_info));
    aes128_ctr_decrypt_inplace((uint8_t*)&out_info->factory_info, sizeof(out_info->factory_info), (const uint8_t*)aes, in->meta.nonce);
}


