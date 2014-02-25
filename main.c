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
#include <drivers/spi.h>
#include <drivers/spi_nor.h>
#include <util.h>
#include <imu.h>
#include <drivers/pwm.h>
#include <hrs.h>
#include <drivers/watchdog.h>
#include <drivers/hlo_fs.h>

#include "hlo_ble_demo.h"
#include "git_description.h"
#include "hello_dfu.h"
#include "sensor_data.h"
#include "util.h"

static uint16_t test_size;
#define APP_GPIOTE_MAX_USERS            2

static app_timer_id_t _imu_timer;

static app_gpiote_user_id_t _imu_gpiote_user;

#define IMU_COLLECTION_INTERVAL 6553 // in timer ticks, so 200ms (0.2*32768)

void
test_3v3() {
	PRINTS("BLINKING LEDS AND VIBE\r\n");
    nrf_gpio_cfg_output(GPIO_3v3_Enable);
    nrf_gpio_pin_set(GPIO_3v3_Enable);

    nrf_gpio_cfg_output(GPIO_HRS_PWM_G1);
    nrf_gpio_cfg_output(GPIO_HRS_PWM_G2);
    nrf_gpio_cfg_output(GPIO_VIBE_PWM);

    while (1){
        nrf_gpio_pin_clear(GPIO_HRS_PWM_G2);
        nrf_gpio_pin_set  (GPIO_HRS_PWM_G1);
        nrf_gpio_pin_set  (GPIO_VIBE_PWM);
		watchdog_pet();
        nrf_delay_ms(100);
        nrf_gpio_pin_clear(GPIO_VIBE_PWM);
		watchdog_pet();
        nrf_delay_ms(390);
        nrf_gpio_pin_clear(GPIO_HRS_PWM_G1);
        nrf_gpio_pin_set  (GPIO_HRS_PWM_G2);
		watchdog_pet();
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
_demo_mode_write_handler(ble_gatts_evt_write_t *event) {
    uint8_t  state = event->data[0];
    uint16_t len = 1;
    uint32_t err;

    DEBUG("_demo_mode_write_handler: 0x", state);

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
_alpha0_from_central_confirmed_write_handler(ble_gatts_evt_write_t *event)
{

}

static void
_alpha0_from_central_write_handler(ble_gatts_evt_write_t *event)
{

}

static void
_alpha0_data_response_write_handler(ble_gatts_evt_write_t *event)
{

}

static void
_hrs_send_data(const uint8_t *data, const uint16_t len) {
    uint32_t err;

    err = hlo_ble_demo_data_send_blocking(data, len);
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
_demo_cmd_write_handler(ble_gatts_evt_write_t *event) {
    PRINTS("_demo_cmd_write_handler called\r\n");
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
            err = sd_ble_gatts_value_set(hlo_ble_demo_get_handle(), 0, &len, &_state);
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
            err = sd_ble_gatts_value_set(hlo_ble_demo_get_handle(), 0, &len, &_state);
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
            err = sd_ble_gatts_value_set(hlo_ble_demo_get_handle(), 0, &len, &_state);
            APP_ERROR_CHECK(err);
            break;

        case TEST_START_IMU:
                //steal the HRS buffer
                //buf = hrs_get_buffer();
                do_imu = 1;
                /*imu_reset_fifo();
				  err = imu_fifo_read(480, buf);
                hlo_ble_demo_data_send_blocking(buf, err);
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

            err = hlo_ble_demo_data_send_blocking(hrs_get_buffer(), test_size);
            // APP_ERROR_CHECK(err);

            // update device state
            if (err == NRF_SUCCESS) {
                _state = TEST_STATE_IDLE;
                err = sd_ble_gatts_value_set(hlo_ble_demo_get_handle(), 0, &len, &_state);
                APP_ERROR_CHECK(err);
            }
            break;

        case TEST_STATE_IDLE:
            do_imu = 0;
            if (_state == TEST_START_IMU) {
                _state = TEST_STATE_IDLE;
            }
            err = sd_ble_gatts_value_set(hlo_ble_demo_get_handle(), 0, &len, &_state);
            APP_ERROR_CHECK(err);
            break;

        default:
            DEBUG("Unhandled state: ", state);
    }
}

void
services_init() {
	hlo_ble_init();

    hlo_ble_demo_init_t demo_init = {
        .conn_handler    = NULL,
        .disconn_handler = NULL,
    };

    hlo_ble_demo_init(&demo_init);

	ble_char_notify_add(BLE_UUID_DATA_CHAR);
	ble_char_write_request_add(BLE_UUID_CONF_CHAR, _demo_mode_write_handler, 4);
	ble_char_write_request_add(BLE_UUID_CMD_CHAR, _demo_cmd_write_handler, 10);
	ble_char_read_add(BLE_UUID_GIT_DESCRIPTION_CHAR,
					  (uint8_t* const)GIT_DESCRIPTION,
					  strlen(GIT_DESCRIPTION));

    hlo_ble_alpha0_init();
    ble_char_read_add(BLE_UUID_GIT_DESCRIPTION_CHAR,
                      (uint8_t* const)GIT_DESCRIPTION,
                      strlen(GIT_DESCRIPTION));
	ble_char_write_request_add(BLE_UUID_HELLO_ALPHA0_FROM_CENTRAL_CONFIRMED, _alpha0_from_central_confirmed_write_handler, 5);
    ble_char_indicate_add(BLE_UUID_HELLO_ALPHA0_FROM_BAND_CONFIRMED);
    ble_char_write_command_add(BLE_UUID_HELLO_ALPHA0_FROM_CENTRAL, _alpha0_from_central_write_handler, 5);
    ble_char_notify_add(BLE_UUID_HELLO_ALPHA0_FROM_BAND);
}

#define STRIDE 32
void
print_page(uint8_t *ptr, uint32_t len)
{
	uint32_t i = 0;
	for (i = 0; i < len; i+=STRIDE) {
		serial_print_hex(&ptr[i], STRIDE);
		PRINTS("\r\n");
	}
}

void
spinor_dump_otp() {
	uint32_t i;
	uint8_t nor_serial[32];

	spinor_enter_secure_mode();
	for (i = 0; i < 128; i++) {
		spinor_read(i*32, 32, nor_serial);
		serial_print_hex(nor_serial, 32);
		PRINTS("\r\n");
	}
	spinor_exit_secure_mode();
}

void
get_random_bytes(uint8_t *buf, uint32_t len) {
	uint32_t i;

	NRF_RNG->CONFIG = 1; //better quality random numbers
	for (i = 0; i < len; i++) {
		NRF_RNG->EVENTS_VALRDY = 0;
		NRF_RNG->TASKS_START = 1;
		while(NRF_RNG->EVENTS_VALRDY==0) {}
		buf[i] = (uint8_t)NRF_RNG->VALUE;
	}
}

bool factory_test(bool test_imu) {
	int32_t err;
	uint32_t i;
	bool passed = true;

	PRINTS("Hello Band EVT3 Factory Init Mode\r\n");
	PRINTS("=================================\r\n\r\n");
	uint8_t hwid = (NRF_FICR->CONFIGID & FICR_CONFIGID_HWID_Msk) >> FICR_CONFIGID_HWID_Pos;
	DEBUG("nRF51822 HW ID:       0x", hwid);

	PRINTS("nRF51822 Device ID:   0x");
	PRINT_HEX((uint8_t *)NRF_FICR->DEVICEID, 8);
	PRINTS("\r\n");

	PRINTS("nRF51822 Device Addr: 0x");
	PRINT_HEX((uint8_t*)NRF_FICR->DEVICEADDR, 8);
	PRINTS("\r\n");

	PRINTS("\r\n");

	PRINTS("SPI NOR Init:   ");
	err = spinor_init(SPI_Channel_0, SPI_Mode3, MISO, MOSI, SCLK, FLASH_nCS);
	if (err != 0) {
		DEBUG("FAIL :", err);
		passed = false;
		goto skip_nor;
	}
	PRINTS("PASS\r\n");

	NOR_Chip_Config *conf = spinor_get_chip_config();

	if (!conf) {
		PRINTS("FAIL: could not get SPINOR chip config\r\n");
		passed = false;
		goto skip_nor;
	}
	DEBUG("SPI NOR Vendor: 0x", conf->vendor_id);
	DEBUG("SPI NOR Chip:   0x", conf->chip_id);
	uint32_t size_mb = conf->capacity / 1024 / 1024;
	DEBUG("SPI NOR Size:   0x", size_mb);
	uint8_t nor_serial[32];
/*
	memset(nor_serial, 0xAA, 16);

	err = spinor_write(0, 16, nor_serial);

    PRINTS("SPI NOR Data Read: 0x");
    err = spinor_read(0, 20, nor_serial);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");
    APP_ERROR_CHECK(err <= 0);
    PRINT_HEX(nor_serial, 20);
    PRINTS("\r\n");
*/
	PRINTS("SPI NOR Serial: ");
	err = spinor_enter_secure_mode();
	if (err != 1) {
		DEBUG("FAIL1: ", err);
		goto init_fail;
	}
	err = spinor_read(0, 16, nor_serial);
	if (err != 16) {
		DEBUG("FAIL2: ", err);
		goto init_fail;
	}
	bool serial_set = true;
	for (i = 0; i < 16; i++) {
		if (nor_serial[i] == 0xFF) {
			serial_set = false;
			break;
		}
	}
	if (serial_set) {
		PRINT_HEX(nor_serial, 16);
		PRINTS("\r\n");
	} else {
		PRINTS("absent.\r\n");
		PRINTS("Generating SPI NOR serial: ");
		get_random_bytes(nor_serial, 16);
		PRINT_HEX(nor_serial, 16);
		PRINTS("\r\n");

		PRINTS("Writing SPI NOR serial: ");
		err = spinor_write(0, 16, nor_serial);
		if (err != 16) {
			DEBUG("FAIL3: ", err);
			goto init_fail;
		}
		memset(nor_serial, 0, 16);
		err = spinor_read(0, 16, nor_serial);
		if (err != 16) {
			DEBUG("FAIL4: ", err);
			goto init_fail;
		}
		if (err != 1) {
			DEBUG("FAIL5: ", err);
			goto init_fail;
		}
	}
	err = spinor_exit_secure_mode();

	//spinor_dump_otp();
/*
	PRINTS("SPI NOR Serial: ");
	err =  spinor_get_serial(nor_serial);
	if (err < 0) {
		PRINTS("FAIL\r\n");
		goto init_fail;
	}
	PRINT_HEX(nor_serial, 24);
	PRINTS("\r\n");
*/
	PRINTS("\r\n");

 skip_nor:
	if(test_imu) {
        PRINTS("IMU Init: ");
        err = imu_init(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
        if (err != 0) {
            DEBUG("FAIL :", err);
            goto init_fail;
        }
        PRINTS("PASS\r\n");
	}

	return passed;

init_fail:
	return false;
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

	// [TODO]: Figure out IMU profile here; need to add profile
	// support to imu.c

	struct sensor_data_header header = {
		.signature = 0x55AA,
		.checksum = 0,
		.size = fifo_left,
		.type = SENSOR_DATA_IMU_PROFILE_0,
		.timestamp = 0,
        .duration = 0,
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

static void _hard_fault() UNUSED;
static void
_hard_fault()
{
	int* p = (int*)0x5;
	*p = 0x69;
}

void
_start()
{
	uint32_t err;
	int32_t ret;
	//watchdog_init(10, 1);
    uint8_t sample[12];
	HLO_FS_Partition_Info parts[2];
	//uint32_t i;

    memset(sample, 0, sizeof(sample)/sizeof(sample[0]));

    //_state = Demo_Config_Standby;
    _state = TEST_STATE_IDLE;

    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

	if (factory_test(false)) {
		PRINTS("\r\nFactory Test PASSED.\r\n\r\n");
	} else {
		PRINTS("***************************\r\n");
		PRINTS("    FACTORY TEST FAILED\r\n");
		PRINTS("***************************\r\n");
	}
/*
	uint8_t page_data[32];

    PRINTS("SPI NOR Data Read: 0x");
    err = spinor_read(0, 32, page_data);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");
    APP_ERROR_CHECK(err <= 0);
    print_page(page_data, 32);
    PRINTS("\r\n");

	ret = hlo_fs_init();
	DEBUG("Init ret: 0x", ret);
	if (ret == HLO_FS_Not_Initialized) {
		parts[0].id = HLO_FS_Partition_Crashlog;
		parts[0].block_offset = -1;
		parts[0].block_count = 5; // 16k
		ret = hlo_fs_format(1, parts, 1);
		DEBUG("Format ret: 0x", ret);
		ret = hlo_fs_init();
		DEBUG("Init ret: 0x", ret);
	}

	PRINTS("Calling append\r\n");
	ret = hlo_fs_append(HLO_FS_Partition_Data, 0, sample);
	DEBUG("data write ret 0x", ret);
*/
	//pwm_test();
	//test_3v3();
#if 0
    uint8_t page_data[512];
	memset(page_data, 0, 512);
    memset(page_data, 0xEE, 256);
	page_data[0] = 0xFD;
	page_data[256] = 0xFB;
	page_data[511] = 0xBF;

    PRINTS("\r\n********************\r\n\r\n");
    //flash test code
    nrf_gpio_cfg_output(GPS_nCS);
    nrf_gpio_pin_set(GPS_nCS);

    err = spinor_init(SPI_Channel_0, SPI_Mode3, MISO, MOSI, SCLK, FLASH_nCS);
    APP_ERROR_CHECK(err);
    PRINTS("SPI NOR configured\r\n");

    PRINTS("SPI NOR Data Read: 0x");
    err = spinor_read(0, 20, sample);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");
    APP_ERROR_CHECK(err <= 0);
    PRINT_HEX(sample, 20);
    PRINTS("\r\n");

    PRINTS("SPI NOR Block Erase: 0x");
    err = spinor_block_erase(0);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");
    APP_ERROR_CHECK(err <= 0);

    PRINTS("SPI NOR Data Read: 0x");
    err = spinor_read(0, 20, sample);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");
    APP_ERROR_CHECK(err <= 0);
    PRINT_HEX(sample, 20);
    PRINTS("\r\n");

    PRINTS("SPI NOR Page Write: 0x");
    err = spinor_write(512, 512, page_data);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");

    spinor_wait_completion();

    PRINTS("SPI NOR Data Read: 0x");
    err = spinor_read(0, 512, page_data);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");
    APP_ERROR_CHECK(err <= 0);
    print_page(page_data, 512);
    PRINTS("\r\n");

    PRINTS("SPI NOR Data Read: 0x");
    err = spinor_read(512, 512, page_data);
    PRINT_HEX(&err, 4);
    PRINTS("\r\n");
    APP_ERROR_CHECK(err <= 0);
    print_page(page_data, 512);
    PRINTS("\r\n");

    while(1) {
        __WFE();
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

    imu_init(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);

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
/*
        if (do_imu) {
            // sample IMU and queue up to send over BLE
            //imu_accel_reg_read(sample);
            //imu_read_regs(sample);
            //hlo_ble_demo_data_send_blocking(sample, 12);
        } else {
*/          // Switch to a low power state until an event is available for the application
            err = sd_app_event_wait();
            APP_ERROR_CHECK(err);
//        }
    }

	NVIC_SystemReset();
}
