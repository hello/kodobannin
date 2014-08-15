#include <ant_interface.h>
#include <app_timer.h>
#include <app_error.h>
#include <ant_parameters.h>
#include "message_ant.h"
#include "util.h"
#include "app.h"
#include "crc16.h"
#include "antutil.h"

#define ANT_EVENT_MSG_BUFFER_MIN_SIZE 32u  /**< Minimum size of an ANT event message buffer. */
enum{
    ANT_DISCOVERY_CENTRAL = 0,
    ANT_DISCOVERY_PERIPHERAL
}ANT_DISCOVERY_ROLE;

typedef struct{
    MSG_Data_t * header;
    MSG_Data_t * payload;
    union{
        uint16_t idx;
        struct{
            uint8_t network;
            uint8_t type;
        }phy;
    };
    union{
        uint16_t count;
        uint16_t prev_crc;
    };
}ChannelContext_t;

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    uint8_t discovery_role;
    ChannelContext_t tx_channel_ctx[NUM_ANT_CHANNELS];
    ChannelContext_t rx_channel_ctx[NUM_ANT_CHANNELS];
    uint8_t discovery_message[8];
}self;
static char * name = "ANT";
#define CHANNEL_NUM_CHECK(ch) (ch < NUM_ANT_CHANNELS)

/**
 * Static declarations
 */
static uint16_t _calc_checksum(MSG_Data_t * data);
/**
 * can't find in api, so just store in rx ctx
 */
static uint8_t _get_channel_type(uint8_t channel);
/*
 * Closes the channel, context are freed at channel closure callback
 */
static MSG_Status _destroy_channel(uint8_t channel);

/* Allocates header based on the payload */
static MSG_Data_t * INCREF _allocate_header_tx(MSG_Data_t * payload);

/* Allocates header based on receiving header packet */
static MSG_Data_t * INCREF _allocate_header_rx(ANT_HeaderPacket_t * buf);

/* Allocates payload based on receiving header packet */
static MSG_Data_t * INCREF _allocate_payload_rx(ANT_HeaderPacket_t * buf);

/* Returns status of the channel */
static uint8_t _get_channel_status(uint8_t channel);

/* Entry point to handle receiving of discovery packet */
static void _handle_discovery(uint8_t channel, const uint8_t * message, const ANT_ChannelID_t * id);

/* Finds inverse of the channel type eg master -> slave */
static uint8_t _match_channel_type(uint8_t remote);

/* Finds an unassigned Channel */
static uint8_t _find_unassigned_channel(void);

/* Finds channel with matching id */
static uint8_t _match_channel(const ANT_ChannelID_t * id);

static uint8_t
_match_channel_type(uint8_t remote){
    switch(remote){
        case CHANNEL_TYPE_SLAVE:
            return CHANNEL_TYPE_MASTER;
        case CHANNEL_TYPE_MASTER:
            return CHANNEL_TYPE_SLAVE;
        case CHANNEL_TYPE_SHARED_SLAVE:
            return CHANNEL_TYPE_SHARED_MASTER;
        case CHANNEL_TYPE_SHARED_MASTER:
            return CHANNEL_TYPE_SHARED_SLAVE;
        case CHANNEL_TYPE_SLAVE_RX_ONLY:
            return CHANNEL_TYPE_MASTER_TX_ONLY;
        case CHANNEL_TYPE_MASTER_TX_ONLY:
            return CHANNEL_TYPE_SLAVE_RX_ONLY;
        default:
            return 0xFF;
    }
}
static uint8_t
_match_channel(const ANT_ChannelID_t * id){
    uint16_t dev_id;
    uint8_t type, xmit, i;
    for(i = 0; i < NUM_ANT_CHANNELS; i++){
        if(!sd_ant_channel_id_get(i,&dev_id, &type, &xmit)){
            if(i != ANT_DISCOVERY_CHANNEL && dev_id == id->device_number){
                return i;
            }
        }
    }
    return 0xFF;
}

