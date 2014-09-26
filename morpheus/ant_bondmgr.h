#pragma once

#include "message_ant.h"
/**
 * ant bond manager used to bond devices
 * 
 **/

typedef struct{
    hlo_ant_device_t id;
    uint64_t full_uid;
    uint64_t reserved[2];
}ANT_BondedDevice_t;

//init
uint32_t ANT_BondMgrInit(void);

//adds to cache(does not write to cache)
uint32_t ANT_BondMgrAdd(const ANT_BondedDevice_t * id);

//writes back to flash
uint32_t ANT_BondMgrCommit(void);

//removes from cache(does not write to flash)
uint32_t ANT_BondMgrRemove(const ANT_BondedDevice_t * id);

//erases all from flash
uint32_t ANT_BondMgr_EraseAll(void);

typedef void(* ANT_BondMgrCB)(const ANT_BondedDevice_t * id);
//util function to iterate over cached devices
void ANT_BondMgrForEach(ANT_BondMgrCB cb); 

