#include "proto_tools.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "nrf_error.h" 

MSG_Data_t * message_from_MorpheusCommand(const MorpheusCommand * command){
    return NULL;
}


uint32_t MorpheusCommand_from_message(MorpheusCommand * command, MSG_Data_t * message){
    return NRF_SUCCESS;
}

void free_MorpheusCommand(MorpheusCommand * command){

}
