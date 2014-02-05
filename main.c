// vi:sw=4:ts=4

#include <app_error.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_gpiote.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
#include <bootloader.h>
#include <dfu_types.h>
#include <bootloader_util_arm.h>
#include <ble_flash.h>
#include <ble_stack_handler.h>
#include <simple_uart.h>
#include <string.h>
#include <spi.h>
#include <util.h>
#include <imu.h>
#include <ble_hello_demo.h>
#include <pwm.h>
#include <hrs.h>
#include <watchdog.h>

#include "git_description.h"
#include "hello_dfu.h"
#include "imu_data.h"

static uint16_t test_size;
#define APP_GPIOTE_MAX_USERS            2

static app_timer_id_t _imu_timer;

static app_gpiote_user_id_t _imu_gpiote_user;

#define IMU_COLLECTION_INTERVAL 6553 // in timer ticks, so 200ms (0.2*32768)

void
test_3v3() {
    nrf_gpio_cfg_output(GPIO_3v3_Enable);
    nrf_gpio_pin_set(GPIO_3v3_Enable);

    nrf_gpio_cfg_output(GPIO_HRS_PWM_G1);
    nrf_gpio_cfg_output(GPIO_HRS_PWM_G2);
    nrf_gpio_cfg_output(GPIO_VIBE_PWM);

    while (1){
        nrf_gpio_pin_clear(GPIO_HRS_PWM_G2);
        nrf_gpio_pin_set  (GPIO_HRS_PWM_G1);
        nrf_gpio_pin_set  (GPIO_VIBE_PWM);
        nrf_delay_ms(100);
        nrf_gpio_pin_clear(GPIO_VIBE_PWM);
        nrf_delay_ms(390);
        nrf_gpio_pin_clear(GPIO_HRS_PWM_G1);
        nrf_gpio_pin_set  (GPIO_HRS_PWM_G2);
        nrf_delay_ms(500);
    }
}

void ble_init();
void ble_advertising_start();

static uint8_t _state;
/*
    // GPS Stuff
    nrf_gpio_cfg_output(GPS_ON_OFF);
    nrf_gpio_pin_set(GPS_ON_OFF);
    nrf_delay_ms(2);
    nrf_gpio_pin_clear(GPS_ON_OFF);

    err_code = init_spi(SPI_Channel_1, SPI_Mode1, MISO, MOSI, SCLK, GPS_nCS);
    APP_ERROR_CHECK(err_code);
*/
static void
_mode_write_handler(ble_gatts_evt_write_t *event) {
    uint8_t  state = event->data[0];
    uint16_t len = 1;
    uint32_t err;

    DEBUG("_mode_write_handler: 0x", state);

    switch (state) {
        case Demo_Config_Standby:
            PRINTS("Enter into sleep mode\r\n");
            // TODO: cancel timers and quiesce hardware
            //       schedule maintenance wakeup with RTC
            break;

	case Demo_Config_Calibrating:
            PRINTS("Start HRS Calibration here\r\n");
            _state = Demo_Config_Calibrating;
            // TODO: notify HRS system here
            break;

        case Demo_Config_Enter_DFU:
            PRINTS("Rebooting into DFU\r\n");
            // set the trap bit for the bootloader and kick the system
            sd_power_gpregret_set(1);
            NVIC_SystemReset();
            break;

        case Demo_Config_ID_Band:
            PRINTS("ID'ing Band with Vibe\r\n");
            nrf_gpio_cfg_output(GPIO_3v3_Enable);
            nrf_gpio_pin_set(GPIO_3v3_Enable);
            nrf_gpio_cfg_output(GPIO_VIBE_PWM);

            for (len = 0; len < 3; len++) {
                nrf_gpio_pin_set  (GPIO_VIBE_PWM);
                nrf_delay_ms(100);
                nrf_gpio_pin_clear(GPIO_VIBE_PWM);
                nrf_delay_ms(390);
            }
            // have to make sure that eveeryone turns this back on when they need it.
            nrf_gpio_pin_clear(GPIO_3v3_Enable);

            // don't let the BLE advertised state sit in this state
            err = sd_ble_gatts_value_set(event->handle, 0, &len, &_state);
            APP_ERROR_CHECK(err);
            break;

	default:
            DEBUG("Unhandled state transition to 0x", state);
            err = sd_ble_gatts_value_set(event->handle, 0, &len, &_state);
            APP_ERROR_CHECK(err);
    };
}