static uint8_t
_find_unassigned_channel(void){
    uint8_t ret = 0xFF;
    uint8_t i;
    for(i = 0; i < NUM_ANT_CHANNELS; i++){
        if( i != ANT_DISCOVERY_CHANNEL && _get_channel_status(i) == STATUS_UNASSIGNED_CHANNEL){
            ret = i;
            break;
        }
    }
    return ret;
}
static MSG_Status
_destroy_channel(uint8_t channel){
    //this destroys the channel regardless of what state its in
    sd_ant_channel_close(channel);
    //sd_ant_channel_unassign(channel);
    return SUCCESS;
}
static
uint8_t _get_channel_type(uint8_t channel){
    return self.rx_channel_ctx[channel].phy.type;
}
/*
 *#define STATUS_UNASSIGNED_CHANNEL                  ((uint8_t)0x00) ///< Indicates channel has not been assigned.
 *#define STATUS_ASSIGNED_CHANNEL                    ((uint8_t)0x01) ///< Indicates channel has been assigned.
 *#define STATUS_SEARCHING_CHANNEL                   ((uint8_t)0x02) ///< Indicates channel is active and in searching state.
 *#define STATUS_TRACKING_CHANNEL                    ((uint8_t)0x03) ///< Indicates channel is active and in tracking state.
 */
static uint8_t
_get_channel_status(uint8_t channel){
    uint8_t ret;
    sd_ant_channel_status_get(channel, &ret);
    return ret;
}
static MSG_Data_t * INCREF
_allocate_header_tx(MSG_Data_t * payload){
    MSG_Data_t * ret = MSG_Base_AllocateDataAtomic(8);
    if(ret){
        ANT_HeaderPacket_t * header = ret->buf;
        header->page  = 0;
        header->page_count = payload->len / 6 + ( ((payload->len)%6)?1:0);
        /*
         *header->src_mod = 0;
         *header->src_submod = 0;
         */
        header->reserved0 = 0;
        header->reserved1 = 0;
        header->size = payload->len;
        header->checksum = _calc_checksum(payload);
    }
    return ret;
}
static MSG_Data_t * INCREF
_allocate_header_rx(ANT_HeaderPacket_t * buf){
    MSG_Data_t * ret = MSG_Base_AllocateDataAtomic(sizeof(ANT_HeaderPacket_t));
    if(ret){
        memcpy(ret->buf, buf, sizeof(ANT_HeaderPacket_t));
    }
    return ret;
}
static void DECREF
_free_context(ChannelContext_t * ctx){
    MSG_Base_ReleaseDataAtomic(ctx->header);
    MSG_Base_ReleaseDataAtomic(ctx->payload);
    ctx->header = NULL;
    ctx->payload = NULL;
}
static uint16_t
_calc_checksum(MSG_Data_t * data){
    uint16_t ret;
    ret = crc16_compute(data->buf, data->len, NULL);
    PRINTS("CS: ");
    PRINT_HEX(&ret,2);
    PRINTS("\r\n");
    return ret;
}
static MSG_Data_t *
_allocate_payload_rx(ANT_HeaderPacket_t * buf){
    MSG_Data_t * ret;
    PRINTS("Pages: ");
    PRINT_HEX(&buf->page_count, 1);
    PRINTS("\r\n");
    ret = MSG_Base_AllocateDataAtomic( 6 * buf->page_count );
    if(ret){
        //readjusting length of the data, previously
        //allocated in multiples of page_size to make assembly easier
        uint16_t ss = ((uint8_t*)buf)[5] << 8;
        ss += ((uint8_t*)buf)[4];
        ret->len = ss;
        PRINTS("OBJ LEN\r\n");
        PRINT_HEX(&ret->len, 2);
    }
    return ret;
}
static MSG_Status
_connect(uint8_t channel){
    //needs at least assigned
    MSG_Status ret = FAIL;
    uint8_t inprogress;
    uint8_t pending;
    sd_ant_channel_status_get(channel, &inprogress);
    if(!sd_ant_channel_open(channel)){
        ret = SUCCESS;
    }
    return ret;
}

