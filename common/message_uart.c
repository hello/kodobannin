#include "nrf.h"
#include "util.h"
#include "message_uart.h"

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    bool initialized;
    MSG_Data_t * rx_buf;
    uint16_t rx_index;
    app_uart_comm_params_t uart_params;
}self;
static char * name = "UART";

static void
_printblocking(const uint8_t * d, uint32_t len, int hex_enable){
    int i;
    if(self.initialized){
        for(i = 0; i < len; i++){
            if(hex_enable){
                while(app_uart_put(hex[0xF&(d[i]>>4)]) != SUCCESS){
                }
                while(app_uart_put(hex[ 0xF&d[i] ] ) != SUCCESS){
                }
            }else{
                while(app_uart_put(d[i]) != SUCCESS){
                }
            }
        }
    }

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
_send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    if(data){
        if(dst.submodule == 0){
            //meta command
        }else{
            //only 1 connection
            MSG_Base_AcquireDataAtomic(data);
            _printblocking("\r\n<data>",8, 0);
            _printblocking(data->buf,data->len,1);
            _printblocking("</data>\r\n",9, 0);
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
    return SUCCESS;
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
                        app_uart_put('\n');
                        app_uart_put('\r');
                        if(self.rx_buf){
                            //execute command
                            MSG_Base_ReleaseDataAtomic(self.rx_buf);
                            self.rx_buf = NULL;
                        }
                        break;
                    case '\b':
                    case '\177': // backspace
                        if(self.rx_index){
                            app_uart_put('\b');
                            app_uart_put(' ');
                            app_uart_put('\b');
                            self.rx_index--;
                        }
                        break;
                    default:
                        if(!self.rx_buf){
                            self.rx_buf = MSG_Base_AllocateDataAtomic(MSG_UART_COMMAND_MAX_SIZE);
                            self.rx_index = 0;
                        }
                        //work on buffer
                        if(self.rx_buf && self.rx_index < self.rx_buf->len){
                            self.rx_buf->buf[self.rx_index++] = c;
                            app_uart_put(c);
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
static MSG_Status
_init(void){
    uint32_t err;
    APP_UART_FIFO_INIT(&self.uart_params, 16, 256, _uart_event_handler, APP_IRQ_PRIORITY_LOW, err);
    if(!err){
        self.initialized = 1;
        return SUCCESS;
    }
    return FAIL;
    
}

MSG_Base_t * MSG_Uart_Base(const app_uart_comm_params_t * params, const MSG_Central_t * parent){
    self.base.init = _init;
    self.base.destroy = _destroy;
    self.base.flush = _flush;
    self.base.send = _send;
    self.base.type = UART;
    self.base.typestr = name;
    self.parent = parent;
    self.uart_params = *params;
    return &self.base;

}
void MSG_Uart_Prints(const char * str){
    if(self.initialized){
        char * head = str;
        while(*head){
            app_uart_put(*head);
            head++;
        }
    }
}

void MSG_Uart_PrintHex(const uint8_t * ptr, uint32_t len){
    if(self.initialized){
        while(len-- >0) {
            app_uart_put(hex[0xF&(*ptr>>4)]);
            app_uart_put(hex[0xF&*ptr++]);
            app_uart_put(' ');
        }
    }
}
