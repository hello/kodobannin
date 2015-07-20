#include "sspi_user.h"
#include "util.h"
#include "message_ble.h"
#include "proto_tools.h"

//MorpheusCommand Handler
static void _handle_MorpheusCommand(const MSG_Central_t * central, MSG_Address_t address, MSG_Data_t * data){
    MorpheusCommand command = {0};
    if( MorpheusCommand_from_message(&command, data) ){
        central->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, data);
        free_MorpheusCommand(&command);
    }else{
        PRINTS("Unable to decode protobuf\r\n");
    }
}

//this is sent from sspi
void spi_handler(const MSG_Central_t * central, MSG_Address_t address, MSG_Data_t * data){
    switch(address){
        case 0:
            _handle_MorpheusCommand(central, address, data);
            break;
        default:
            PRINTS("Unknown SSPI Address:");
            PRINT_HEX(&address, sizeof(address));
            PRINTS("\r\n");
            break;
    }
}