static MSG_Status
_disconnect(uint8_t channel){
    if(!sd_ant_channel_close(channel)){
        return SUCCESS;
    }
    return FAIL;
}

static MSG_Status
_configure_channel(uint8_t channel, const ANT_ChannelPHY_t * spec, const ANT_ChannelID_t * id, uint8_t  ext_fields){
    uint32_t ret = 0;
    if(_get_channel_status(channel)){
        ret = _destroy_channel(channel);
    }
    if(!ret){
        ret += sd_ant_channel_assign(channel, spec->channel_type, spec->network, ext_fields);
        ret += sd_ant_channel_radio_freq_set(channel, spec->frequency);
        ret += sd_ant_channel_period_set(channel, spec->period);
        self.rx_channel_ctx[channel].phy.network = spec->network;
        self.rx_channel_ctx[channel].phy.type = spec->channel_type;
        ret += sd_ant_channel_id_set(channel, id->device_number, id->device_type, id->transmit_type);
        ret += sd_ant_channel_low_priority_rx_search_timeout_set(channel, 0xFF);
        ret += sd_ant_channel_rx_search_timeout_set(channel, 0);
    }
    return ret?FAIL:SUCCESS;
}
static void
_handle_discovery(uint8_t channel, const uint8_t * obj, const ANT_ChannelID_t * id){
    PRINTS("Found ID = ");
    PRINT_HEX(&(id->device_number), 2);
    switch(self.discovery_role){
        case ANT_DISCOVERY_CENTRAL:
            break;
        case ANT_DISCOVERY_PERIPHERAL:
            break;
        default:
            return;
    }
}

static MSG_Status
_set_discovery_mode(uint8_t role){
    ANT_ChannelID_t id = {0};
    ANT_ChannelPHY_t phy = { 
        .period = 273,
        .frequency = 66,
        .channel_type = CHANNEL_TYPE_SLAVE,
        .network = 0};
    switch(role){
        case ANT_DISCOVERY_CENTRAL:
            //central mode
            PRINTS("CENTRAL\r\n");
            //close all channels
            APP_OK(_configure_channel(ANT_DISCOVERY_CHANNEL, &phy, &id, 0));
            APP_OK(sd_ant_lib_config_set(ANT_LIB_CONFIG_MESG_OUT_INC_DEVICE_ID | ANT_LIB_CONFIG_MESG_OUT_INC_RSSI | ANT_LIB_CONFIG_MESG_OUT_INC_TIME_STAMP));
            sd_ant_rx_scan_mode_start(0);
            break;
        case ANT_DISCOVERY_PERIPHERAL:
            //peripheral mode
            //configure shit here
            PRINTS("PERIPHERAL\r\n");
            phy.channel_type = CHANNEL_TYPE_MASTER;
            id.device_number = GET_UUID_16();
            id.device_type = ANT_HW_TYPE;
            id.transmit_type = 0;
            phy.frequency = 32768;
            _configure_channel(ANT_DISCOVERY_CHANNEL, &phy, &id, 0);
            _connect(ANT_DISCOVERY_CHANNEL);
            break;
        default:
            break;
    }
    return SUCCESS;
}

