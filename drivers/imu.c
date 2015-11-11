// vi:noet:sw=4 ts=4
#include <platform.h>

#include <spi.h>
#include <simple_uart.h>
#include <util.h>

#include <app_error.h>
#include <nrf_delay.h>
#include <nrf_gpio.h>
#include <string.h>
#include <app_gpiote.h>

#include "gpio_nor.h"
#include "imu.h"
#include "lis2dh_registers.h"
#include "sensor_data.h"

#include <watchdog.h>

#define LIS2DH_LOW_POWER_MG_PER_CNT (16)
#define LIS2DH_HRES_MG_PER_CNT 		(1)
#define MPU6500_MG_PER_LSB 			(16384UL)

#define LIS2DH_CONVERT_TO_MG(x)		((x) == 0)?(LIS2DH_LOW_POWER_MG_PER_CNT):(LIS2DH_HRES_MG_PER_CNT)
#define LIS2DH_SHIFT_POS(x)			((x) == 0)?(8):(4)
//#define IMU_USE_INT1

enum {
	IMU_COLLECTION_INTERVAL = 6553, // in timer ticks, so 200ms (0.2*32768)
};

static SPI_Context _spi_context;

static inline void _register_read(Register_t register_address, uint8_t* const out_value)
{
	uint8_t buf[2] = { SPI_Read(register_address), 0};
	int32_t ret;

	ret = spi_xfer(&_spi_context, 1, buf, 1, out_value);
	BOOL_OK(ret == 1);
}

static inline void _register_write(Register_t register_address, uint8_t value)
{
	uint8_t buf[2] = { SPI_Write(register_address), value };
	int32_t ret;

	ret = spi_xfer(&_spi_context, 2, buf, 0, NULL);
	BOOL_OK(ret == 2);
}

inline void imu_set_accel_freq(enum imu_hz sampling_rate)
{
	uint8_t reg;
	_register_read(REG_CTRL_1, &reg);

	// Clear the ODR
	reg &= ~OUTPUT_DATA_RATE;

	reg |= ( sampling_rate << 4);
	_register_write(REG_CTRL_1, reg);

}

unsigned imu_get_sampling_interval(enum imu_hz hz)
{
	switch (hz) {
	case IMU_HZ_0:
		return 0;
		break;
	case IMU_HZ_1:
		return 1;
	case IMU_HZ_10:
		return 10;
	case IMU_HZ_25:
		return 25;

	default:
		return 0;
	}
}

// Read all accelerometer values
uint16_t imu_accel_reg_read(uint16_t *values)
{
	uint8_t * buf = (uint8_t*)values;
	_register_read(REG_ACC_X_LO, buf++);
	_register_read(REG_ACC_X_HI, buf++);
	_register_read(REG_ACC_Y_LO, buf++);
	_register_read(REG_ACC_Y_HI, buf++);
	_register_read(REG_ACC_Z_LO, buf++);
	_register_read(REG_ACC_Z_HI, buf++);


	return 6;
}

void imu_accel_convert_count(uint16_t* values, uint8_t mode, uint8_t hres)
{
	uint8_t reg;
	_register_read(REG_CTRL_4, &reg);

	uint8_t multiplier = LIS2DH_CONVERT_TO_MG(reg & HIGHRES);

	uint8_t i;

	for(i=0;i<3;i++)
	{
		//shift right by 8/6/4 depending on mode and HRES
		values[i] = values[i] >> 4;//LIS2DH_SHIFT_POS(multiplier);

		// Convert to mg
		values[i] *= multiplier;

		//convert to match the 6500...
		values[i] = (values [i] * MPU6500_MG_PER_LSB)/1000;

	}


	DEBUG("The value[0] is 0x",values[0]);
	DEBUG("The value[1] is 0x",values[1]);
	DEBUG("The value[2] is 0x",values[2]);
}

inline void imu_set_accel_range(enum imu_accel_range range)
{
	uint8_t reg;

	_register_read(REG_CTRL_4, &reg);

	// Clear the FSR
	reg &= ~FS_MASK;

	_register_write(REG_CTRL_4, reg | (range << ACCEL_CFG_SCALE_OFFSET));
}

inline void imu_enable_all_axis()
{
	uint8_t reg;

	_register_read(REG_CTRL_1, &reg);
	_register_write(REG_CTRL_1, reg | (AXIS_ENABLE));
}

inline uint8_t imu_clear_interrupt_status()
{
	// clear the interrupt by reading INT_SRC register
	uint8_t int_source;
	_register_read(REG_INT1_SRC, &int_source);
	//DEBUG("The INT1_SRC is 0x",int_source);


	return int_source;
}

inline uint8_t imu_clear_interrupt2_status()
{
	// clear the interrupt by reading INT_SRC register
	uint8_t int_source;

	_register_read(REG_INT2_SRC, &int_source);

	return int_source;
}

