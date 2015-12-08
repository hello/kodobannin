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


#define IMU_USE_PIN_INT1

enum {
	IMU_COLLECTION_INTERVAL = 6553, // in timer ticks, so 200ms (0.2*32768)
};

static SPI_Context _spi_context;

static bool imu_intr_ovrn = false;

static bool imu_is_above_thr(uint16_t* values);

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

	/*
	PRINTS("IMU: ");
	for(uint8_t i=0;i<3;i++){
		uint8_t temp = ((values[i] & 0xFF00) >> 8);
		PRINT_BYTE(&temp,sizeof(uint8_t));
		PRINT_BYTE((uint8_t*)&values[i],sizeof(uint8_t));
		PRINTS(" ");


	}
	PRINTS("\r\n");
*/
	return 6;
}

// Read all bytes from FIFO
uint16_t imu_fifo_read_all(uint16_t* values, uint8_t bytes_to_read)
{
	uint16_t bytes_read = 0;

	uint8_t buf[1] = { SPI_Read(REG_ACC_X_LO)};

	// Enable multiple byte read
	buf[0] |= 0x40;

	bytes_read = spi_xfer(&_spi_context, 1, buf, bytes_to_read, (uint8_t*) values);
	BOOL_OK(bytes_read == bytes_to_read);

	return bytes_read;

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



inline uint8_t imu_read_fifo_src_reg()
{
	uint8_t int_src = 0;

	_register_read(REG_FIFO_SRC, &int_src);

	//DEBUG(" fifo: ",int_src);

	return int_src;
}

inline uint8_t imu_clear_interrupt_status()
{

	// clear the interrupt by reading INT_SRC register
	uint8_t int_source;
	_register_read(REG_INT1_SRC, &int_source);
	//DEBUG("INT: ",int_source);

	return int_source;
}


uint8_t imu_handle_fifo_read(uint16_t* values)
{

	uint8_t ret = 0;
	uint8_t fifo_src_reg;
	uint8_t bytes_to_read;


	fifo_src_reg = imu_read_fifo_src_reg();

	if((imu_intr_ovrn == true) ||
			(fifo_src_reg & 0x80))
	{

		// FIFO sample size
		bytes_to_read = fifo_src_reg & FIFO_FSS_MASK;

		// Read FIFO
		ret = imu_fifo_read_all(values,bytes_to_read);

		// Reset FIFO - change mode to bypass
		imu_set_fifo_mode(IMU_FIFO_BYPASS_MODE);

		// Enable stream to FIFO mode
		imu_set_fifo_mode(IMU_FIFO_STREAM_TO_FIFO_MODE);

		_register_write(REG_CTRL_3, INT1_AOI1);


		imu_intr_ovrn = false;
	}
	else if(imu_intr_ovrn == false)
	{
		// AOI intr occurred but FIFO not full, wait for WTM INT
		_register_write(REG_CTRL_3, INT1_FIFO_WATERMARK);//INT1_FIFO_OVERRUN);
		imu_intr_ovrn = true;
	}



	return ret;

}

// Check if any of the IMU values are above threshold
static bool imu_is_above_thr(uint16_t* values)
{
	uint8_t index;
	bool return_value = false;

	for(index=0;index<3;index++)
	{
		// Compare with threshold - 55mg or 80mg? //TODO
		if( (abs( (int16_t)values[index] ) >> 4) >= 80)
		{

			return_value =  true;

			goto exit;
		}

	}

	exit:
	return return_value;

}

inline uint8_t imu_clear_interrupt2_status()
{
	// clear the interrupt by reading INT_SRC register
	uint8_t int_source;

	_register_read(REG_INT2_SRC, &int_source);

	return int_source;
}

inline void imu_enable_hres()
{
	uint8_t reg;

	_register_read(REG_CTRL_4, &reg);
	_register_write(REG_CTRL_4, reg | HIGHRES );

}

inline void imu_reset_hp_filter()
{
	uint8_t reg;

	//reset HP filter
	// A reading at this address forces the high-pass filter to recover instantaneously the dc level of the
	// acceleration signal provided to its inputs
	// this sets the current/reference acceleration/tilt state against which the device performs the threshold comparison.
	_register_read(REG_REFERENCE,&reg);
}

inline void imu_disable_hres()
{
	uint8_t reg;

	_register_read(REG_CTRL_4, &reg);
	reg &= ~HIGHRES;
	_register_write(REG_CTRL_4, reg);
}

inline void imu_lp_enable()
{
	uint8_t reg;

	_register_read(REG_CTRL_1, &reg);
	_register_write(REG_CTRL_1, reg | LOW_POWER_MODE);

}

inline void imu_lp_disable()
{
	uint8_t reg;

	_register_read(REG_CTRL_1, &reg);
	reg &= ~LOW_POWER_MODE;
	_register_write(REG_CTRL_1, reg);

}

void imu_enter_normal_mode()
{
	// IMU reset enables low power mode, LP has to be disabled before HRES is enabled
	imu_lp_disable();

	imu_enable_hres();

	imu_reset_hp_filter();

}



void imu_enter_low_power_mode()
{

	imu_disable_hres();

	imu_lp_enable();

	imu_reset_hp_filter();

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

inline void imu_fifo_enable()
{
	uint8_t reg;

	_register_read(REG_CTRL_5, &reg);
	reg |= FIFO_EN;
	_register_write(REG_CTRL_5, reg);

}

inline void imu_set_fifo_mode(enum imu_fifo_mode fifo_mode)
{
	uint8_t reg;

	_register_read(REG_FIFO_CTRL, &reg);


	// Clear the FIFO mode
	reg &= ~FIFO_MODE_SELECTION;
	reg |= (fifo_mode << ACCEL_FIFO_MODE_OFFSET);


	_register_write(REG_FIFO_CTRL, reg | 0x19 );// TODO: Is watermark threshold needed

}

inline void imu_fifo_disable()
{
	uint8_t reg;

	_register_read(REG_CTRL_5, &reg);
	reg &= ~FIFO_EN;
	_register_write(REG_CTRL_5, reg);

}

typedef struct __attribute__((packed)){

	uint16_t x;
	uint16_t y;
	uint16_t z;
}imu_data_t;
typedef union fifo_buffer {

	uint8_t bytes[IMU_FIFO_CAPACITY_BYTES];
	int16_t values[IMU_FIFO_CAPACITY_VALUES];
	imu_data_t imu_data[IMU_FIFO_CAPACITY_SAMPLES];

}fifo_buffer_t;


int32_t imu_init_low_power(enum SPI_Channel channel, enum SPI_Mode mode, 
		uint8_t miso, uint8_t mosi, uint8_t sclk,
		uint8_t nCS,
		enum imu_hz sampling_rate,
		enum imu_accel_range acc_range, uint16_t wom_threshold)
{
	int32_t err;
	uint8_t reg;

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
	_register_write(REG_CTRL_2, HIGHPASS_AOI_INT1);

	// Enable 4-wire SPI mode, Enable Block data update (output registers not updated until MSB and LSB have been read)
	_register_write(REG_CTRL_4, BLOCKDATA_UPDATE);//80

	// Set full scale range (Ctrl 4)
	imu_set_accel_range(acc_range);

	// Interrupt request latched
	_register_write(REG_CTRL_5, (LATCH_INTERRUPT1));


	// reset HP filter
	//imu_reset_hp_filter();

	// Enable OR combination interrupt generation for all axis
	reg = INT1_Z_HIGH | INT1_Z_LOW | INT1_Y_HIGH | INT1_Y_LOW | INT1_X_HIGH | INT1_X_LOW;
	_register_write(REG_INT1_CFG, reg | INT1_6D); //all axis

	imu_wom_set_threshold(wom_threshold);

	// Clear FIFO control register to select Bypass mode, set trigger to be INT1 and FIFO threshold as 0
	_register_write(REG_FIFO_CTRL,0x00);

#ifdef IMU_FIFO_ENABLE

	// FIFO enable
	imu_fifo_enable();

	// Update FIFO mode
	imu_set_fifo_mode(IMU_FIFO_STREAM_TO_FIFO_MODE);

	imu_intr_ovrn = false;
#else
	imu_fifo_disable();

#endif

	// TODO
	//_register_write(REG_ACT_THR,wom_threshold);

	int16_t values[3];
	imu_accel_reg_read(values);
	(void) values;

	imu_clear_interrupt_status();

#ifdef IMU_ENABLE_LOW_POWER
	imu_enter_low_power_mode();
#else

	imu_enter_normal_mode();

#endif



#ifdef IMU_USE_PIN_INT1

	#ifdef IMU_FIFO_ENABLE
		// interrupts are not enabled in INT 1 pin
		_register_write(REG_CTRL_3, INT1_AOI1) ;//INT1_FIFO_OVERRUN);
	#else
		// interrupts are not enabled in INT 1 pin
		_register_write(REG_CTRL_3, INT1_AOI1);
	#endif

	// Enable INT 1 function on INT 2 pin
	_register_write(REG_CTRL_6, 0x00);

#else

	// interrupts are not enabled in INT 1 pin
	_register_write(REG_CTRL_3, 0x00);

	#ifdef IMU_FIFO_ENABLE
		// Enable INT 1 function on INT 2 pin
		_register_write(REG_CTRL_6, INT1_OUTPUT_ON_LINE_2);
	#else
		// Enable INT 1 function on INT 2 pin
		_register_write(REG_CTRL_6, INT1_OUTPUT_ON_LINE_2);
	#endif

#endif


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