static void
_handle_channel_closure(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    PRINTS("Channel Close: ");
    PRINT_HEX(channel, 1);
    PRINTS("\r\n");
    if(*channel == ANT_DISCOVERY_CHANNEL && self.discovery_role == ANT_DISCOVERY_CENTRAL){
        PRINTS("Re-opening Channel ");
        PRINT_HEX(channel, 1);
        PRINTS("\r\n");
        //accept discovery even if its new
        MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_SET_ROLE, &self.discovery_role, 1);
    }else if(*channel == ANT_DISCOVERY_CHANNEL){
        //discovery timesout to prevent power loss
        self.discovery_role = 0xFF;
    }else{
        uint8_t type = self.rx_channel_ctx[*channel].phy.type;
        if(type == CHANNEL_TYPE_SLAVE || type == CHANNEL_TYPE_SHARED_SLAVE || type == CHANNEL_TYPE_SLAVE_RX_ONLY){
            _connect(*channel);
        }
    }
    self.rx_channel_ctx[*channel].prev_crc = 0x00;
    _free_context(&self.rx_channel_ctx[*channel]);
    _free_context(&self.tx_channel_ctx[*channel]);
}
static void
_assemble_payload(ChannelContext_t * ctx, ANT_PayloadPacket_t * packet){
    //technically if checksum is xor, is possible to do incremental xor to 
    //find out if the data is valid without doing it at the header packet
    //but for simplicity's sake, lets just leave the optomizations later...
    uint16_t offset = (packet->page - 1) * 6;
    if(ctx->payload){
        ctx->payload->buf[offset] = packet->payload[0];
        ctx->payload->buf[offset + 1] = packet->payload[1];
        ctx->payload->buf[offset + 2] = packet->payload[2];
        ctx->payload->buf[offset + 3] = packet->payload[3];
        ctx->payload->buf[offset + 4] = packet->payload[4];
        ctx->payload->buf[offset + 5] = packet->payload[5];
    }
//    PRINT_HEX(&ctx->payload->buf, ctx->payload->len);
    PRINTS("LEN: | ");
    PRINT_HEX(&ctx->payload->len, sizeof(ctx->payload->len));
    PRINTS(" |");
    PRINTS("\r\n");

}
static uint8_t
_integrity_check(ChannelContext_t * ctx){
    if(ctx->header && ctx->payload){
        ANT_HeaderPacket_t * cmp = ctx->header->buf;
        if(_calc_checksum(ctx->payload) == cmp->checksum){
            return 1;
        }
    }
    return 0;
}
static MSG_Data_t * 
_assemble_rx(uint8_t channel, ChannelContext_t * ctx, uint8_t * buf, uint32_t buf_size){
    MSG_Data_t * ret = NULL;
    if(buf[0] == 0 && buf[1] > 0){
        //scenarios
        //either header file is a new one
        //or header file is the old one
        uint16_t new_crc = (uint16_t)(buf[7] << 8) + buf[6];
        PRINTS("H");
        ANT_HeaderPacket_t * header = (ANT_HeaderPacket_t *)buf;
        if(new_crc != ctx->prev_crc){
            _free_context(ctx);
            ctx->header = _allocate_header_rx(header);
            ctx->payload = _allocate_payload_rx(header);
            ctx->prev_crc = new_crc;
        }else if(_integrity_check(ctx)){
            ret = ctx->payload;
            MSG_Base_AcquireDataAtomic(ret);
            _free_context(ctx);
        }else{
            //Repeated Message
        }
    }else if(buf[0] <= buf[1] && buf[1] > 0){
        if(ctx->header){
            _assemble_payload(ctx, (ANT_PayloadPacket_t *)buf);
        }else{
            //here in the future create a buffer based on pages
        }
    }else{
        //unknown packet, just drop everything and possibly close channel
        _free_context(ctx);
    }
    return ret;
}
static uint8_t DECREF
_assemble_tx(ChannelContext_t * ctx, uint8_t * out_buf, uint32_t buf_size){
    ANT_HeaderPacket_t * header = ctx->header->buf;
    if(ctx->count == 0){
        memcpy(out_buf, ctx->header->buf, 8);
        return 0;
    }else{
        if(ctx->idx == 0){
            memcpy(out_buf, header, sizeof(*header));
        }else{
            uint16_t offset = (ctx->idx - 1) * 6;
            int i;
            out_buf[0] = ctx->idx;
            out_buf[1] = header->page_count;
            for(i = 0; i < 6; i++){
                if( offset + i < ctx->payload->len ){
                    out_buf[2+i] = ctx->payload->buf[offset+i];
                }else{
                    out_buf[2+i] = 0;
                }
            }
        }
        if(++ctx->idx > header->page_count){
            ctx->idx = 0;
            ctx->count--;
        }
    }
    return 1;

}

