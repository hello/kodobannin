#include "nrf.h"
#include "util.h"
#include "message_uart.h"

static struct{
    MSG_Base_t base;
    MSG_Base_t * parent;
    bool initialized;
    char cmdbuf[MSG_UART_COMMAND_MAX_SIZE];
    uint8_t cmdbuf_index;
}self;

static MSG_Status
_init(void){
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}
static MSG_Status
_flush(void){
    return SUCCESS;
}
static MSG_Status
_send(MSG_Data_t * data){
    for(int i = 0; i < data->len; i++){
        app_uart_put(data->buf[i]);
    }
    return SUCCESS;
}
static MSG_Status
_buf_to_msg_data(MSG_Data_t * out_msg_data){
    if(out_msg_data){
        out_msg_data->len = self.cmdbuf_index;
    }
}
static void
_uart_event_handler(app_uart_evt_t * evt){
    uint8_t c;
    switch(evt->evt_type){
        /**< An event indicating that UART data has been received. The data is available in the FIFO and can be fetched using @ref app_uart_get. */
        case APP_UART_DATA_READY:
            while(!app_uart_get(&c)){
                switch(c){
                    case '\r':
                    case '\n':
                        if(self.cmdbuf_index){
                            app_uart_put('\n');
                            app_uart_put('\r');
                            //execute command
                            _buf_to_msg_data(0);
                            self.parent->send(0);
                            self.cmdbuf_index = 0;
                        }
                        break;
                    case '\b':
                    case '\177': // backspace
                        if(self.cmdbuf_index){
                            app_uart_put('\b');
                            app_uart_put(' ');
                            app_uart_put('\b');
                            self.cmdbuf_index--;
                        }
                        break;
                    default:
                        app_uart_put(c);
                        if(self.cmdbuf_index < MSG_UART_COMMAND_MAX_SIZE - 1){
                            self.cmdbuf_index++;
                        }
                        break;
                }
            }
            break;
        /**< An error in the FIFO module used by the app_uart module has occured. The FIFO error code is stored in app_uart_evt_t.data.error_code field. */
        case APP_UART_FIFO_ERROR:
            break;
        /**< An communication error has occured during reception. The error is stored in app_uart_evt_t.data.error_communication field. */
        case APP_UART_COMMUNICATION_ERROR:
            break;
        /**< An event indicating that UART has completed transmission of all available data in the TX FIFO. */
        case APP_UART_TX_EMPTY:
            break;
        /**< An event indicating that UART data has been received, and data is present in data field. This event is only used when no FIFO is configured. */
        case APP_UART_DATA:
            break;
    }
}

MSG_Base_t * MSG_Uart_Init(const app_uart_comm_params_t * params, MSG_Base_t * central){
    uint32_t err;
    if(self.initialized){
        return &self.base;
    }
    self.parent = central;
    APP_UART_FIFO_INIT(params, 32, 512, _uart_event_handler, APP_IRQ_PRIORITY_HIGH, err);
    if(!err){
        self.initialized = 1;
    }
    return &self.base;
}
void MSG_Uart_Prints(const char * str){
    if(self.initialized){
        char * head = str;
        while(*head){
            app_uart_put(*head);
            head++;
        }
        app_uart_put('\0');
    }
}

