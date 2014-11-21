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
    device_meta_info_t * meta = &(info->meta);
    device_encrypted_info_t * einfo = &(info->factory_info);

    RNG_custom_init(NRF_FICR->ER, 16);

    meta->bootloader_ver = FIRMWARE_VERSION_8BIT;
    meta->hw_revision = HW_REVISION;
    memset(meta->reserved, 0xA5, 4);
    get_random(META_NONCE_SIZE, meta->nonce);

    memcpy(einfo->device_id,NRF_FICR->DEVICEID, sizeof(einfo->device_id));
    memcpy(einfo->device_address,NRF_FICR->DEVICEADDR, sizeof(einfo->device_address));
    get_random(16, einfo->device_aes);
    memcpy(einfo->ficr, NRF_FICR, sizeof(einfo->ficr));
    sha1_calc(einfo, sizeof(*einfo) - SHA1_DIGEST_LENGTH, einfo->sha);
 

    aes128_ctr_encrypt_inplace(einfo, sizeof(*einfo), (uint8_t*)HLO_FACTORY_AES, meta->nonce);

    meta->factory_crc = crc16_compute((uint8_t*)einfo, sizeof(*einfo), NULL);
}



