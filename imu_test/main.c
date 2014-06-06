// vi:noet:sw=4 ts=4

#include <app_gpiote.h>
#include <app_timer.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <nrf_sdm.h>
#include <softdevice_handler.h>

#include "app.h"
#include "imu.h"
#include "platform.h"
#include "sensor_data.h"
#include "util.h"

//

enum {
    APP_GPIOTE_MAX_USERS = 1,
};

enum {
    IMU_COLLECTION_INTERVAL = 6553, // in timer ticks, so 200ms (0.2*32768)
};

//

static app_timer_id_t _imu_timer;
static app_gpiote_user_id_t _imu_gpiote_user;

//

static uint32_t _imu_start_motion_time;
static uint32_t _imu_last_motion_time;

static void
_imu_process(void* context)
{
    uint32_t err;

    uint32_t current_time;
    (void) app_timer_cnt_get(&current_time);

    uint32_t ticks_since_last_motion;
    (void) app_timer_cnt_diff_compute(current_time, _imu_last_motion_time, &ticks_since_last_motion);

    uint32_t ticks_since_motion_start;
    (void) app_timer_cnt_diff_compute(current_time, _imu_start_motion_time, &ticks_since_motion_start);

    struct imu_settings settings;
    imu_get_settings(&settings);

    // DEBUG("Ticks since last motion: ", ticks_since_last_motion);
    // DEBUG("Ticks since motion start: ", ticks_since_motion_start);
    // DEBUG("FIFO watermark ticks: ", settings.ticks_to_fifo_watermark);

    if(ticks_since_last_motion < IMU_COLLECTION_INTERVAL
       && ticks_since_motion_start < settings.ticks_to_fifo_watermark) {
        err = app_timer_start(_imu_timer, IMU_COLLECTION_INTERVAL, NULL);
        APP_ERROR_CHECK(err);
        return;
    }

    imu_wom_disable();

    unsigned sample_size;
    switch(settings.active_sensors) {
    case IMU_SENSORS_ACCEL:
        sample_size = 6;
        break;
    case IMU_SENSORS_ACCEL_GYRO:
        sample_size = 12;
        break;
    }

    uint16_t fifo_left = imu_fifo_bytes_available();
    fifo_left -= fifo_left % sample_size;

    // [TODO]: Figure out IMU profile here; need to add profile
    // support to imu.c

    struct sensor_data_header header = {
        .signature = 0x55AA,
        .checksum = 0,
        .size = fifo_left,
        .type = SENSOR_DATA_IMU_PROFILE_0,
        .timestamp = 123456789,
        .duration = 50,
    };
    header.checksum = memsum(&header, sizeof(header));

    // Write data_header to persistent storage here
    DEBUG("IMU sensor data header: ", header);

    while(fifo_left > 0) {
        // For MAX_FIFO_READ_SIZE, we want to use a number that (1)
        // will not overflow the stack (keep it under ~2k), and is (2)
        // evenly divisible by 12, which is the number of bytes per
        // sample if we're sampling from both gyroscope and the
        // accelerometer.

#define MAX_FIFO_READ_SIZE 1920 // Evenly divisible by 48
        unsigned read_size = MIN(fifo_left, MAX_FIFO_READ_SIZE);
        read_size -= fifo_left % sample_size;

        uint8_t imu_data[read_size];
        uint16_t bytes_read = imu_fifo_read(read_size, imu_data);

        DEBUG("Bytes read from FIFO: ", bytes_read);

        fifo_left -= bytes_read;

        // Write imu_data to persistent storage here

#define PRINT_FIFO_DATA

#ifdef PRINT_FIFO_DATA
        unsigned i;
        for(i = 0; i < bytes_read; i += sizeof(int16_t)) {
            if(i % sample_size == 0 && i != 0) {
                PRINTS("\r\n");
            }
            int16_t* p = (int16_t*)(imu_data+i);
            PRINT_HEX(p, sizeof(int16_t));
        }

        PRINTS("\r\n");

#endif
    }

    _imu_start_motion_time = 0;

    imu_deactivate();

    PRINTS("Deactivating IMU.\r\n");
}

static void
_imu_gpiote_process(uint32_t event_pins_low_to_high, uint32_t event_pins_high_to_low)
{
    uint32_t err;

    if(!_imu_start_motion_time) {
        (void) app_timer_cnt_get(&_imu_start_motion_time);
    }
    (void) app_timer_cnt_get(&_imu_last_motion_time);

    PRINTS("Motion detected.\r\n");

    imu_activate();

    // The _imu_timer below may already be running, but the nRF
    // documentation for app_timer_start() specifically says "When
    // calling this method on a timer which is already running, the
    // second start operation will be ignored." So we're OK here.
    err = app_timer_start(_imu_timer, IMU_COLLECTION_INTERVAL, NULL);
    APP_ERROR_CHECK(err);

    imu_clear_interrupt_status();
}

//

void
_start()
{
    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    NRF_CLOCK->TASKS_LFCLKSTART = 1;
    while(NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {};

    int32_t err = imu_init(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err);

    APP_TIMER_INIT(APP_TIMER_PRESCALER,
                   APP_TIMER_MAX_TIMERS,
                   APP_TIMER_OP_QUEUE_SIZE,
                   false);

    err = app_timer_create(&_imu_timer, APP_TIMER_MODE_SINGLE_SHOT, _imu_process);
    APP_ERROR_CHECK(err);

    nrf_gpio_cfg_input(IMU_INT, GPIO_PIN_CNF_PULL_Pullup);

    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);

    err = app_gpiote_user_register(&_imu_gpiote_user, 0, 1 << IMU_INT, _imu_gpiote_process);
    APP_ERROR_CHECK(err);

    err = app_gpiote_user_enable(_imu_gpiote_user);
    APP_ERROR_CHECK(err);

    for(;;) {
        __WFE();

        __SEV();
        __WFE();
    }
}
