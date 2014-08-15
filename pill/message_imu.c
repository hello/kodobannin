// vi:noet:sw=4 ts=4
#include <platform.h>

#include <app_timer.h>
#include <spi.h>
#include <simple_uart.h>
#include <util.h>

#include <app_error.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <string.h>
#include <app_gpiote.h>
#include <imu.h>

#include "message_imu.h"
#include "mpu_6500_registers.h"
#include "sensor_data.h"
#include "message_base.h"
#include "timedfifo.h"


#include "message_ant.h"
#include "antutil.h"

#include <watchdog.h>


// Magic number: (0xFFFF / 4000)
//The range of accelerometer is +/-2G, the range of representation is 16bit in IMU
// And we want 3 digit precision, so it is 0xFFFF / 4000 = 16.38375
#define KG_CONVERT_FACTOR	16

enum {
    IMU_COLLECTION_INTERVAL = 6553, // in timer ticks, so 200ms (0.2*32768)
};

static SPI_Context _spi_context;

static app_timer_id_t _wom_timer;
static app_gpiote_user_id_t _gpiote_user;
static uint32_t _start_motion_time;
static uint32_t _last_motion_time;
static char * name = "IMU";
static bool initialized = false;

static MSG_Central_t * parent;
static MSG_Base_t base;


static struct imu_settings _settings = {
	.wom_threshold = 100,
	.low_power_mode_sampling_rate = IMU_HZ_15_63,  //IMU_HZ_15_63; IMU_HZ_31_25; IMU_HZ_62_50
    .normal_mode_sampling_rate = IMU_HZ_15_63, //IMU_HZ_15_63; IMU_HZ_31_25; IMU_HZ_62_50
	.active_sensors = IMU_SENSORS_ACCEL,//|IMU_SENSORS_GYRO,
    .accel_range = IMU_ACCEL_RANGE_2G,
	.gyro_range = IMU_GYRO_RANGE_2000_DPS,
	.ticks_to_fill_fifo = 0,
	.ticks_to_fifo_watermark = 0,
	.active = true,
    .data_ready_callback = NULL,
    .wom_callback = NULL,
};


static inline void _reset_accel_range(enum imu_accel_range range)
{
	_settings.accel_range = range;
    imu_set_accel_range(range);
}

static inline void _reset_gyro_range(enum imu_gyro_range range)
{
	_settings.gyro_range = range;
	imu_set_gyro_range(range);
}

void imu_set_data_ready_callback(imu_data_ready_callback_t callback)
{
    if(callback == _settings.data_ready_callback) {
        return;
    }

    uint8_t interrupt_enable_register;
    _register_read(MPU_REG_INT_EN, &interrupt_enable_register);

    if(callback) {
        interrupt_enable_register |= INT_EN_RAW_READY;
    } else {
        interrupt_enable_register &= ~INT_EN_RAW_READY;
    }

    _register_write(MPU_REG_INT_EN, interrupt_enable_register);

    _settings.data_ready_callback = callback;
}

void imu_set_wom_callback(imu_wom_callback_t callback)
{
    _settings.wom_callback = callback;
}

inline void imu_get_settings(struct imu_settings *settings)
{
	*settings = _settings;
}


static void _reset_ticks_to_fill_fifo()
{
    uint32_t samples_to_fill_fifo;
    switch(_settings.active_sensors) {
    case IMU_SENSORS_ACCEL:
        samples_to_fill_fifo = 682; // floor(4096/6.0)
        break;
    case IMU_SENSORS_ACCEL_GYRO:
        samples_to_fill_fifo = 341; // floor(4096/12.0)
        break;
    }

    unsigned ms = imu_get_sampling_interval(_settings.normal_mode_sampling_rate);
    uint32_t ticks_per_sample = 32 /* floor(32768/1024.0) */ * ms;

    _settings.ticks_to_fill_fifo = ticks_per_sample * samples_to_fill_fifo;

    // ~0.1s in timer ticks, rounded up to nearest 8. This leaves this
    // long before the FIFO buffer overflows.
    _settings.ticks_to_fifo_watermark = _settings.ticks_to_fill_fifo - 3000;
}


inline bool imu_is_active()
{
	return _settings.active == true;
}


