#include "sspi_user.h"
#include "util.h"
#include "message_ble.h"

void spi_handler(const MSG_Central_t * central, MSG_Address_t address, MSG_Data_t * data){
    central->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, data);
}
