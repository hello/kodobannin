#include "message_sspi.h"
#include "util.h"

#define REG_READ 1
#define REG_WRITE 0
#define TEST_STR "RELLO"
typedef enum{
    IDLE = 0,
    READING,
    WRITING,
    ERROR
}SSPIState;

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    spi_slave_config_t config;
    volatile SSPIState current_state;
    uint8_t control_reg;
    /*
     * A transaction is a series of 3 CS pin transitions
     * That may not be interrupted
     */
    struct{
        enum{
            WAIT_READ_RX_LEN = 0,
            WRITE_TX_LEN,
            WAIT_READ_RX_BUF,
            WRITE_TX_BUF,
            FIN_READ,
            FIN_WRITE,
            TXRX_ERROR
        }state;
        uint16_t length_reg;
        MSG_Data_t * payload;
    }transaction;

    /*
     * Only one queue_tx right now
     */
    MSG_Data_t * queued_tx;
    MSG_Data_t * dummy;
}self;

static char * name = "SSPI";
static void _spi_evt_handler(spi_slave_evt_t event);

static SSPIState
_reset(void){
    PRINTS("RESET\r\n");
    spi_slave_buffers_set(&self.control_reg, &self.control_reg, 1, 1);
    return IDLE;
}
static MSG_Data_t *
_dequeue_tx(void){
    MSG_Data_t * ret = self.queued_tx;
#ifdef PLATFORM_HAS_SSPI
    if(SSPI_INT != 0){
        nrf_gpio_cfg_output(SSPI_INT);
        nrf_gpio_pin_write(SSPI_INT, 0);
    }
#endif
    self.queued_tx = NULL;
    return ret;
    //this function pops the tx queue for spi, for now, simply returns a new buffer with test code
    //return MSG_Base_AllocateStringAtomic(TEST_STR);
}
static uint32_t
_queue_tx(MSG_Data_t * o){
    if(self.queued_tx){
        //we are forced to drop since something shouldn't be here
        MSG_Base_ReleaseDataAtomic(self.queued_tx);
    }
    self.queued_tx = o;
#ifdef PLATFORM_HAS_SSPI
    if(SSPI_INT != 0){
        nrf_gpio_cfg_output(SSPI_INT);
        nrf_gpio_pin_write(SSPI_INT, 1);
    }
#endif
    
    return 0;

}

static SSPIState
_initialize_transaction(){
    switch(self.control_reg){
        default:
            PRINTS("IN UNKNOWN MODE\r\n");
            return _reset();
            break;
        case REG_READ:
            PRINTS("IN READ MODE\r\n");
            self.transaction.state = WAIT_READ_RX_LEN;
            self.transaction.payload = MSG_Base_AllocateDataAtomic(256);
            return READING;
        case REG_WRITE:
            PRINTS("IN WRITE MODE\r\n");
            //prepare buffer here
            self.transaction.length_reg = 43; //get from tx queue;
            self.transaction.payload = _dequeue_tx();
            self.transaction.state = WRITE_TX_LEN;
            return WRITING;
    }
}
static SSPIState
_handle_transaction(){
    switch(self.transaction.state){
        case WAIT_READ_RX_LEN:
            PRINTS("@WAIT RX LEN\r\n");
            spi_slave_buffers_set(&self.transaction.length_reg, &self.transaction.length_reg, sizeof(self.transaction.length_reg), sizeof(self.transaction.length_reg));
            self.transaction.state = WAIT_READ_RX_BUF;
            break;
        case WRITE_TX_LEN:
            PRINTS("@WRITE TX LEN\r\n");
            spi_slave_buffers_set(&self.transaction.length_reg,self.dummy->buf, sizeof(self.transaction.length_reg), sizeof(self.transaction.length_reg));
            self.transaction.state = WRITE_TX_BUF;
            break;
        case WAIT_READ_RX_BUF:
            PRINTS("@WAIT RX BUF\r\n");
            if(self.transaction.payload){
                //the context variable in MSG_Base_t will receive the first Byte(address) of the data, while the rest will receive the same
                //it'll be then routed to the correct module with addr->module translation module message_router
                spi_slave_buffers_set(self.transaction.payload->buf, &self.transaction.payload->context, self.transaction.length_reg, self.transaction.length_reg);
                self.transaction.state = FIN_READ;
                break;
            }else{
                //no buffer wat do?
            }
            return _reset();
        case WRITE_TX_BUF:
            PRINTS("@WRITE TX BUF\r\n");
            if(self.transaction.payload){
                spi_slave_buffers_set(self.transaction.payload->buf, self.dummy->buf, self.transaction.length_reg, self.transaction.length_reg);
                self.transaction.state = FIN_WRITE;
                break;
            }else{
                //no buffer wat do?
            }
            return _reset();
        case FIN_READ:
            PRINTS("@FIN RX BUF\r\n");
            //send and release
            return _reset();
        case FIN_WRITE:
            PRINTS("@FIN TX BUF\r\n");
            MSG_Base_ReleaseDataAtomic(self.transaction.payload);
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
    MSG_Base_ReleaseDataAtomic(self.dummy);
    return SUCCESS;
}
static MSG_Status
_flush(void){
    return SUCCESS;
}
static MSG_Status
_send(MSG_ModuleType src, MSG_Data_t * data){
    return SUCCESS;
}

static void
_spi_evt_handler(spi_slave_evt_t event){
    uint8_t swap;
    char t[3] = {0};
    uint32_t ret = 0;
    switch(event.evt_type){
        case SPI_SLAVE_BUFFERS_SET_DONE:
            break;
        case SPI_SLAVE_XFER_DONE:
            //do things with buffer
            switch(self.current_state){
                case IDLE:
                    self.current_state = _initialize_transaction();
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
    uint32_t ret;
    if(spi_slave_init(&self.config) || 
            spi_slave_evt_handler_register(_spi_evt_handler)){
        PRINTS("SPI FAIL");
        return FAIL;
    }
#ifdef PLATFORM_HAS_SSPI
    if(SSPI_INT != 0){
        nrf_gpio_cfg_output(SSPI_INT);
        nrf_gpio_pin_write(SSPI_INT, 0);
    }
#endif
    self.dummy = MSG_Base_AllocateDataAtomic(230);
    self.current_state = _reset();
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
