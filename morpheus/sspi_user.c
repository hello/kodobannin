#include "sspi_user.h"
#include "util.h"
#include "message_ble.h"
#include "proto_tools.h"

//MorpheusCommand Handler from spi
static void _spi_handle_MorpheusCommand(const MSG_Central_t * central, MSG_Address_t address, MSG_Data_t * data){
    MorpheusCommand command = {0};
    if( MorpheusCommand_from_message(&command, data) ){
        switch(command.type){
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_GET_DEVICE_ID:
                if(self.boot_state == BOOT_COMPLETED){
                    //allows resyncing of topboard information
                    _sync_device_id();
                    _sync_pairing_info(self.cached_is_pairing, self.cached_bond_count);
                }
                // else, fallback to normal boot sequence, the same code in SYNC_DEVICE_ID
                // DONOT put a break here, it is intended to fall to 
                // MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SYNC_DEVICE_ID:
                PRINTS("BLE init command received: ");
                PRINT_HEX(&command.type, sizeof(command.type));
                PRINTS("\r\n");
                if(self.boot_state != BOOT_COMPLETED){
                    _init_ble_stack(&command);
                }
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_MORPHEUS_DFU_BEGIN:
                PRINTS("DFU from CC3200..\r\n");
                REBOOT_TO_DFU();
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
                PRINTS("switch to pairing..\r\n");
                central->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, data);
                _hold_to_enter_pairing_mode();
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_NORMAL_MODE:
                PRINTS("switch to normal..\r\n");
                central->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, data);
                _hold_to_enter_normal_mode();
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_ERASE_PAIRED_PHONE:
                PRINTS("erase phone..\r\n");
                central->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, data);
                hble_refresh_bonds(ERASE_OTHER_BOND, true);
                break;
            case MorpheusCommand_CommandType_MORPHEUS_COMMAND_FACTORY_RESET:
                PRINTS("Factory reset from CC3200..\r\n");
                central->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, data);
                hble_refresh_bonds(ERASE_ALL_BOND, true);
                break;
            default:
                central->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, data);
                break;
    }else{
        PRINTS("Unable to decode protobuf\r\n");
    }
    free_MorpheusCommand(&command);
}

//this is sent from sspi
void spi_handler(const MSG_Central_t * central, MSG_Address_t address, MSG_Data_t * data){
    switch(address){
        case 0:
            _spi_handle_MorpheusCommand(central, address, data);
            break;
        default:
            PRINTS("Unknown SSPI Address:");
            PRINT_HEX(&address, sizeof(address));
            PRINTS("\r\n");
            break;
    }
}
