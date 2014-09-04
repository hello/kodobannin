#ifdef ANT_STACK_SUPPORT_REQD  // This is a temp fix because we need to compile on 51822 S110

#include <ant_interface.h>
#include <ant_parameters.h>

#include <app_timer.h>
#include <app_error.h>

#include "message_ant.h"
#include "util.h"
#include "app.h"
#include "crc16.h"
#include "antutil.h"
#include "ant_devices.h"

#define ANT_EVENT_MSG_BUFFER_MIN_SIZE 32u  /**< Minimum size of an ANT event message buffer. */
#define ANT_SESSION_NUM 3

typedef struct{
    ANT_HeaderPacket_t header;
    MSG_Data_t * payload;
    union{
        //tx
        int16_t idx;
        //rx
        struct{
            uint8_t network;
            uint8_t type;
        }phy;
    };
    union{
        //tx
        int16_t count;
    };
}ConnectionContext_t;

typedef struct{
    ANT_ChannelID_t id;
    ConnectionContext_t rx_ctx;
    ConnectionContext_t tx_ctx;
    MSG_Queue_t * tx_queue;
}ANT_Session_t;

static struct{
    MSG_Base_t base;
    MSG_Central_t * parent;
    uint8_t discovery_role;
    uint8_t discovery_action;
    ANT_Session_t sessions[ANT_SESSION_NUM];
    uint32_t sent, rcvd;
    //test
    uint32_t expected;
    uint32_t total;
    uint32_t matched;
}self;

static char * name = "ANT";

/**
 * Static declarations
 */
static uint16_t _calc_checksum(MSG_Data_t * data);
/*
 * Closes the channel, context are freed at channel closure callback
 */
static MSG_Status _destroy_channel(uint8_t channel);

/* Allocates payload based on receiving header packet */
static MSG_Data_t * INCREF _allocate_payload_rx(ANT_HeaderPacket_t * buf);

/* Returns status of the channel */
static uint8_t _get_channel_status(uint8_t channel);

/* Finds inverse of the channel type eg master -> slave */
static uint8_t _match_channel_type(uint8_t remote);

/* Finds an unassigned Channel */
static ANT_Session_t * _find_unassigned_session(uint8_t * out_channel);

/* Finds session by id */
static ANT_Session_t * _get_session_by_id(const ANT_ChannelID_t * id);

/* prepares tx context */
static void _prepare_tx(ConnectionContext_t * ctx, MSG_Data_t * d);

/* handles single packet special functions */
static MSG_Data_t * INCREF _handle_ant_single_packet(ConnectionContext_t * ctx, uint8_t type, uint8_t * payload);

/* handle unknown device(not paird) indicated by 0 0 header */
static void _handle_unknown_device(const ANT_ChannelID_t * id);

