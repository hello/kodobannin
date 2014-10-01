/**
 * mostly copypasta code from ble_bondmngr.c
 * TODO
 * make applicationg generic
 **/
#include "ant_bondmgr.h"
#include "pstorage.h"
#include "util.h"
#include "crc16.h"

#define ANT_BOND_MANAGER_DATA_SIGNATURE  0x53540000
#define ANT_BOND_MANAGER_DATA_COUNT  4

typedef struct{
    pstorage_handle_t   mp_flash_bond_info;                              /**< Pointer to flash location to write next Bonding Information. */
    pstorage_size_t bond_count;                                /** < Number of bonded devices in flash **/
    ANT_BondedDevice_t devices[ANT_BOND_MANAGER_DATA_COUNT];
    uint32_t crc[ANT_BOND_MANAGER_DATA_COUNT];
}ANT_BondMgr_t;

static ANT_BondMgr_t self;
static volatile int dirty;
static void bm_pstorage_cb_handler(pstorage_handle_t * handle,
                                   uint8_t             op_code,
                                   uint32_t            result,
                                   uint8_t           * p_data,
                                   uint32_t            data_len);
static uint32_t bonding_info_load_from_flash(ANT_BondedDevice_t * p_bond, pstorage_size_t * block_idx);
static uint32_t load_all_from_flash(void);
static uint32_t crc_extract(uint32_t header, uint16_t * p_crc);
static uint32_t _commit_block(const ANT_BondedDevice_t * p_bond, pstorage_size_t  block_idx);

static uint32_t crc_extract(uint32_t header, uint16_t * p_crc){
    if ((header & 0xFFFF0000U) == ANT_BOND_MANAGER_DATA_SIGNATURE)
    {
        *p_crc = (uint16_t)(header & 0x0000FFFFU);

        return NRF_SUCCESS;
    }
    else if (header == 0xFFFFFFFFU)
    {
        return NRF_ERROR_NOT_FOUND;
    }
    else
    {
        return NRF_ERROR_INVALID_DATA;
    }
}

static uint32_t bonding_info_load_from_flash(ANT_BondedDevice_t * p_bond, pstorage_size_t * block_idx)
{
    pstorage_handle_t source_block;
    uint32_t          err_code;
    uint32_t          crc;
    uint32_t        m_crc_bond_info;
    uint16_t          crc_header;
    

    // Get block pointer from base
    err_code = pstorage_block_identifier_get(&self.mp_flash_bond_info,
                                                *block_idx,
                                             &source_block);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    err_code = pstorage_load((uint8_t *)&crc, &source_block, sizeof(uint32_t), 0);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    // Extract CRC from header.
    // if result is NRF_ERROR_NOT_FOUND, then no data needs to be loaded
    err_code = crc_extract(crc, &crc_header);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    
    // Load devices.
    err_code = pstorage_load((uint8_t *)p_bond,
                             &source_block,
                             sizeof(*p_bond),
                             sizeof(uint32_t));
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Check CRC.
    m_crc_bond_info = crc16_compute((uint8_t *)p_bond,
                                    sizeof(*p_bond),
                                    NULL);
    if (m_crc_bond_info == crc_header)
    {
        (*block_idx)++;
        return NRF_SUCCESS;
    }
    else
    {
        return NRF_ERROR_INVALID_DATA;
    }
    return NRF_SUCCESS;
}

uint32_t ANT_BondMgrInit(void){
    pstorage_module_param_t param;
    uint32_t err_code;
          
    param.block_size  = sizeof (ANT_BondedDevice_t) + sizeof (uint32_t);
    param.block_count = ANT_BOND_MANAGER_DATA_COUNT;
    param.cb          = bm_pstorage_cb_handler;
    
    err_code = pstorage_register(&param, &self.mp_flash_bond_info);    
    if (err_code != NRF_SUCCESS)
    {
        PRINTS("ANT BOND Error =");
        PRINT_HEX(&err_code, 2);
        return err_code;
    }
    load_all_from_flash();
    return err_code;
}