static void
_handle_tx(uint8_t * channel, uint8_t * buf, uint8_t buf_size, uint8_t is_slave){
    uint8_t message[ANT_STANDARD_DATA_PAYLOAD_SIZE];
    uint32_t ret;
    ChannelContext_t * ctx = &self.tx_channel_ctx[*channel];
    if(!ctx->header || !ctx->payload){
        if(is_slave){
            return;
        }else{
            _destroy_channel(*channel);
            return;
        }
    }else if(!_assemble_tx(ctx, message, ANT_STANDARD_DATA_PAYLOAD_SIZE)){
        PRINTS("FIN\r\n");
        _destroy_channel(*channel);
    }
    ret = sd_ant_broadcast_message_tx(*channel,sizeof(message), message);
}
static void
_handle_rx(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    ANT_MESSAGE * msg = (ANT_MESSAGE*)buf;
    uint8_t * rx_payload = msg->ANT_MESSAGE_aucPayload;
    ChannelContext_t * ctx = &self.rx_channel_ctx[*channel];
    if(*channel == ANT_DISCOVERY_CHANNEL && self.discovery_role == ANT_DISCOVERY_CENTRAL){
        EXT_MESG_BF ext = msg->ANT_MESSAGE_sExtMesgBF;
        uint8_t * extbytes = msg->ANT_MESSAGE_aucExtData;
        /*
         *PRINT_HEX(extbytes, buf_size);
         *PRINTS("\r\n");
         */
        if(ext.ucExtMesgBF & MSG_EXT_ID_MASK){
            ANT_ChannelID_t id = (ANT_ChannelID_t){
                .device_number = *((uint16_t*)extbytes),
                .device_type = extbytes[4],
                .transmit_type = extbytes[3],
            };
            _handle_discovery(*channel, rx_payload, &id);
        }
    }
    MSG_Data_t * ret = _assemble_rx(*channel, ctx, rx_payload, buf_size);
    if(ret){
        PRINTS("GOT A NEw MESSAGE OMFG\r\n");
        {
            MSG_Address_t src = (MSG_Address_t){ANT, channel+1};
            MSG_Address_t dst = (MSG_Address_t){UART, 1};
            //then dispatch to uart for printout
            self.parent->dispatch(src, dst, ret);
        }
        MSG_Base_ReleaseDataAtomic(ret);
    }
    //PRINTS("\r\n");
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
    if(!data){
        return FAIL;
    }
    if(dst.submodule == 0){
        MSG_ANTCommand_t * antcmd = (MSG_ANTCommand_t*)data->buf;
        switch(antcmd->cmd){
            default:
            case ANT_PING:
                PRINTS("ANT_PING\r\n");
                break;
            case ANT_SET_ROLE:
                PRINTS("ANT_SET_ROLE\r\n");
                PRINT_HEX(&antcmd->param.role, 0x1);
                return _set_discovery_mode(antcmd->param.role);
            case ANT_CREATE_CHANNEL:
                {
                    PRINTS("Create Channel\r\n");
                    uint8_t och = _match_channel(&antcmd->param.settings.id);
                    if(och < NUM_ANT_CHANNELS){
                        //Channel already establshed
                        PRINTS("Channel Already Established\r\n");
                        return SUCCESS;
                    }
                    uint8_t ch = _find_unassigned_channel();
                    if(ch < NUM_ANT_CHANNELS){
                        uint32_t ret;
                        PRINTS("CH = ");
                        PRINT_HEX(&ch, 1);
                        _free_context(&self.rx_channel_ctx[ch]);
                        _free_context(&self.tx_channel_ctx[ch]);
                        ret = _configure_channel(ch, &antcmd->param.settings.phy, &antcmd->param.settings.id);
                        ret = _connect(ch);
                    }
                }
                break;
        }
    }else{
        uint8_t channel = dst.submodule - 1;
        PRINTS("SENDING TO CH: ");
        PRINT_HEX(&channel, 1);
        if(_get_channel_status(channel) == STATUS_UNASSIGNED_CHANNEL){
            PRINTS("CH not configured\r\n");
            return FAIL;
        }
        ChannelContext_t * ctx = &self.tx_channel_ctx[channel];
        if(!ctx->header && !ctx->payload){
            //channel is ready to transmit
            MSG_Base_AcquireDataAtomic(data);
            ctx->payload = data;
            ctx->header = _allocate_header_tx(ctx->payload);
            ctx->count = 6;
            if(ctx->header){
                _connect(channel);
            }else{
                PRINTS("Header Allocation Failed\r\n");
                _free_context(ctx);
            }
        }else{
            PRINTS("CH Busy\r\n");
        }

    }
    return SUCCESS;
}