static void
_handle_unknown_device(const ANT_ChannelID_t * id){
    switch(self.discovery_action){
        default:
        case ANT_DISCOVERY_NO_ACTION:
            break;
        case ANT_DISCOVERY_REPORT_DEVICE:
            {
                PRINTS("Found Unknown ID = ");
                PRINT_HEX(&id->device_number, 2);
                PRINTS("\r\n");
                PRINTS("No Action Needed\r\n");
            }
            //send message to spi and ble and uart
            break;
        case ANT_DISCOVERY_ACCEPT_NEXT_DEVICE:
            {
                //create session
                self.discovery_action = ANT_DISCOVERY_REPORT_DEVICE;
                MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_CREATE_SESSION, id, sizeof(*id));
            }
            break;
        case ANT_DISCOVERY_ACCEPT_ALL_DEVICE:
            //create session
            break;
    }
}
static MSG_Data_t * INCREF
_handle_ant_single_packet(ConnectionContext_t * ctx, uint8_t type, uint8_t * payload){
    MSG_Data_t * ret = NULL;
    switch(type){
        default:
        case ANT_FUNCTION_NULL:
            PRINTS("Unknown Function\r\n");
            break;
        case ANT_FUNCTION_TEST:
            {
                uint32_t rcvd = payload[0];
                if(self.total == 0){
                    PRINTS("Test Init!\r\n");
                    self.matched++;
                }else if(rcvd == self.expected){
                    self.matched++;
                }
                self.total++;
                self.expected = rcvd+1;
                PRINTS("M: ");
                PRINT_HEX(&self.matched, 4);
                PRINTS(" T: ");
                PRINT_HEX(&self.total, 4);
                PRINTS("\r\n");
            }
            break;
        case ANT_FUNCTION_END:
            break;
    }
    return ret;
}
static void
_prepare_tx(ConnectionContext_t * ctx, MSG_Data_t * data){
    if(ctx && data){
        if(data->context & MSG_DATA_CTX_META_DATA){
            memcpy(&ctx->header, data->buf, 8);
            ctx->payload = NULL;
            ctx->idx = 0;
            ctx->count = 1;
            MSG_Base_ReleaseDataAtomic(data);
        }else{
            ctx->payload = data;
            ctx->header.size = data->len;
            ctx->header.checksum = _calc_checksum(data);
            ctx->header.page = 0;
            ctx->header.page_count = data->len/6 + (((data->len)%6)?1:0);
            ctx->count = ANT_DEFAULT_TRANSMIT_LIMIT;
            ctx->idx = -ANT_DEFAULT_HEADER_TRANSMIT_LIMIT;
        }
    }
}
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
static ANT_Session_t * _get_session_by_id(const ANT_ChannelID_t * id){
    int i;
    for(i = 0; i < ANT_SESSION_NUM; i++){
        //if(self.sessions[i].rx_ctx.
        if(self.sessions[i].id.device_number == id->device_number){
            return &self.sessions[i];
        }

    }
    return NULL;

}

