#include "message_sspi.h"
#include <nrf_gpio.h>
#include "util.h"
#include <string.h>
#include "hlo_queue.h"

#define REG_READ_FROM_SSPI  0
#define REG_WRITE_TO_SSPI 1
typedef enum{
    IDLE = 0,
    READING,
    WRITING,
    ERROR
}SSPIState;

typedef struct{
    MSG_Data_t * msg;
    MSG_Address_t address;
}queue_message_t;

static struct{
    MSG_Base_t base;
    const MSG_Central_t * parent;
    spi_slave_config_t config;
    volatile SSPIState current_state;
    uint8_t control_reg;
    /*
     * A transaction is a series of 3 CS pin transitions
     * That may not be interrupted
     */
    struct{
        enum{
            WAIT_READ_RX_CTX = 0,
            WRITE_TX_CTX,
            WAIT_READ_RX_BUF,
            WRITE_TX_BUF,
            FIN_READ,
            FIN_WRITE,
            TXRX_ERROR
        }state;
        struct{
            uint16_t length;
            union{
                MSG_Address_t address;
                uint16_t pad;
            };
        }context_reg PACKED;
        MSG_Data_t * payload;
    }transaction;

    /*
     * Only one queue_tx right now
     */
    uint8_t dummy[4];
    hlo_queue_t * tx_queue;
}self;

static char * name = "SSPI";
static void _spi_evt_handler(spi_slave_evt_t event);

static SSPIState
_reset(void){
    /*
     *PRINTS("RESET\r\nWaiting Input....\r\n");
     */
    memset(&self.transaction.context_reg, 0, sizeof(self.transaction.context_reg));
    spi_slave_buffers_set(&self.control_reg, &self.control_reg, 1, 1);
    return IDLE;
}
static uint32_t
_dequeue_tx(queue_message_t * out_msg){
    uint32_t ret = hlo_queue_read(self.tx_queue, (unsigned char *)out_msg, sizeof(*out_msg));
#ifdef PLATFORM_HAS_SSPI
    if(ret != NRF_SUCCESS && SSPI_INT != 0){
        DEBUGS("spi_low\r\n");
        nrf_gpio_pin_clear(SSPI_INT);
    }
#endif
    return ret;
}
static uint32_t
_queue_tx(MSG_Data_t * o, MSG_Address_t address){
    queue_message_t msg = {
        .msg = o,
        .address = address,
    };
    if( hlo_queue_empty_size(self.tx_queue) >= sizeof(msg) ){
        hlo_queue_write(self.tx_queue, (unsigned char*)&msg, sizeof(msg));
    }else{
        return 1;
    }
#ifdef PLATFORM_HAS_SSPI
    if(SSPI_INT != 0){
        DEBUGS("spi_high\r\n");
        nrf_gpio_pin_set(SSPI_INT);
    }
#endif
    return 0;

}