void imu_enter_normal_mode()
{
	uint8_t reg;

	_register_read(REG_CTRL_1, &reg);
	reg &= ~LOW_POWER_MODE;
	_register_write(REG_CTRL_1, reg);


	_register_read(REG_CTRL_4, &reg);
	_register_write(REG_CTRL_4, reg | HIGHRES );

	// reset HP filter
	_register_read(REG_REFERENCE,&reg);


}

void imu_enter_low_power_mode()
{
	uint8_t reg;


	_register_read(REG_CTRL_4, &reg);
	reg &= ~HIGHRES;
	_register_write(REG_CTRL_4, reg);

	_register_read(REG_CTRL_1, &reg);
	_register_write(REG_CTRL_1, reg | LOW_POWER_MODE);


	// reset HP filter
	_register_read(REG_REFERENCE,&reg);
}


void imu_wom_set_threshold(uint16_t microgravities)
{

	// 16 is FSR is 2g
	_register_write(REG_INT1_THR, microgravities / 16);

}


inline void imu_reset()
{
	PRINTS("IMU reset\r\n");
	_register_write(REG_CTRL_5, BOOT);
	nrf_delay_ms(100);
}

void imu_spi_enable()
{
	spi_enable(&_spi_context);
}

void imu_spi_disable()
{
	spi_disable(&_spi_context);
}

inline void imu_power_on()
{
#ifdef PLATFORM_HAS_IMU_VDD_CONTROL
	gpio_cfg_s0s1_output_connect(IMU_VDD_EN, 0);
#endif
}

inline void imu_power_off()
{
#ifdef PLATFORM_HAS_IMU_VDD_CONTROL
	gpio_cfg_s0s1_output_connect(IMU_VDD_EN, 1);
	gpio_cfg_d0s1_output_disconnect(IMU_VDD_EN);

#endif
}

int32_t imu_init_low_power(enum SPI_Channel channel, enum SPI_Mode mode, 
		uint8_t miso, uint8_t mosi, uint8_t sclk,
		uint8_t nCS,
		enum imu_hz sampling_rate,
		enum imu_accel_range acc_range, uint16_t wom_threshold)
{
	int32_t err;

	err = spi_init(channel, mode, miso, mosi, sclk, nCS, &_spi_context);
	if (err != 0) {
		PRINTS("Could not configure SPI bus for IMU\r\n");
		return err;
	}

	// Check for valid Chip ID
	uint8_t whoami_value = 0xA5;
	_register_read(REG_WHO_AM_I, &whoami_value);


	if (whoami_value != CHIP_ID) {
		DEBUG("Invalid IMU ID found. Expected 0x33, got 0x", whoami_value);
		APP_ASSERT(0);
	}


	// Reset chip (Reboot memory content - enables SPI 3 wire mode by default)
	imu_reset();

	imu_enable_all_axis();

	// Set inactive sampling rate (Ctrl Reg 1)
	imu_set_accel_freq(sampling_rate);

	// Enable high pass filter for interrupts, normal HP filter mode selected by default
	// HP filter needs to be reset while switching modes
	_register_write(REG_CTRL_2, HIGHPASS_AOI_INT1 );


	// interrupts are not enabled in INT 1 pin
	_register_write(REG_CTRL_3, 0x00);


	// Enable 4-wire SPI mode, Enable Block data update (output registers not updated until MSB and LSB have been read)
	_register_write(REG_CTRL_4, BLOCKDATA_UPDATE);//80

	// Set full scale range (Ctrl 4)
	imu_set_accel_range(acc_range);

	// Interrupt request latched
	_register_write(REG_CTRL_5, (LATCH_INTERRUPT1));


	uint8_t reg;

	// reset HP filter
	// A reading at this address forces the high-pass filter to recover instantaneously the dc level of the
	// acceleration signal provided to its inputs
	// this sets the current/reference acceleration/tilt state against which the device performs the threshold comparison.
	_register_read(REG_REFERENCE,&reg);

	// Enable OR combination interrupt generation for all axis
	reg = INT1_Z_HIGH | INT1_Z_LOW | INT1_Y_HIGH | INT1_Y_LOW | INT1_X_HIGH | INT1_X_LOW;
	_register_write(REG_INT1_CFG, reg | INT1_6D); //all axis

	imu_wom_set_threshold(wom_threshold);

	// TODO
	//_register_write(REG_ACT_THR,wom_threshold);

	int16_t values[3];
	imu_accel_reg_read(values);
	(void) values;

	imu_clear_interrupt_status();

	imu_enter_low_power_mode();


	// Enable INT 1 function on INT 2 pin
	_register_write(REG_CTRL_6, INT1_OUTPUT_ON_LINE_2);

	return err;
}

// TODO self test not implemented
int imu_self_test(void){

	if(false){

		PRINTS("Pass\r\n");
		return 0;
	}else{
		PRINTS("Fail\r\n");
		return -1;
	}

}