static MSG_Status
_init(){
    uint32_t ret;
    uint8_t network_key[8] = {0,0,0,0,0,0,0,0};
    ret = sd_ant_stack_reset();
    ret += sd_ant_network_address_set(0,network_key);
    /* Exclude list, don't scan for canceled devices */
    //ret += sd_ant_id_list_config(ANT_DISCOVERY_CHANNEL, 4, 1);
    if(!ret){
        return SUCCESS;
    }else{
        return FAIL;
    }
}
MSG_Base_t * MSG_ANT_Base(MSG_Central_t * parent){
    self.parent = parent;
    {
        self.base.init =  _init;
        self.base.flush = _flush;
        self.base.send = _send;
        self.base.destroy = _destroy;
        self.base.type = ANT;
        self.base.typestr = name;
    }
    return &self.base;

}
void ant_handler(ant_evt_t * p_ant_evt){
    uint8_t event = p_ant_evt->event;
    uint8_t ant_channel = p_ant_evt->channel;
    uint32_t * event_message_buffer = p_ant_evt->evt_buffer;
    switch(event){
        case EVENT_RX_FAIL:
            //PRINTS("FRX\r\n");
            break;
        case EVENT_RX:
            PRINTS("R");
            _handle_rx(&ant_channel,event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            if(_get_channel_type(ant_channel) == CHANNEL_TYPE_SLAVE ||
                    _get_channel_type(ant_channel) == CHANNEL_TYPE_SHARED_SLAVE){
                _handle_tx(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE, 1);
            }
            break;
        case EVENT_RX_SEARCH_TIMEOUT:
            PRINTS("RXTO\r\n");
            break;
        case EVENT_RX_FAIL_GO_TO_SEARCH:
            _destroy_channel(ant_channel);
            PRINTS("RXFTS\r\n");
            break;
        case EVENT_TRANSFER_RX_FAILED:
            PRINTS("RFFAIL\r\n");
            break;
        case EVENT_TX:
            PRINTS("T");
            _handle_tx(&ant_channel,event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE, 0);
            break;
        case EVENT_TRANSFER_TX_FAILED:
            break;
        case EVENT_CHANNEL_COLLISION:
            PRINTS("XX\r\n");
            break;
        case EVENT_CHANNEL_CLOSED:
            _handle_channel_closure(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        default:
            {
                PRINT_HEX(&event,1);
            }
            break;
    }

}
uint8_t MSG_ANT_BondCount(void){
    uint8_t i, ret = 0;
    for(i = 0; i < NUM_ANT_CHANNELS; i++){
        if(i != ANT_DISCOVERY_CHANNEL){
            if(_get_channel_status(i) > 0){
                ret++;
            }
        }
    }
    return ret;
}
