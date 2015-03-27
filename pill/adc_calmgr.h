#pragma once

/**
 * adc calbration values manager
 * 
 **/

typedef struct{
    uint32_t res;
    uint32_t type; // pullup/down
    uint32_t slope;
    uint32_t offset;
}ADC_CalValue_t;

//init
uint32_t ADC_CalMgr_Init(void);

//adds to cache(does not write to cache)
uint32_t ADC_CalMgr_Add(const ADC_CalValue_t * id);

//writes to flash
uint32_t ADC_CalMgr_Commit(void);

//removes from cache(does not write to flash)
uint32_t ADC_CalMgr_Remove(const ADC_CalValue_t * id);

//Queries the entry by adc id
ADC_CalValue_t * ADC_CalMgr_Query(const uint32_t type);

//erases all from flash
uint32_t ADC_CalMgr_EraseAll(void);

int ADC_CalMgr_ForEach(void);