static void _imu_wom_process(void* context)
{
    uint32_t err;

    uint32_t current_time;
    (void) app_timer_cnt_get(&current_time);

    uint32_t ticks_since_last_motion;
    (void) app_timer_cnt_diff_compute(current_time, _last_motion_time, &ticks_since_last_motion);

    uint32_t ticks_since_motion_start;
    (void) app_timer_cnt_diff_compute(current_time, _start_motion_time, &ticks_since_motion_start);

    struct imu_settings settings;
    imu_get_settings(&settings);

    // DEBUG("Ticks since last motion: ", ticks_since_last_motion);
    // DEBUG("Ticks since motion start: ", ticks_since_motion_start);
    // DEBUG("FIFO watermark ticks: ", settings.ticks_to_fifo_watermark);

    if(ticks_since_last_motion < IMU_COLLECTION_INTERVAL
       && ticks_since_motion_start < settings.ticks_to_fifo_watermark) {
        err = app_timer_start(_wom_timer, IMU_COLLECTION_INTERVAL, NULL);
        APP_ERROR_CHECK(err);
        return;
    }

    imu_wom_disable();

    // [TODO]: Figure out IMU profile here; need to add profile
    // support to imu.c

    struct sensor_data_header header = {
        .signature = 0x55AA,
        .checksum = 0,
        .size = imu_fifo_bytes_available(),
        .type = SENSOR_DATA_IMU_PROFILE_0,
        .timestamp = 123456789,
        .duration = 50,
    };
    header.checksum = memsum(&header, sizeof(header));

    /*
    if(_settings.wom_callback) {
        _settings.wom_callback(&header);
    }
    */

    _start_motion_time = 0;

    imu_enter_low_power_mode(_settings.low_power_mode_sampling_rate, _settings.wom_threshold);

    PRINTS("Deactivating IMU.\r\n");
}

static void _dispatch_motion_data_via_ant(const int16_t* values, size_t len)
{
	MSG_Data_t * message_data = MSG_Base_AllocateDataAtomic(len);
	if(message_data){
		memcpy(message_data->buf, values, len);
		parent->dispatch((MSG_Address_t){0, 0},(MSG_Address_t){ANT, 2}, message_data);
		MSG_Base_ReleaseDataAtomic(message_data);
	}

	/* do not advertise if has at least one bond */
	if(MSG_ANT_BondCount() == 0){
		// let's save one variable in the stack.

		message_data = MSG_Base_AllocateDataAtomic(sizeof(ANT_DiscoveryProfile_t));
		if(message_data){
			SET_DISCOVERY_PROFILE(message_data);
			parent->dispatch((MSG_Address_t){0,0},(MSG_Address_t){ANT,1}, message_data);
			MSG_Base_ReleaseDataAtomic(message_data);
		}
	}else{
		uint8_t ret = MSG_ANT_BondCount();
		PRINTS("bonds = ");
		PRINT_HEX(&ret, 1);
	}
}

static void _aggregate_motion_data(const int16_t* raw_xyz, size_t len)
{
	int16_t values[3];
	memcpy(raw_xyz, values, len);

	values[0] /= KG_CONVERT_FACTOR;
	values[1] /= KG_CONVERT_FACTOR;
	values[2] /= KG_CONVERT_FACTOR;

	//int32_t aggregate = ABS(values[0]) + ABS(values[1]) + ABS(values[2]);
	int32_t aggregate = values[0] * values[0] + values[1] * values[1] + values[2] * values[2];
	if( aggregate > INT16_MAX){
		aggregate = INT16_MAX;
	}

	//TF_SetCurrent((uint16_t)values[0]);
	
	if(TF_GetCurrent() < aggregate ){
		TF_SetCurrent((tf_unit_t)aggregate);
		PRINTS("NEW MAX: ");
		PRINT_HEX(&aggregate, sizeof(aggregate));
	}
}


static void _imu_gpiote_process(uint32_t event_pins_low_to_high, uint32_t event_pins_high_to_low)
{

	uint8_t interrupt_status = imu_clear_interrupt_status();
	if(interrupt_status & INT_STS_WOM_INT)
	{
		MSG_PING(parent,IMU,IMU_READ_XYZ);

		/*
		tf_unit_t values[3];
		imu_accel_reg_read((uint8_t*)values);
		imu_clear_interrupt_status();
		//uint8_t interrupt_status = imu_clear_interrupt_status();

		int16_t* p_raw_xyz = get_raw_xzy_address();
		p_raw_xyz[0] = values[0];
		p_raw_xyz[1] = values[1];
		p_raw_xyz[2] = values[2];

		app_sched_event_put(p_raw_xyz, 6, pill_ble_stream_data);
		*/
	}

}

