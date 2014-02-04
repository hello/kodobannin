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
#include <ble_hello_demo.h>
#include <drivers/pwm.h>
#include <hrs.h>
#include <drivers/watchdog.h>

#include "git_description.h"
#include "hello_dfu.h"

static uint16_t test_size;
#define APP_GPIOTE_MAX_USERS            2

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

bool factory_test() {
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
	PRINTS("IMU Init: ");
	err = imu_init(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
	if (err != 0) {
		DEBUG("FAIL :", err);
		goto init_fail;
	}
	PRINTS("PASS\r\n");
	return passed;

init_fail:
	return false;
}

void
_start()
{
	uint32_t err;
	watchdog_init(10, 1);
    uint8_t sample[12];
	//uint32_t i;

    memset(sample, 0, sizeof(sample)/sizeof(sample[0]));

    //_state = Demo_Config_Standby;
    _state = TEST_STATE_IDLE;

    simple_uart_config(SERIAL_RTS_PIN, SERIAL_TX_PIN, SERIAL_CTS_PIN, SERIAL_RX_PIN, false);

	if (factory_test()) {
		PRINTS("\r\nFactory Test PASSED.\r\n\r\n");
	} else {
		PRINTS("***************************\r\n");
		PRINTS("    FACTORY TEST FAILED\r\n");
		PRINTS("***************************\r\n");
	}
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
    // IMU standalone test code
#if 0
    err = imu_init(SPI_Channel_0, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err);

    // for do_imu code
    int16_t *values = (int16_t *)sample;
    int16_t old_values[6];
    uint16_t diff[6];
    uint32_t read, sent;

    imu_read_regs((uint8_t *)old_values);

    while(1) {
        //read = imu_accel_reg_read(sample);
        read = imu_read_regs((uint8_t *)sample);

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

    // init ble
	ble_init();

    // start advertising
	ble_advertising_start();

    // init imu SPI channel and interface
#if 0
    err = imu_init(SPI_Channel_0, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS);
    APP_ERROR_CHECK(err);

    //imu_selftest(SPI_Channel_0);

	imu_set_sensors(IMU_SENSORS_ACCEL|IMU_SENSORS_GYRO);
#endif
    // loop on BLE events FOREVER
    while(1) {
    	watchdog_pet();

        if (do_imu) {
            // sample IMU and queue up to send over BLE
            //imu_accel_reg_read(sample);
            //imu_read_regs(sample);
            //ble_hello_demo_data_send_blocking(sample, 12);
        } else {
            // Switch to a low power state until an event is available for the application
            err = sd_app_event_wait();
            APP_ERROR_CHECK(err);
        }
    }

	NVIC_SystemReset();
}
