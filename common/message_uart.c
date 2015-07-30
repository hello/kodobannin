#include "nrf.h"
#include "util.h"
#include "message_uart.h"

static struct{
    MSG_Base_t base;
    const MSG_Central_t * parent;
    bool initialized;
    uint8_t last_char;
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
        }else if(dst.submodule == MSG_UART_HEX){
            //only 1 connection
            _printblocking("\r\n<data>",8, 0);
            _printblocking(data->buf,data->len,1);
            _printblocking("</data>\r\n",9, 0);
        }else if(dst.submodule == MSG_UART_SLIP){
            uint8_t test_slip[] = {0xc0, 0xc1, 0xae, 0x00, 0x91, 0x02, 0x00, 0x00, 0x00, 0xb8, 0x43, 0x00, 0x00, 0xe1, 0x38, 0xc0};
            _printblocking(test_slip,sizeof(test_slip), 0);
        }else if(dst.submodule == MSG_UART_STRING){
            _printblocking("\r\n<data>",8, 0);
            _printblocking(data->buf,data->len,0);
            _printblocking("</data>\r\n",9, 0);
        }
    }
    return SUCCESS;
}
uint8_t MSG_Uart_GetLastChar(void){
    return self.last_char;
}
static void
_uart_event_handler(app_uart_evt_t * evt){
    uint8_t c;
    switch(evt->evt_type){
        /**< An event indicating that UART data has been received. The data is available in the FIFO and can be fetched using @ref app_uart_get. */
        case APP_UART_DATA_READY:
            while(!app_uart_get(&c)){
                self.last_char = c;
                switch(c){
                    case '\r':
                    case '\n':
                        app_uart_put('\n');
                        app_uart_put('\r');
                        if(self.rx_buf){
                            self.rx_buf->buf[self.rx_index] = '\0';
                            //dispatch command to main context
                            self.parent->dispatch((MSG_Address_t){UART,1},(MSG_Address_t){CLI,0}, self.rx_buf);
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
                        //write directly on buffer
                        if(self.rx_buf && self.rx_index < self.rx_buf->len - 1){
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
    {
        uint16_t uart_id = 0;
        app_uart_buffers_t buffers = {0};
#ifdef PLATFORM_HAS_SERIAL_CROSS_CONNECT
        static uint8_t rx_buf[128];
        static uint8_t tx_buf[256];
#else
        static uint8_t rx_buf[16];
        static uint8_t tx_buf[256];
#endif
        buffers.rx_buf = rx_buf;
        buffers.rx_buf_size = sizeof(rx_buf);
        buffers.tx_buf = tx_buf;
        buffers.tx_buf_size = sizeof(tx_buf);
        err = app_uart_init(&self.uart_params, &buffers, _uart_event_handler, APP_IRQ_PRIORITY_LOW, &uart_id);
    }
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

void MSG_Uart_Printc(char c){
    if(self.initialized){
        app_uart_put(c);
    }
}

void MSG_Uart_Prints(const char * str){
    if(self.initialized){
        const char * head = str;
        while(*head){
            app_uart_put(*head);
            head++;
        }
    }
}

void MSG_Uart_PrintHex(const uint8_t * ptr, uint32_t len){
    ptr+=len-1;
    if(self.initialized){
        while(len-- >0) {
            app_uart_put(hex[0xF&(*ptr>>4)]);
            app_uart_put(hex[0xF&*ptr--]);
            app_uart_put(' ');
        }
    }
}

void MSG_Uart_PrintByte(const uint8_t * ptr, uint32_t len){
    ptr+=len-1;
    if(self.initialized){
        while(len-- >0) {
            app_uart_put(hex[0xF&(*ptr>>4)]);
            app_uart_put(hex[0xF&*ptr--]);
        }
    }
}

void MSG_Uart_PrintDec(const int * ptr, uint32_t len){
     uint8_t index,digit[10],count;
     uint32_t number;

     if(self.initialized){
         index = 0;
         count = len;
         number = *ptr;
         if( number < 0 ) {
             app_uart_put('-');
             number = -number;
         }

         while(number!=0) {
             digit[index++] = number % 10;
             number /= 10;
         }
         while(index-- >0) {
             
             app_uart_put(hex[0xF&(digit[index])]);
         }
    }
}
#include "stdarg.h"
void MSG_Uart_Printf(char * fmt, ... ) { //look, no buffer...
    va_list va_args;
    char * p = fmt;
    
    if(!self.initialized){ return; }
    va_start(va_args, fmt);
    
    while(*p) {
        switch (*p) {
            case '%': //control char
                ++p; //skip control char
                switch(*p) {
                    case 'x': { int x;
                        x = va_arg(va_args, int);
                        MSG_Uart_PrintHex((const uint8_t *)&x, sizeof(x));
                        break; }
                    case 'd': { int x;
                        x = va_arg(va_args, int);
                        MSG_Uart_PrintDec(&x, 10);
                        break; }
                    case 'l': { uint64_t x;
                        x = va_arg(va_args, uint64_t);
                        MSG_Uart_PrintHex(&x, sizeof(uint64_t));
                        break; }
                    case 's': { char * c;
                        c = va_arg(va_args, char*);
                        while( *c++ ) {
                            app_uart_put(*c);
                        }
                        break; }
                    default:
                        app_uart_put((uint8_t)'%');
                        app_uart_put(*p);

                }
                ++p; //skip control char
                break;
            default:
                app_uart_put(*p++); //print some of the format string and advance
        }
    }
    
    va_end(va_args);
}


