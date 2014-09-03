#pragma once

#include "message_ant.h"
/**
 * ant bond manager used to bond devices
 * 
 **/

typedef struct{
    ANT_ChannelID_t id;
    uint64_t full_uid;
    uint64_t reserved[2];
}ANT_BondedDevice_t;
//typedef ANT_ChannelID_t ANT_BondedDevice_t;

uint32_t ANT_BondMgrInit(void);
uint32_t ANT_BondMgrAdd(const ANT_BondedDevice_t * id);
uint32_t ANT_BondMgrRemove(const ANT_BondedDevice_t * id);
uint32_t ANT_BondMgr_EraseAll(void);
typedef void(* ANT_BondMgrCB)(const ANT_BondedDevice_t * id);
void ANT_BondMgrForEach(ANT_BondMgrCB cb); 