static void
_hrs_send_data(const uint8_t *data, const uint16_t len) {
    uint32_t err;

    err = ble_hello_demo_data_send_blocking(data, len);
    APP_ERROR_CHECK(err);
}

static void
_data_write_handler(ble_gatts_evt_write_t *event) {
    PRINTS("_data_write_handler called\r\n");
}

#define TEST_START_HRS  0x33
#define TEST_HRS_DONE   0x34
#define TEST_CAL_HRS    0x35
#define TEST_START_HRS2 0x36
#define TEST_START_IMU  0x55
#define TEST_SEND_DATA  0x44
#define TEST_STATE_IDLE 0x66
#define TEST_ENTER_DFU  0x99

static volatile uint8_t do_imu = 0;

static void
_cmd_write_handler(ble_gatts_evt_write_t *event) {
    PRINTS("_cmd_write_handler called\r\n");
    uint8_t  state = event->data[0];
    uint16_t len = 1;
    uint32_t err;
    //uint8_t *buf;

    hrs_parameters_t hrs_parameters;
    memset(&hrs_parameters, 0, sizeof(hrs_parameters));

    switch (state) {
        case TEST_START_HRS:
            do_imu = 0;
            PRINT_HEX(event->data, 6);
            test_size = *(uint16_t *)&event->data[4];
            DEBUG("Starting HRS job: ", test_size);

            hrs_run_test( event->data[1], *(uint16_t *)&event->data[2], *(uint16_t *)&event->data[4], 0);
            _state = TEST_HRS_DONE;
            err = sd_ble_gatts_value_set(ble_hello_demo_get_handle(), 0, &len, &_state);
            APP_ERROR_CHECK(err);
            break;

    case TEST_START_HRS2:
            do_imu = 0;
            PRINT_HEX(event->data, 9);
            test_size = *(uint16_t *)&event->data[4];
            DEBUG("Starting HRS job: ", test_size);

	    hrs_parameters = (hrs_parameters_t) {
		    .power_level = event->data[1],
		    .delay = *(uint16_t *)&event->data[2],
		    .samples = *(uint16_t *)&event->data[4],
		    .discard_samples = *(uint16_t *)&event->data[6],
		    .keep_the_lights_on = (bool)event->data[8],
		    .discard_threshold = event->data[9],
	    };
	    hrs_run_test2(hrs_parameters);
            _state = TEST_HRS_DONE;
            err = sd_ble_gatts_value_set(ble_hello_demo_get_handle(), 0, &len, &_state);
            APP_ERROR_CHECK(err);
            break;

        case TEST_CAL_HRS:
            do_imu = 0;
            PRINT_HEX(event->data, 6);
            test_size = *(uint16_t *)&event->data[4];
            DEBUG("Starting HRS cal: ", test_size);

            hrs_run_test( event->data[1], *(uint16_t *)&event->data[2], *(uint16_t *)&event->data[4], 1);
			// hrs_calibrate( event->data[1], event->data[2], *(uint16_t *)&event->data[3], *(uint16_t *)&event->data[5], &_hrs_send_data);
            _state = TEST_HRS_DONE;
            err = sd_ble_gatts_value_set(ble_hello_demo_get_handle(), 0, &len, &_state);
            APP_ERROR_CHECK(err);
            break;

        case TEST_START_IMU:
                //steal the HRS buffer
                //buf = hrs_get_buffer();
                do_imu = 1;
                /*imu_reset_fifo();
				  err = imu_fifo_read(480, buf);
                ble_hello_demo_data_send_blocking(buf, err);
                */break;

	case TEST_ENTER_DFU:
            do_imu = 0;
            PRINTS("Rebooting into DFU");
            // set the trap bit for the bootloader and kick the system
            //NRF_POWER->GPREGRET |= 0x1;
            sd_power_gpregret_set(1);
            NVIC_SystemReset();
            break;

        case TEST_SEND_DATA:
            do_imu = 0;
            DEBUG("Sending HRS data: ", test_size);

            err = ble_hello_demo_data_send_blocking(hrs_get_buffer(), test_size);
            // APP_ERROR_CHECK(err);

            // update device state
            if (err == NRF_SUCCESS) {
                _state = TEST_STATE_IDLE;
                err = sd_ble_gatts_value_set(ble_hello_demo_get_handle(), 0, &len, &_state);
                APP_ERROR_CHECK(err);
            }
            break;

        case TEST_STATE_IDLE:
            do_imu = 0;
            if (_state == TEST_START_IMU) {
                _state = TEST_STATE_IDLE;
            }
            err = sd_ble_gatts_value_set(ble_hello_demo_get_handle(), 0, &len, &_state);
            APP_ERROR_CHECK(err);
            break;

        default:
            DEBUG("Unhandled state: ", state);
    }
}