static uint32_t _commit_block(const ANT_BondedDevice_t * p_bond, pstorage_size_t  block_idx){
    uint32_t err_code;
    pstorage_handle_t dest_block;
    uint32_t m_crc_bond_info;
    //self.crc[block_idx] = crc16_compute(NULL, 0, NULL);
    err_code = pstorage_block_identifier_get(&self.mp_flash_bond_info,block_idx,&dest_block);
    // Get block pointer from base
    if (err_code != NRF_SUCCESS){
        return err_code;
    }

    // Write Bonding Information
    err_code = pstorage_store(&dest_block,
            (uint8_t *)p_bond,
            sizeof(*p_bond),
            sizeof(uint32_t));


    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    m_crc_bond_info = crc16_compute((uint8_t *)p_bond,
            sizeof(*p_bond),
            NULL);

    // Write header
    self.crc[block_idx] = (ANT_BOND_MANAGER_DATA_SIGNATURE | m_crc_bond_info);
    err_code = pstorage_store(&dest_block, (uint8_t *)&self.crc[block_idx],sizeof(uint32_t),0);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    return NRF_SUCCESS;

}
uint32_t ANT_BondMgrCommit(void){
    uint32_t m_crc_bond_info, i;
    if(dirty){
        ANT_BondMgr_EraseAll();
        for(i = 0; i < self.bond_count; i++){
            _commit_block(&self.devices[i], i);
        }
        dirty = 0;
    }else{
        PRINTS("No Need to recommit\r\n");
    }
    return 0;
}

uint32_t ANT_BondMgrAdd(const ANT_BondedDevice_t * p_bond){
    // Check if flash is full
    if (self.bond_count >= ANT_BOND_MANAGER_DATA_COUNT)
    {
        return NRF_ERROR_NO_MEM;
    }
    // Check if in cache
    {
        int i;
        for(i = 0; i < ANT_BOND_MANAGER_DATA_COUNT; i ++){
            if(self.devices[i].full_uid == p_bond->full_uid){
                PRINTS("Already in flash");
                return NRF_SUCCESS;
            }
        }
        self.devices[self.bond_count] = *p_bond;
        dirty = 1;
    }
    self.bond_count++;
    return NRF_SUCCESS;
}
uint32_t ANT_BondMgrRemove(const ANT_BondedDevice_t * p_bond){
    //swap with the last bondcount
    //TODO implement
    if(self.bond_count){
        int i;
        ANT_BondedDevice_t * tmp = &self.devices[self.bond_count - 1];
        for(i = 0; i < ANT_BOND_MANAGER_DATA_COUNT; i++){
            if(self.devices[i].full_uid == p_bond->full_uid){
                PRINTS("Erasing ID");
                PRINT_HEX(&self.devices[i].id.device_number, 2);
                PRINTS("\r\n");
                self.devices[i] = *tmp;
                self.bond_count--;
                dirty = 1;
                return 0;
            }
        }
    }
    return 1;
}
void ANT_BondMgrForEach(ANT_BondMgrCB cb){
    int i;
    for(i = 0; i < self.bond_count; i++){
        cb(&self.devices[i]);
    }
}

uint32_t ANT_BondMgr_EraseAll(void){
    uint32_t err_code;
    err_code = pstorage_clear(&self.mp_flash_bond_info, ANT_BOND_MANAGER_DATA_COUNT * (sizeof(uint32_t) + sizeof(ANT_BondedDevice_t)));
    return err_code;
}

//only call on init
static uint32_t load_all_from_flash(){
    ANT_BondedDevice_t dev;
    uint32_t err;
    do{
        err = bonding_info_load_from_flash(&dev, &self.bond_count);
        self.devices[self.bond_count] = dev;
    }while(err == NRF_SUCCESS);
    PRINTS("ANT BOND COUNT = ");
    PRINT_HEX(&self.bond_count, 2);
    return err;
}
static void bm_pstorage_cb_handler(pstorage_handle_t * handle,
                                   uint8_t             op_code,
                                   uint32_t            result,
                                   uint8_t           * p_data,
                                   uint32_t            data_len){

    PRINTS("H ");
    PRINT_HEX(handle, 4);
    PRINTS("X ");
    PRINT_HEX(&op_code, 1);
    PRINTS("D ");
    PRINT_HEX(p_data, 4);
    PRINTS("\r\n");
}
