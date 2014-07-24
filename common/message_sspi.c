#include "message_sspi.h"
static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    spi_slave_config_t config;
}self;
static name = "SSPI";

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
_evt_handler(spi_slave_evt event){
    switch(event.evt_type){
        case SPI_SLAVE_BUFFER_SET_DONE:
            break;
        case SPI_SLAVE_XFER_DONE:
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
    ret = spi_slave_init(&self.config);
    if(!ret){
        //LOG
        return FAIL;
    }
    spi_slave_evt_handler_register(_evt_handler);
    return ret;

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