static ANT_Session_t *
_find_unassigned_session(uint8_t * out_channel){
    ANT_Session_t * ret = NULL;
    uint8_t i;
    for(i = 0; i < ANT_SESSION_NUM; i++){
        if(self.sessions[i].id.device_number == 0){
            ret = &self.sessions[i];
            if(out_channel){
                *out_channel = i;
            }
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
static void DECREF
_free_context(ConnectionContext_t * ctx){
    MSG_Base_ReleaseDataAtomic(ctx->payload);
    ctx->payload = NULL;
}
static uint16_t
_calc_checksum(MSG_Data_t * data){
    uint16_t ret;
    ret = crc16_compute(data->buf, data->len, NULL);
    return ret;
}
static MSG_Data_t *
_allocate_payload_rx(ANT_HeaderPacket_t * buf){
    MSG_Data_t * ret;
    ret = MSG_Base_AllocateDataAtomic( 6 * buf->page_count );
    if(ret){
        //readjusting length of the data, previously
        //allocated in multiples of page_size to make assembly easier
        uint16_t ss = ((uint8_t*)buf)[5] << 8;
        ss += ((uint8_t*)buf)[4];
        ret->len = ss;
    }
    return ret;
}
static MSG_Status
_connect(uint8_t channel){
    //needs at least assigned
    MSG_Status ret = FAIL;
    uint32_t status;
    uint8_t inprogress;
    uint8_t pending;
    sd_ant_channel_status_get(channel, &inprogress);
    status = sd_ant_channel_open(channel);
    if( !status ){
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
        ret += sd_ant_channel_id_set(channel, id->device_number, id->device_type, id->transmit_type);
        ret += sd_ant_channel_low_priority_rx_search_timeout_set(channel, 0xFF);
        ret += sd_ant_channel_rx_search_timeout_set(channel, 0);
    }
    return ret?FAIL:SUCCESS;
}

static MSG_Status
_set_discovery_mode(ANT_DISCOVERY_ROLE role){
    ANT_ChannelID_t id = {0};
    ANT_ChannelPHY_t phy = {
        .period = 273,
        .frequency = 66,
        .channel_type = CHANNEL_TYPE_SLAVE,
        .network = 0};
    self.discovery_role = role;
    switch(role){
        case ANT_DISCOVERY_CENTRAL:
            //central mode
            PRINTS("CENTRAL\r\n");
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
            id.device_type = HLO_ANT_DEVICE_TYPE_PILL_EVT;
            id.transmit_type = 0;
            phy.frequency = 66;
            phy.period = 1092;
            APP_OK(sd_ant_lib_config_set(ANT_LIB_CONFIG_MESG_OUT_INC_DEVICE_ID | ANT_LIB_CONFIG_MESG_OUT_INC_RSSI | ANT_LIB_CONFIG_MESG_OUT_INC_TIME_STAMP));
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
}
static void
_assemble_payload(ConnectionContext_t * ctx, ANT_PayloadPacket_t * packet){
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
}
static uint8_t
_integrity_check(ConnectionContext_t * ctx){
    if(ctx->count >= ctx->header.page_count && ctx->payload){
        if(_calc_checksum(ctx->payload) == ctx->header.checksum){
            return 1;
        }
    }
    return 0;
}
/**
 * Assembles the pyload portion
 */
static MSG_Data_t *
_assemble_rx(ConnectionContext_t * ctx, uint8_t * buf, uint32_t buf_size){
    MSG_Data_t * ret = NULL;
    if(buf[1] > 0){
        if(buf[0] == 0){
            //standard header
            ANT_HeaderPacket_t * new_header = (ANT_HeaderPacket_t *)buf;
            uint16_t new_crc = (uint16_t)(buf[7] << 8) + buf[6];
            PRINTS("CRC: ");
            PRINT_HEX(&new_crc, 2);
            PRINTS("\r\n");
            if(_integrity_check(ctx)){
                ret = ctx->payload;
                MSG_Base_AcquireDataAtomic(ret);
                _free_context(ctx);
            }
            if(ctx->header.checksum != new_crc){
                _free_context(ctx);
                ctx->header = *new_header;
                ctx->payload = _allocate_payload_rx(&ctx->header);
                ctx->count = 0;
            }else{
                PRINTS("Same msg");
            }
        }else if(buf[0] <= buf[1]){
            //payload
            if(ctx->payload){
                _assemble_payload(ctx, (ANT_PayloadPacket_t *)buf);
                ctx->count++;
            }
        }
    }else{
        ret = _handle_ant_single_packet(ctx, buf[0], buf+2);
    }
    return ret;
}
static uint8_t DECREF
_assemble_tx(ConnectionContext_t * ctx, uint8_t * out_buf, uint32_t buf_size){
    ANT_HeaderPacket_t * header = &ctx->header;
    if(!ctx->payload || ctx->count <= 0){
        memcpy(out_buf, &ctx->header, 8);
        return 0;
    }else{
        if(ctx->idx <= 0){
            memcpy(out_buf, header, sizeof(*header));
        }else if(ctx->idx <= header->page_count){
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
_handle_tx(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    uint8_t message[ANT_STANDARD_DATA_PAYLOAD_SIZE];
    ANT_Session_t * session = NULL;
    if(self.discovery_role == ANT_DISCOVERY_CENTRAL){
        //central, only channels 1-7 are tx
        //channel 1-7 maps to index 0-6
        uint8_t idx = *channel - 1;
        if(idx < ANT_SESSION_NUM){
            session = &self.sessions[idx];
        }
    }else{
        //periperal, all channels are tx
        //channels 0-7 mapes to index 0-7
        if(*channel < ANT_SESSION_NUM){
            session = &self.sessions[*channel];
        }
    }
    if(session){
        uint32_t ret = _assemble_tx(&session->tx_ctx, message, ANT_STANDARD_DATA_PAYLOAD_SIZE);
        if(ret == 0){
            MSG_Data_t * next = MSG_Base_DequeueAtomic(session->tx_queue);
            _free_context(&session->tx_ctx);
            if(next){
                _prepare_tx(&session->tx_ctx, next);
            }else if(self.discovery_role == ANT_DISCOVERY_PERIPHERAL){
                _destroy_channel(*channel);
            }
            self.sent++;
        }
        sd_ant_broadcast_message_tx(*channel,sizeof(message), message);
    }
}
static void
_handle_rx(uint8_t * channel, uint8_t * buf, uint8_t buf_size){
    ANT_MESSAGE * msg = (ANT_MESSAGE*)buf;
    uint8_t * rx_payload = msg->ANT_MESSAGE_aucPayload;
    ANT_Session_t * session = NULL;
    ANT_ChannelID_t id = {0};
    if(self.discovery_role == ANT_DISCOVERY_CENTRAL){
        EXT_MESG_BF ext = msg->ANT_MESSAGE_sExtMesgBF;
        uint8_t * extbytes = msg->ANT_MESSAGE_aucExtData;
        if(ext.ucExtMesgBF & MSG_EXT_ID_MASK){
            id = (ANT_ChannelID_t){
                .device_number = *((uint16_t*)extbytes),
                .device_type = extbytes[4],
                .transmit_type = extbytes[3],
            };
        }else{
            //error
            PRINTS("RX Error\r\n");
            return;
        }
    }else{
        if(sd_ant_channel_id_get(*channel, &id.device_number, &id.device_type, &id.transmit_type)){
            //error
            PRINTS("RX Error\r\n");
            return;
        }
    }
    session = _get_session_by_id(&id);
    //handle
    if(session){
        ConnectionContext_t * ctx = &session->rx_ctx;
        MSG_Data_t * ret = _assemble_rx(ctx, rx_payload, buf_size);
        if(ret){
            PRINTS("MSG FROM ID = ");
            PRINT_HEX(&id.device_number, 2);
            PRINTS("\r\n");
            {
                MSG_Address_t src = (MSG_Address_t){ANT, channel+1};
                //then dispatch to uart for printout
                self.parent->dispatch(src, (MSG_Address_t){UART, 1}, ret);
                if(self.discovery_role == ANT_DISCOVERY_CENTRAL){
                    self.parent->dispatch(src, (MSG_Address_t){SSPI,1}, ret);
                }
            }
            MSG_Base_ReleaseDataAtomic(ret);
            self.rcvd++;
        }
    }else{
        //null function packet is used to see what is around the area
        //if no session exists for that ID
        if(rx_payload[0] == 0 && rx_payload[1] == 0){
            _handle_unknown_device(&id);
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
            case ANT_ADVERTISE:
                {
                    uint8_t advdata[8] = {0};
                    MSG_SEND_CMD(self.parent, ANT, MSG_ANTCommand_t, ANT_SEND_RAW, advdata, 8);
                }
                break;
            case ANT_SEND_RAW:
                {
                    MSG_Data_t * d = MSG_Base_AllocateDataAtomic(8);
                    if(d){
                        memcpy(d->buf, antcmd->param.raw_data, 8);
                        d->context |= MSG_DATA_CTX_META_DATA;
                        //default to 1st session for now
                        self.parent->dispatch((MSG_Address_t){ANT, 0}, (MSG_Address_t){ANT,1}, d);
                        MSG_Base_ReleaseDataAtomic(d);
                    }
                }
                break;
            case ANT_SET_DISCOVERY_ACTION:
                {
                    self.discovery_action = antcmd->param.action;
                }
                break;
            case ANT_SET_ROLE:
                PRINTS("ANT_SET_ROLE\r\n");
                PRINT_HEX(&antcmd->param.role, 0x1);
                return _set_discovery_mode(antcmd->param.role);
            case ANT_CREATE_SESSION:
                //since scanning in central mode, rx does not map directly to channels
                //sessions become virtual channels
                {
                    uint8_t out_channel;
                    if(_get_session_by_id(&antcmd->param.session_info)){
                        PRINTS("Session Already Exists\r\n");
                        return SUCCESS;
                    }
                    ANT_Session_t * s = _find_unassigned_session(&out_channel);
                    PRINTS("Create Session\r\n");
                    switch(self.discovery_role){
                        case ANT_DISCOVERY_CENTRAL:
                            if(s){
                                s->id = antcmd->param.session_info;
                                {
                                    uint16_t ret ;
                                    ANT_ChannelPHY_t phy = {
                                        .frequency = 66,
                                        .period = 1092,
                                        .channel_type = CHANNEL_TYPE_SLAVE,
                                        .network = 0
                                    };
                                    PRINTS("Creating Session ID = ");
                                    PRINT_HEX(&s->id.device_number, 2);
                                    ret = _configure_channel(out_channel+1, &phy, &s->id,0);
                                    PRINTS("Assign Queue\r\n");
                                    MSG_Data_t * q = MSG_Base_AllocateDataAtomic(MSG_BASE_DATA_BUFFER_SIZE);
                                    if(q){
                                        s->tx_queue = MSG_Base_InitQueue(q->buf, q->len);
                                        PRINTS("Queue Init OK");
                                    }else{
                                        PRINTS("Can't load data");
                                    }
                                }
                            }else{
                                PRINTS("Out of sessions");
                            }
                            break;
                        case ANT_DISCOVERY_PERIPHERAL:
                            if(s){
                                PRINTS("Assign ID\r\n");
                                s->id = antcmd->param.session_info;
                                PRINTS("Assign Queue\r\n");
                                MSG_Data_t * q = MSG_Base_AllocateDataAtomic(MSG_BASE_DATA_BUFFER_SIZE);
                                if(q){
                                    s->tx_queue = MSG_Base_InitQueue(q->buf, q->len);
                                    PRINTS("Queue Init OK");
                                }else{
                                    PRINTS("Can't load data");
                                }
                            }
                            break;
                        default:
                            PRINTS("Need to define a role first!\r\n");
                            break;
                    }
                }
                break;
        }
    }else{
        uint8_t channel = dst.submodule - 1;
        if(channel < ANT_SESSION_NUM){
            ANT_Session_t * session = &self.sessions[channel];
            if(SUCCESS == MSG_Base_QueueAtomic(session->tx_queue, data)){
                MSG_Base_AcquireDataAtomic(data);
                PRINTS("Sending...\r\n");
                if(self.discovery_role == ANT_DISCOVERY_PERIPHERAL){
                    _connect(channel);
                }else if(self.discovery_role == ANT_DISCOVERY_CENTRAL){
                    if(!session->tx_ctx.payload){
                        //temporary dirty fix to kick of central tx
                        //due to the way rx scan mode works, you can not just connect
                        uint8_t message[ANT_STANDARD_DATA_PAYLOAD_SIZE] = {0};
                        sd_ant_broadcast_message_tx(channel+1, sizeof(message), message);
                    }
                }
            }else{
                PRINTS("Queue Full\r\n");
            }
        }else{
            PRINTS("CHANNEL ERROR");
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
            break;
        case EVENT_RX_SEARCH_TIMEOUT:
            PRINTS("RXTO\r\n");
            break;
        case EVENT_RX_FAIL_GO_TO_SEARCH:
            PRINTS("RXFTS\r\n");
            break;
        case EVENT_TRANSFER_RX_FAILED:
            PRINTS("RFFAIL\r\n");
            break;
        case EVENT_TX:
            PRINTS("T");
            _handle_tx(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        case EVENT_TRANSFER_TX_FAILED:
            break;
        case EVENT_CHANNEL_COLLISION:
            PRINTS("XX\r\n");
            break;
        case EVENT_CHANNEL_CLOSED:
            //_handle_channel_closure(&ant_channel, event_message_buffer, ANT_EVENT_MSG_BUFFER_MIN_SIZE);
            break;
        default:
            {
                PRINTS("UE:");
                PRINT_HEX(&event,1);
                PRINTS("\\");
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
#endif