static SSPIState
_initialize_transaction(){
    switch(self.control_reg){
        default:
            /*
             *PRINTS("IN UNKNOWN MODE\r\n");
             */
            return _reset();
            break;
        case REG_WRITE_TO_SSPI:
            /*
             *PRINTS("READ FROM MASTER\r\n");
             */
            self.transaction.state = WAIT_READ_RX_CTX;
            return READING;
        case REG_READ_FROM_SSPI:
            //PRINTS("WRITE TO MASTER\r\n");
            //prepare buffer here
            {
                queue_message_t msg = {0};
                if(NRF_SUCCESS == _dequeue_tx(&msg)){
                    self.transaction.payload = msg.msg;
                    self.transaction.context_reg.address = msg.address;
                }
            }
            if(self.transaction.payload){
                //change to payload address
                self.transaction.context_reg.length = self.transaction.payload->len; //get from tx queue;
            }else{
                /*
                 *PRINTS("DQFr\n");
                 */
                self.transaction.context_reg.length = 0; //get from tx queue;
                self.transaction.context_reg.address = (MSG_Address_t){0,0};
            }
            self.transaction.state = WRITE_TX_CTX;
            return WRITING;
    }
}
static SSPIState
_handle_transaction(){
    switch(self.transaction.state){
        case WAIT_READ_RX_CTX:
            /*
             *PRINTS("@WAIT RX LEN\r\n");
             */
            spi_slave_buffers_set((uint8_t*)&self.transaction.context_reg, (uint8_t*)&self.transaction.context_reg, sizeof(self.transaction.context_reg), sizeof(self.transaction.context_reg));
            self.transaction.state = WAIT_READ_RX_BUF;
            break;
        case WRITE_TX_CTX:
            /*
             *PRINTS("@WRITE TX LEN\r\n");
             */
            spi_slave_buffers_set((uint8_t*)&self.transaction.context_reg, self.dummy, sizeof(self.transaction.context_reg), sizeof(self.dummy));
            self.transaction.state = WRITE_TX_BUF;
            break;
        case WAIT_READ_RX_BUF:
            /*
             *PRINTS("@WAIT RX BUF\r\n");
             */
            self.transaction.payload = MSG_Base_AllocateDataAtomic(self.transaction.context_reg.length);
            if(self.transaction.payload){
                spi_slave_buffers_set(self.transaction.payload->buf, self.transaction.payload->buf, self.transaction.context_reg.length, self.transaction.context_reg.length);
                self.transaction.state = FIN_READ;
            }else{
                //no buffer wat do?
                spi_slave_buffers_set(self.dummy, self.dummy, 0, 0);
                self.transaction.state = FIN_READ;
            }
            break;
        case WRITE_TX_BUF:
            /*
             *PRINTS("@WRITE TX BUF\r\n");
             */
            if(self.transaction.payload){
                spi_slave_buffers_set(self.transaction.payload->buf, self.dummy, self.transaction.context_reg.length, sizeof(self.dummy));
                self.transaction.state = FIN_WRITE;
            }else{
                //no buffer wat do?
                spi_slave_buffers_set(self.dummy, self.dummy, 0, 0);
                self.transaction.state = FIN_WRITE;
            }
            break;
        case FIN_READ:
            /*
             *PRINTS("@FIN RX BUF\r\n");
             *PRINTS("LEN = ");
             *PRINT_HEX(&self.transaction.context_reg.length, sizeof(uint16_t));
             *PRINTS(" ADDR  = ");
             *PRINT_HEX(&self.transaction.context_reg.pad, sizeof(uint16_t));
             */
            //send and release
            self.parent->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){BLE, 1}, self.transaction.payload);
            //self.parent->dispatch( (MSG_Address_t){SSPI, 1}, (MSG_Address_t){UART, 1}, self.transaction.payload);

            if(self.transaction.payload){
                MSG_Base_ReleaseDataAtomic(self.transaction.payload);
                self.transaction.payload = NULL;
            }
            //only releasing now
            return _reset();
        case FIN_WRITE:
            /*
             *PRINTS("@FIN TX BUF\r\n");
             *PRINTS("LEN = ");
             *PRINT_HEX(&self.transaction.context_reg.length, sizeof(uint16_t));
             *PRINTS(" ADDR  = ");
             *PRINT_HEX(&self.transaction.context_reg.pad, sizeof(uint16_t));
             */
            //send and release
            if(self.transaction.payload){
                MSG_Base_ReleaseDataAtomic(self.transaction.payload);
                self.transaction.payload = NULL;
            }
            return _reset();
        case TXRX_ERROR:
            //fall through to reset
        default:
            //fall through to reset
            return _reset();
    }
    return self.current_state;

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
        switch(dst.submodule){
            case 0:
                //fallthrough for backward compatibility
            case 1:
                if(0 == _queue_tx(data, ADDR(0, 0))){
                    MSG_Base_AcquireDataAtomic(data);
                }else{
                    return FAIL;
                }
                break;
            default:
                if(0 == _queue_tx(data, ADDR(0, dst.submodule))){
                    MSG_Base_AcquireDataAtomic(data);
                }else{
                    return FAIL;
                }
                break;
        }
    }
    return SUCCESS;
}

static void
_spi_evt_handler(spi_slave_evt_t event){
    switch(event.evt_type){
        case SPI_SLAVE_BUFFERS_SET_DONE:
            break;
        case SPI_SLAVE_XFER_DONE:
            //do things with buffer
            switch(self.current_state){
                case IDLE:
                    self.current_state = _initialize_transaction();
                    if(self.current_state == IDLE) break;
                case WRITING:
                case READING:
                    self.current_state = _handle_transaction();
                    break;
                default:
                    self.current_state = _reset();
                    break;
            }
            //reload
            break;
        default:
        case SPI_SLAVE_EVT_TYPE_MAX:
            //unknown???
            break;
    }
}
static MSG_Status
_init(){
    if(spi_slave_init(&self.config) || 
            spi_slave_evt_handler_register(_spi_evt_handler)){
        /*
         *PRINTS("SPI FAIL");
         */
        return FAIL;
    }
#ifdef PLATFORM_HAS_SSPI
    if(SSPI_INT != 0){
        /*
         *PRINTS("LOW");
         */
        nrf_gpio_cfg_output(SSPI_INT);
        nrf_gpio_pin_clear(SSPI_INT);
    }
#endif
    self.current_state = _reset();
    self.tx_queue = hlo_queue_init(64);
    APP_ASSERT(self.tx_queue);
    return SUCCESS;

}

MSG_Base_t * MSG_SSPI_Base(const spi_slave_config_t * p_spi_slave_config, const MSG_Central_t * parent){
    if(!p_spi_slave_config){
        return NULL;
    }
    self.config = *p_spi_slave_config;
    self.parent = parent;
    {
        self.base.init =  _init;
        self.base.flush = _flush;
        self.base.send = _send;
        self.base.destroy = _destroy;
        self.base.type = SSPI;
        self.base.typestr = name;
    }
    return &self.base;
}
