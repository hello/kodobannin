#include "message_sspi.h"
#include "util.h"

enum{
    READ_REQ = 0,
    WRITE_REQ
}SSPI_State_Req;

typedef enum{
    IDLE = 0,
    READ_RX_LEN,
    READING,
    WRITE_TX_LEN,
    WRITING,
    ERROR = 0xFF
}SSPIState;

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    spi_slave_config_t config;
#ifdef MSG_SSPI_TXRX_BUFFER_SIZE
    uint8_t tx_buf[MSG_SSPI_TX_BUFFER_SIZE+1];
    uint8_t rx_buf[MSG_SSPI_RX_BUFFER_SIZE+1];
#else
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];
#endif
    SSPIState current_state;
    struct{
        MSG_Data_t * tx_obj;
        uint16_t offset;
    }current_tx_context;
}self;

static char * name = "SSPI";
static void _spi_evt_handler(spi_slave_evt_t event);


static SSPIState
_change_state(SSPIState s, uint8_t * r){
    switch(s){
        case IDLE:
            if(r[0] == WRITE_REQ) return WRITE_TX_LEN;
            if(r[0] == READ_REQ) return READ_RX_LEN;
            return ERROR;
        case READ_RX_LEN:
            return READING;
        case WRITE_TX_LEN:
            return WRITING;
        case READING:
        case WRITING:
            return IDLE;
    }
}
static uint32_t
_spi_reload_buffers(SSPIState s){
    switch(s){
        case IDLE:
            return spi_slave_buffers_set(self.tx_buf, self.rx_buf, 1,1);
        case READ_RX_LEN:
        case WRITE_TX_LEN:
            return spi_slave_buffers_set(self.tx_buf, self.rx_buf, 2,2);
        default:
            return spi_slave_buffers_set(self.tx_buf, self.rx_buf, sizeof(self.tx_buf),sizeof(self.rx_buf));
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
            self.current_state = _change_state(self.rx_buf, self.current_state);
            //do things with buffer
            //reload
            _spi_reload_buffers(self.current_state);
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
    self.current_state = IDLE;
    _spi_reload_buffers(self.current_state);
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