void
services_init() {
    // add hello demo service
    ble_hello_demo_init_t demo_init = {
        .conn_handler    = NULL,
        .disconn_handler = NULL,
    };

    ble_hello_demo_init(&demo_init);

	ble_char_notify_add(BLE_UUID_DATA_CHAR);
	ble_char_write_add(BLE_UUID_CONF_CHAR, _mode_write_handler, 4);
	ble_char_write_add(BLE_UUID_CMD_CHAR, _cmd_write_handler, 10);
	ble_char_read_add(BLE_UUID_GIT_DESCRIPTION_CHAR,
					  (uint8_t* const)GIT_DESCRIPTION,
					  sizeof(GIT_DESCRIPTION));
}

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

    struct imu_data_header_v0 data_header = {
		.version = 0,
        .timestamp = 0,
		.sensors = settings.active_sensors,
		.accel_range = IMU_ACCEL_RANGE_2G,
		.gyro_range = IMU_GYRO_RANGE_500_DPS,
        .hz = settings.active_sample_rate,
        .bytes = fifo_left,
    };

    // Write data_header to persistent storage here
    DEBUG("IMU data header: ", data_header);

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

	    watchdog_pet();

        // Write imu_data to persistent storage here

#undef PRINT_FIFO_DATA

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

void
_start()
{
	uint32_t err;
	watchdog_init(10, 1);
    uint8_t sample[12];

    memset(sample, 0, sizeof(sample)/sizeof(sample[0]));

    //_state = Demo_Config_Standby;
    _state = TEST_STATE_IDLE;

    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

    //pwm_test();

    // IMU standalone test code
#if 0
    err = init_spi(SPI_Channel_0, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err);

    imu_init(SPI_Channel_0);

    // for do_imu code
    int16_t *values = sample;
    int16_t old_values[6];
    uint16_t diff[6];
    uint32_t read, sent;

    imu_read_regs(old_values);
    int i;
    while(1) {
        //read = imu_accel_reg_read(sample);
        read = imu_read_regs(sample);

        // let's play around with calc'ing diffs
        for (i=0; i < 6; i++) {
            diff[i] = old_values[i] - values[i];
            old_values[i] = values[i];
        }
        //read = imu_fifo_read(6, sample);
        //if (read > 0) {
		PRINT_HEX(diff, read);
		PRINTS("\r\n");
        //}
        nrf_delay_ms(5);
    }
#endif

    //adc_test();

	APP_TIMER_INIT(APP_TIMER_PRESCALER,
				   APP_TIMER_MAX_TIMERS,
				   APP_TIMER_OP_QUEUE_SIZE,
				   false);

    // init ble
	ble_init();

    // start advertising
	ble_advertising_start();

    // init imu SPI channel and interface

    err = init_spi(SPI_Channel_0, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err);

    //imu_selftest(SPI_Channel_0);

    imu_init(SPI_Channel_0);

	err = app_timer_create(&_imu_timer, APP_TIMER_MODE_SINGLE_SHOT, _imu_process);
    APP_ERROR_CHECK(err);

	nrf_gpio_cfg_input(IMU_INT, GPIO_PIN_CNF_PULL_Pullup);

	APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);

	err = app_gpiote_user_register(&_imu_gpiote_user, 0, 1 << IMU_INT, _imu_gpiote_process);
	APP_ERROR_CHECK(err);

	err = app_gpiote_user_enable(_imu_gpiote_user);
	APP_ERROR_CHECK(err);

    // loop on BLE events FOREVER
    while(1) {
    	watchdog_pet();

        if (do_imu) {
            // sample IMU and queue up to send over BLE
            //imu_accel_reg_read(sample);
            imu_read_regs(sample);
            ble_hello_demo_data_send_blocking(sample, 12);
        } else {
            // Switch to a low power state until an event is available for the application
            err = sd_app_event_wait();
            APP_ERROR_CHECK(err);
        }
    }

	NVIC_SystemReset();
}