void imu_printf_wom_callback(struct sensor_data_header* header)
{
    DEBUG("IMU sensor data header: ", *header);

    uint16_t fifo_size = header->size;
    DEBUG("IMU FIFO size: ", fifo_size);

    imu_printf_data_ready_callback(fifo_size);
}

void imu_printf_data_ready_callback(uint16_t fifo_bytes_available)
{
    (void)fifo_bytes_available;

    uint8_t buf[6];
    imu_accel_reg_read(buf);
    PRINT_HEX(buf, sizeof(buf));
    PRINTS("\r\n");
}

void imu_printf_data_ready_callback_fifo(uint16_t fifo_bytes_available)
{
    PRINT_HEX(&fifo_bytes_available, sizeof(fifo_bytes_available));
    PRINTS(" ");

    while(fifo_bytes_available > 0) {
        // For MAX_FIFO_READ_SIZE, we want to use a number that (1)
        // will not overflow the stack (keep it under ~2k), and is (2)
        // evenly divisible by 12, which is the number of bytes per
        // sample if we're sampling from both gyroscope and the
        // accelerometer.

        enum {
            MAX_FIFO_READ_SIZE = 1920, // Evenly divisible by 48
        };

        unsigned read_size = MIN(fifo_bytes_available, MAX_FIFO_READ_SIZE);
        read_size -= fifo_bytes_available % 6;

        uint8_t imu_data[read_size];
        uint16_t bytes_read = imu_fifo_read(read_size, imu_data);

        fifo_bytes_available -= bytes_read;

#define PRINT_FIFO_DATA

#ifdef PRINT_FIFO_DATA
        unsigned i;
        for(i = 0; i < bytes_read; i += sizeof(int16_t)) {
            if(i % 6 == 0 && i != 0) {
                PRINTS("\r\n");
            }
            int16_t* p = (int16_t*)(imu_data+i);
            PRINT_HEX(p, sizeof(int16_t));
        }

        PRINTS("\r\n");
#endif
    }
}


static MSG_Status _init(void){
    return SUCCESS;
}

static MSG_Status _destroy(void){
    return SUCCESS;
}

static MSG_Status _flush(void){
    return SUCCESS;
}

static MSG_Status _send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
	if(data){
		MSG_Base_AcquireDataAtomic(data);
		MSG_IMUCommand_t * cmd = data->buf;
		switch(cmd->cmd){
			default:
			case IMU_PING:
				break;
			case IMU_READ_XYZ:
				{
					int16_t values[3];
					imu_accel_reg_read((uint8_t*)values);
					//uint8_t interrupt_status = imu_clear_interrupt_status();

					if(_settings.wom_callback){
						_settings.wom_callback(values, sizeof(values));
					}

					_aggregate_motion_data(values, sizeof(values));
#ifdef ANT_ENABLE
					_dispatch_motion_data_via_ant(values, sizeof(values));
#endif
				}


				break;
		}
		MSG_Base_ReleaseDataAtomic(data);
		PRINTS("\r\n");
	}
    return SUCCESS;
}


MSG_Base_t * MSG_IMU_Init(const MSG_Central_t * central)
{
	if(!initialized){
		parent = central;
        base.init = _init;
        base.destroy = _destroy;
        base.flush = _flush;
        base.send = _send;
        base.type = IMU;
        base.typestr = name;
		if(!imu_init_low_power(SPI_Channel_1, SPI_Mode0, IMU_SPI_MISO, IMU_SPI_MOSI, IMU_SPI_SCLK, IMU_SPI_nCS, 
			_settings.low_power_mode_sampling_rate, _settings.accel_range, _settings.wom_threshold))
		{
			nrf_gpio_cfg_input(IMU_INT, GPIO_PIN_CNF_PULL_Pullup);
		    //APP_OK(app_timer_create(&_wom_timer, APP_TIMER_MODE_SINGLE_SHOT, _imu_wom_process));
		    APP_OK(app_gpiote_user_register(&_gpiote_user, 0, 1 << IMU_INT, _imu_gpiote_process));
		    APP_OK(app_gpiote_user_enable(_gpiote_user));

		    imu_clear_interrupt_status();
			//imu_calibrate_zero();

			PRINTS("IMU: initialization done.\r\n");
			initialized = true;
		}
	}
	return &base;

}
