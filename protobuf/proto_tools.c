#include "proto_tools.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "nrf_error.h" 
#include "heap.h"

#define ENCODE_STRING_FIELD(f)  f.funcs.encode = (f.arg ? _encode_string_fields : NULL)
#define DECODE_STRING_FIELD(f)  f.funcs.decode = _decode_string_field
#define FREE_ARG(f) if(f.arg){vPortFree(f.arg); f.arg = NULL;}
//********************************************************************************
//static common
static bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg){
    PRINTS("string length: ");
    PRINT_HEX(&stream->bytes_left, sizeof(stream->bytes_left));
    PRINTS("\r\n");

    if(stream->bytes_left == 0){
        PRINTS("No data for field tag");
        PRINT_HEX(&field->tag, sizeof(field->tag));
        PRINTS("\r\n");
        return true;
    }

    char * string_page = pvPortMalloc(stream->bytes_left + 1);
    if(!string_page){
        PRINTS("no memory");
        return false;
    }

    memset(string_page, 0, stream->bytes_left + 1);

    if (!pb_read(stream, string_page, stream->bytes_left)){
        PRINTS("_decode_string_field read content failed\r\n");// nrf_delay_ms(1);
        vPortFree(string_page);
        return false;
    }
	
	*arg = string_page;
    return true;
}
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
//********************************************************************************
//static MorpheusCommand
static void _assign_encode_functions_MorpheusCommand(MorpheusCommand* command){
    ENCODE_STRING_FIELD(command->accountId);
    ENCODE_STRING_FIELD(command->deviceId);
    ENCODE_STRING_FIELD(command->wifiName);
    ENCODE_STRING_FIELD(command->wifiSSID);
    ENCODE_STRING_FIELD(command->wifiPassword);
}
static void _assign_decode_functions_MorpheusCommand(MorpheusCommand* command){
    DECODE_STRING_FIELD(command->accountId);
    DECODE_STRING_FIELD(command->deviceId);
    DECODE_STRING_FIELD(command->wifiName);
    DECODE_STRING_FIELD(command->wifiSSID);
    DECODE_STRING_FIELD(command->wifiPassword);
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

    if(_encode_MorpheusCommand(command, proto_page->buf, &proto_len)){
        return proto_page;
    }else{
        MSG_Base_ReleaseDataAtomic(proto_page);
        return NULL;
    }
    return NULL;

}


bool MorpheusCommand_from_message(MorpheusCommand * command, MSG_Data_t * message){
    if(!command){
        PRINTS("Invalid buffer\r\n");
        return false;
    }

    memset(command, 0, sizeof(MorpheusCommand));
    _assign_decode_functions_MorpheusCommand(command);

    pb_istream_t stream = pb_istream_from_buffer((uint8_t*)message->buf, message->len);
    bool status = pb_decode(&stream, MorpheusCommand_fields, command);

    /*
     *morpheus_ble_remove_decode_funcs(command);
     */
    if (!status){
        PRINTS("Decoding protobuf failed, error: ");
        PRINTS(PB_GET_ERROR(&stream));
        PRINTS("\r\n");
    }

    return status;
}

void free_MorpheusCommand(MorpheusCommand * command){
    FREE_ARG(command->accountId);
    FREE_ARG(command->deviceId);
    FREE_ARG(command->wifiName);
    FREE_ARG(command->wifiSSID);
    FREE_ARG(command->wifiPassword);
}
