#ifndef PROTO_TOOLS_H
#define PROTO_TOOLS_H
//helper file for converting protobuf to MSG_Data_t (and vice versa) for transmission between modules
#include "util.h"
#include "message_base.h"
#include "morpheus_ble.pb.h"

MSG_Data_t * INCREF message_from_MorpheusCommand(MorpheusCommand * command);


bool MorpheusCommand_from_message(MorpheusCommand * command, MSG_Data_t * message);

/* only frees internal malloc'd memory, command pointer must be freed by caller if necessary */
void free_MorpheusCommand(MorpheusCommand * command);


#endif
