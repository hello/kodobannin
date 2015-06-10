#include "proto_tools.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "nrf_error.h" 

#define ENCODE_STRING_FIELD(f)  f.funcs.encode = (f.arg ? _encode_string_fields : NULL)
#define DECODE_STRING_FIELD(f)  f.funcs.decode = _decode_string_fields
//********************************************************************************
//static 

static bool _encode_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg){
    bool ret = false;
    if(!arg){
        return false;
    }

    uint8_t * str = (uint8_t *)*arg;

    if (pb_encode_tag_for_field(stream, field)){
        ret = pb_encode_string(stream, str, strlen(str));
    }

    return ret;
}

static void _assign_encode_functions_MorpheusCommand(MorpheusCommand* command){
    ENCODE_STRING_FIELD(command->accountId);
    ENCODE_STRING_FIELD(command->deviceId);
    ENCODE_STRING_FIELD(command->wifiName);
    ENCODE_STRING_FIELD(command->wifiSSID);
    ENCODE_STRING_FIELD(command->wifiPassword);
}

static bool _encode_MorpheusCommand(MorpheusCommand * command, char * raw, size_t * len){
    pb_ostream_t out_stream = {0};
    command->version = 0;//todo verify if this is required

    _assign_encode_functions_MorpheusCommand(command);

    if(raw)
    {
        out_stream = pb_ostream_from_buffer(raw, *len);
    }

    bool status = pb_encode(&out_stream, MorpheusCommand_fields, command);

    if(status)
    {
        *len = out_stream.bytes_written;
    }else{
        PRINTS("encode protobuf failed: ");
        PRINTS(PB_GET_ERROR(&out_stream));
        PRINTS("\r\n");
    }

    return status;
}
//********************************************************************************
//public 
MSG_Data_t * INCREF message_from_MorpheusCommand(MorpheusCommand * command){
    size_t proto_len = 0;
    if(!command || !_encode_MorpheusCommand(command, NULL, &proto_len))
    {
        PRINTS("encode failed\r\n");
        return NULL;
    }

    PRINTS("Proto len: ");
    PRINT_HEX(&proto_len, sizeof(proto_len));
    PRINTS("\r\n");

    MSG_Data_t* proto_page = MSG_Base_AllocateDataAtomic(proto_len);
    if(!proto_page){
        PRINTS("no memory");
        return NULL;
    }

    if(_encode_MorpheusCommand(command, proto_page->buf, &proto_len)
            && proto_len == proto_page->len){
        return proto_page;
    }else{
        MSG_Base_ReleaseDataAtomic(proto_page);
        return NULL;
    }
    return NULL;

}


uint32_t MorpheusCommand_from_message(MorpheusCommand * command, MSG_Data_t * message){
    return NRF_SUCCESS;
}

void free_MorpheusCommand(MorpheusCommand * command){

}
