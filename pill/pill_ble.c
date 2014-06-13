// vi:noet:sw=4 ts=4

#include <stddef.h>
#include <string.h>

#include <app_timer.h>
#include <ble_gatts.h>
#include <ble_srv_common.h>

#include "imu_data.h"
#include "sensor_data.h"
#include "imu.h"
#include "rtc.h"
#include "hlo_ble.h"
#include "hlo_ble_time.h"
#include "pill_ble.h"
#include "util.h"

extern uint8_t hello_type;

static uint16_t _pill_service_handle;
static uint16_t _stream_service_handle;
static struct hlo_ble_time _current_time;

static uint8_t _big_buffer[256];

static uint8_t _imu_data[12];

static void
_send_imu_data(void* imu_data, uint16_t fifo_bytes_available)
{
    hlo_ble_notify(0xFFAA, imu_data, fifo_bytes_available, NULL);
}

static void
_imu_data_ready(uint16_t fifo_bytes_available)
{
    fifo_bytes_available = MIN(12, fifo_bytes_available);
    imu_fifo_read(fifo_bytes_available, _imu_data);
    app_sched_event_put(_imu_data, fifo_bytes_available, _send_imu_data);
}

static void
_imu_wom_callback(struct sensor_data_header* header)
{
    uint16_t fifo_size = header->size;
    DEBUG("IMU FIFO size: ", fifo_size);
}

static void
_data_send_finished()
{
}

static void
_stream_write_handler(ble_gatts_evt_write_t* event)
{
    switch(event->data[0]) {
    case 0x00:
        // stop realtime accelerometer stream
        PRINTS("Stream stop\r\n");
        imu_set_data_ready_callback(NULL);
        imu_set_wom_callback(_imu_wom_callback);
        break;
    case 0x01:
        // start realtime accelerometer stream
        PRINTS("Stream start\r\n");
        imu_set_wom_callback(NULL);
        imu_activate();
        imu_set_data_ready_callback(_imu_data_ready);
        break;
    }
}

static void
_get_time(void* event_data, uint16_t event_size)
{
    struct rtc_time_t rtc_time;
    rtc_read(&rtc_time);

    PRINTS("_get_time: RTC = ");
    PRINT_HEX(rtc_time.bytes, sizeof(rtc_time.bytes));
    PRINTS(", ");
    rtc_printf(&rtc_time);
    PRINTS("\r\n");

    rtc_time_to_ble_time(&rtc_time, &_current_time);
    PRINTS("BLE: ");
    PRINT_HEX(_current_time.bytes, sizeof(_current_time.bytes));
    PRINTS(", ");
    hlo_ble_time_printf(&_current_time);
    PRINTS("\r\n");

    hlo_ble_notify(BLE_UUID_DAY_DATE_TIME_CHAR, _current_time.bytes, sizeof(_current_time.bytes), NULL);
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
            struct rtc_time_t rtc_time;

            PRINTS("Setting time to: ");
            PRINT_HEX(&command->set_time.bytes, sizeof(struct hlo_ble_time));
            PRINTS("\r\n");

            rtc_time_from_ble_time(&command->set_time, &rtc_time);

            PRINTS("BLE: ");
            hlo_ble_time_printf(&command->set_time);
            PRINTS("\r\n");

            PRINTS("RTC: ");
            rtc_printf(&rtc_time);
            PRINTS("\r\nRTC: ");
            PRINT_HEX(rtc_time.bytes, sizeof(rtc_time.bytes));
            PRINTS("\r\n");

            rtc_write(&rtc_time);

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
        hlo_ble_char_notify_add(BLE_UUID_DAY_DATE_TIME_CHAR);
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

    imu_set_wom_callback(_imu_wom_callback);
}
