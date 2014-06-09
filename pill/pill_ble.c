// vi:noet:sw=4 ts=4

#include <stddef.h>
#include <string.h>

#include <ble_gatts.h>
#include <ble_srv_common.h>

#include "aigz.h"
#include "hlo_ble.h"
#include "hlo_ble_time.h"
#include "pill_ble.h"
#include "util.h"

extern uint8_t hello_type;

static uint16_t _pill_service_handle;
static uint16_t _stream_service_handle;
static struct hlo_ble_time _current_time;

static uint8_t _big_buffer[256];

static void
_data_send_finished()
{
}

static void
_stream_write_handler(ble_gatts_evt_write_t* event)
{

}

static void
_get_time(void* event_data, uint16_t event_size)
{
    PRINTS("_get_time\r\n");

    struct aigz_time_t rtc_time;
    aigz_read(&rtc_time);

     aigz_time_to_ble_time(&rtc_time, &_current_time);
     hlo_ble_notify(BLE_UUID_DATE_TIME_CHAR, _current_time.bytes, sizeof(_current_time.bytes), NULL);
 }

static void
_command_write_handler(ble_gatts_evt_write_t* event)
{
    struct pill_command* command = (struct pill_command*)event->data;

    DEBUG("_command_write_handler(): ", command->command);

    switch(command->command) {
    case PILL_COMMAND_STOP_ACCELEROMETER:
    case PILL_COMMAND_START_ACCELEROMETER:
    case PILL_COMMAND_CALIBRATE:
    case PILL_COMMAND_DISCONNECT:
        hlo_ble_notify(0xD00D, &command->command, sizeof(command->command), NULL);
        break;
    case PILL_COMMAND_SEND_DATA:
        hlo_ble_notify(0xFEED, _big_buffer, sizeof(_big_buffer), _data_send_finished);
        break;
    case PILL_COMMAND_GET_TIME:
        {
            // app_sched_event_put(NULL, 0, _get_time);
            _get_time(NULL, 0);
            break;
        }
    case PILL_COMMAND_SET_TIME:
        {
            struct aigz_time_t rtc_time;
            aigz_time_from_ble_time(&command->set_time, &rtc_time);
            aigz_write(&rtc_time);
            break;
        }
    };
}

static void
_data_ack_handler(ble_gatts_evt_write_t* event)
{
    PRINTS("_data_ack_handler()\r\n");
}

void
pill_ble_evt_handler(ble_evt_t* ble_evt)
{
    DEBUG("Pill BLE event handler: ", ble_evt->header.evt_id);
    // sd_ble_gatts_rw_authorize_reply
}

void
pill_ble_services_init()
{
    _current_time = (struct hlo_ble_time) {
        .year = 2014,
        .month = 1,
        .day = 2,
        .hours = 3,
        .minutes = 4,
        .seconds = 5,
    };

    {
        ble_uuid_t pill_service_uuid = {
            .type = hello_type,
            .uuid = BLE_UUID_PILL_SVC,
        };

        APP_OK(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &pill_service_uuid, &_pill_service_handle));

        hlo_ble_char_write_request_add(0xDEED, &_command_write_handler, sizeof(struct pill_command));
        hlo_ble_char_notify_add(0xD00D);
        hlo_ble_char_notify_add(0xFEED);
        hlo_ble_char_write_command_add(0xF00D, &_data_ack_handler, sizeof(struct pill_data_response));
        hlo_ble_char_notify_add(BLE_UUID_DATE_TIME_CHAR);
    }

    {
        ble_uuid_t stream_service_uuid = {
            .type = hello_type,
            .uuid = 0xFFA0,
        };

        APP_OK(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &stream_service_uuid, &_stream_service_handle));

        hlo_ble_char_write_command_add(0xFFA1, &_stream_write_handler, 1);
        hlo_ble_char_notify_add(0xFFAA);
    }
}
