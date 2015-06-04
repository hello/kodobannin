/**
 * mostly copypasta code from ble_bondmngr.c
 * TODO
 * make applicationg generic
 **/
#include <stdint.h>

#include "adc_calmgr.h"
#include "pstorage.h"
#include "util.h"
#include "crc16.h"

#define ADC_CAL_MANAGER_DATA_SIGNATURE  0x53940000
#define ADC_CAL_MANAGER_DATA_COUNT  4

typedef struct{
    pstorage_handle_t   mp_flash_cal_val_info;  /** < Pointer to flash location to write next cal val info */
    pstorage_size_t cal_val_count;              /** < Number of calibration values in flash **/
    ADC_CalValue_t values[ADC_CAL_MANAGER_DATA_COUNT];
    uint32_t crc[ADC_CAL_MANAGER_DATA_COUNT];
}ADC_CalMgr_t;

static ADC_CalMgr_t self;
static volatile int dirty;
static void bm_pstorage_cb_handler(pstorage_handle_t * handle,
                                   uint8_t             op_code,
                                   uint32_t            result,
                                   uint8_t           * p_data,
                                   uint32_t            data_len);
static uint32_t values_info_load_from_flash(ADC_CalValue_t * p_value, pstorage_size_t * block_idx);
static uint32_t load_all_from_flash(void);
static uint32_t crc_extract(uint32_t header, uint16_t * p_crc);
static uint32_t _commit_block(const ADC_CalValue_t * p_value, pstorage_size_t  block_idx);

static uint32_t crc_extract(uint32_t header, uint16_t * p_crc){
    if ((header & 0xFFFF0000U) == ADC_CAL_MANAGER_DATA_SIGNATURE)
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

static uint32_t values_info_load_from_flash(ADC_CalValue_t * p_value, pstorage_size_t * block_idx)
{
    pstorage_handle_t source_block;
    uint32_t          err_code;
    uint32_t          crc;
    uint32_t        m_crc_cal_val_info;
    uint16_t          crc_header;

    // Get block pointer from base
    err_code = pstorage_block_identifier_get(&self.mp_flash_cal_val_info,
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
    
    // Load calibration values
    err_code = pstorage_load((uint8_t *)p_value,
                             &source_block,
                             sizeof(*p_value),
                             sizeof(uint32_t));
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    // Check CRC.
    m_crc_cal_val_info = crc16_compute((uint8_t *)p_value,
                                    sizeof(*p_value),
                                    NULL);
    if (m_crc_cal_val_info == crc_header)
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

uint32_t ADC_CalMgr_Init(void){
    pstorage_module_param_t param;
    uint32_t err_code;
          
    param.block_size  = sizeof (ADC_CalValue_t) + sizeof (uint32_t);
    param.block_count = ADC_CAL_MANAGER_DATA_COUNT;
    param.cb          = bm_pstorage_cb_handler;
    
    err_code = pstorage_register(&param, &self.mp_flash_cal_val_info);    
    if (err_code != NRF_SUCCESS)
    {
        PRINTS("ADC Calibration Error =");
        PRINT_HEX(&err_code, 2);
        return err_code;
    }
    load_all_from_flash();
    return err_code;
}

static uint32_t _commit_block(const ADC_CalValue_t * p_value, pstorage_size_t  block_idx){
    uint32_t err_code;
    pstorage_handle_t dest_block;
    uint32_t m_crc_cal_val_info;
    //self.crc[block_idx] = crc16_compute(NULL, 0, NULL);
    err_code = pstorage_block_identifier_get(&self.mp_flash_cal_val_info,block_idx,&dest_block);
    // Get block pointer from base
    if (err_code != NRF_SUCCESS){
        return err_code;
    }

    // Write Bonding Information
    err_code = pstorage_store(&dest_block,
            (uint8_t *)p_value,
            sizeof(*p_value),
            sizeof(uint32_t));


    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    m_crc_cal_val_info = crc16_compute((uint8_t *)p_value,
            sizeof(*p_value),
            NULL);

    // Write header
    self.crc[block_idx] = (ADC_CAL_MANAGER_DATA_SIGNATURE | m_crc_cal_val_info);
    err_code = pstorage_store(&dest_block, (uint8_t *)&self.crc[block_idx],sizeof(uint32_t),0);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    return NRF_SUCCESS;

}
uint32_t ADC_CalMgr_Commit(void){
    uint32_t i;
    if(dirty){
        ADC_CalMgr_EraseAll();
        for(i = 0; i < self.cal_val_count; i++){
            _commit_block(&self.values[i], i);
        }
        dirty = 0;
    }else{
        PRINTS("No Need to recommit\r\n");
    }
    return 0;
}

uint32_t ADC_CalMgr_Add(const ADC_CalValue_t * p_value){
    // Check if flash is full
    if (self.cal_val_count >= ADC_CAL_MANAGER_DATA_COUNT)
    {
        return NRF_ERROR_NO_MEM;
    }
    // Check if in cache
    {
        int i;
        for(i = 0; i < self.cal_val_count; i ++){
            if(self.values[i].pu == p_value->pu &&
               self.values[i].pd == p_value->pd){
                PRINTS("Already in flash");
                return NRF_SUCCESS;
            }
        }
        self.values[self.cal_val_count] = *p_value;
        dirty = 1;
    }
    self.cal_val_count++;
    return NRF_SUCCESS;
}
uint32_t ADC_CalMgr_Remove(const ADC_CalValue_t * p_value){
    if(self.cal_val_count){
        int i;
        ADC_CalValue_t * tmp = &self.values[self.cal_val_count - 1];
        for(i = 0; i < ADC_CAL_MANAGER_DATA_COUNT; i++){
            if(self.values[i].pu == p_value->pu &&
               self.values[i].pd == p_value->pd){
                PRINTS("Erasing PU/PD");
                PRINT_HEX(&self.values[i].pu, 2);
                PRINT_HEX(&self.values[i].pd, 2);
                PRINTS("\r\n");
                self.values[i] = *tmp;
                self.cal_val_count--;
                dirty = 1;
                return 0;
            }
        }
    }
    PRINTS("Device not found\r\n");
    return 1;
}
ADC_CalValue_t * ADC_CalMgr_Query(const uint32_t pu, const uint32_t pd){
    int i;
    for(i = 0; i < self.cal_val_count; i++){
        if(self.values[i].pu == pu &&
           self.values[i].pd == pd){
            return &self.values[i];
        }
    }
    return NULL;
}
int ADC_CalMgr_Count(void){
 // int i;
 // for(i = 0; i < self.cal_val_count; i++){
 //     cb(&self.devices[i]);
 // }
    return self.cal_val_count;
}

uint32_t ADC_CalMgr_EraseAll(void){
    uint32_t err_code;
    err_code = pstorage_clear(&self.mp_flash_cal_val_info, ADC_CAL_MANAGER_DATA_COUNT * (sizeof(uint32_t) + sizeof(ADC_CalValue_t)));
    return err_code;
}

//only call on init
static uint32_t load_all_from_flash(){
    ADC_CalValue_t value;
    uint32_t err;
    while(1){
         err = values_info_load_from_flash(&value, &self.cal_val_count);
        if(err != NRF_SUCCESS){
            break;
        }else{
            self.values[self.cal_val_count-1] = value;
        }
    }
    PRINTS("ADC Cal Count = ");
    PRINT_DEC(&self.cal_val_count, 2);
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
