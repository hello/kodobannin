#include "message_sspi.h"
#include "util.h"

#define REG_READ 1
#define REG_WRITE 0
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
}self;

static char * name = "SSPI";
static void _spi_evt_handler(spi_slave_evt_t event);

static SSPIState
_reset(void){
    PRINTS("RESET\r\n");
    spi_slave_buffers_set(&self.control_reg, &self.control_reg, 1, 1);
    return IDLE;
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
            return READING;
        case REG_WRITE:
            PRINTS("IN WRITE MODE\r\n");
            //prepare buffer here
            /*MSG_Data_t * txctx = get_tx_queue();*/
            self.transaction.length_reg = 0; //get from tx queue;
            self.transaction.payload = NULL;
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
            spi_slave_buffers_set(&self.transaction.length_reg,&self.transaction.length_reg, sizeof(self.transaction.length_reg), sizeof(self.transaction.length_reg));
            self.transaction.state = WRITE_TX_BUF;
            break;
        case WAIT_READ_RX_BUF:
            PRINTS("@WAIT RX BUF\r\n");
            self.transaction.payload = MSG_Base_AllocateDataAtomic(self.transaction.length_reg);
            if(self.transaction.payload){
                spi_slave_buffers_set(self.transaction.payload->buf, self.transaction.payload->buf, self.transaction.length_reg, self.transaction.length_reg);
                self.transaction.state = FIN_READ;
                break;
            }else{
                //no buffer wat do?
            }
            //fall through to reset
        case WRITE_TX_BUF:
            //fall through to reset
        case FIN_READ:
            PRINTS("@FIN RX BUF\r\n");
            //send and release
            MSG_Base_ReleaseDataAtomic(self.transaction.payload);
            //fall through to reset
        case FIN_WRITE:
            //fall through to reset
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
