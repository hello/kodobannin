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

#define IMU_WTM_THRESHOLD (0x19UL)

#define IMU_USE_PIN_INT1
//#define IMU_USE_PIN_INT2

#if defined(IMU_USE_PIN_INT1) && defined(IMU_USE_PIN_INT2)
	#error Use either pin INT1 or INT2 for IMU. Not both.
#endif

// Uncomment to display debug print statements
//#define IMU_MODULE_DEBUG

enum {
	IMU_COLLECTION_INTERVAL = 6553, // in timer ticks, so 200ms (0.2*32768)
};

static SPI_Context _spi_context;

// true if imu fifo watermark interrupt is enabled
static bool imu_wtm_intr_en = false;

static uint16_t imu_fifo_read_all(uint16_t* values, uint32_t bytes_to_read);


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

inline void imu_self_test_enable()
{
	uint8_t reg;
	_register_read(REG_CTRL_4, &reg);

	// Clear the self-test mode configuration
	reg &= ~SELFTEST_ENABLE;

	// select self-test 0
	_register_write(REG_CTRL_1, reg | SELFTEST_MODE0);
}

inline void imu_self_test_disable()
{
	uint8_t reg;
	_register_read(REG_CTRL_4, &reg);

	// Clear the self-test mode configuration
	reg &= ~SELFTEST_ENABLE;

	// select normal mode
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
	uint16_t bytes_read = 0;
	uint16_t bytes_to_read = 6;

	uint8_t buf[1] = { SPI_Read(REG_ACC_X_LO)};

	// Enable multiple byte read
	buf[0] |= 0x40;

	bytes_read = spi_xfer(&_spi_context, 1, buf, bytes_to_read, (uint8_t*) values);
	BOOL_OK(bytes_read == bytes_to_read);

#ifdef IMU_MODULE_DEBUG
	PRINTS("IMU: ");
	for(uint8_t i=0;i<3;i++){
		uint8_t temp = ((values[i] & 0xFF00) >> 8);
		PRINT_BYTE(&temp,sizeof(uint8_t));
		PRINT_BYTE((uint8_t*)&values[i],sizeof(uint8_t));
		PRINTS(" ");


	}
	PRINTS("\r\n");
#endif

	return 6;
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

	return int_source;
}

#define IMU_DATA_THR	55U

bool imu_data_within_thr(int16_t value)
{
	if(abs(value) < (uint16_t)IMU_DATA_THR)
		return true;

	return false;
}

uint8_t imu_handle_fifo_read(uint16_t* values)
{

	uint8_t ret = 0;
	uint8_t fifo_src_reg;
	uint8_t fifo_unread_samples;
	const uint8_t fifo_channels_per_sample = 3;
	const uint8_t fifo_bytes_per_channel = 2;


	fifo_src_reg = imu_read_fifo_src_reg();

	//If wtm interrupt is enabled or if wtm flag is set, read FIFO
	if((imu_wtm_intr_en == true) ||
		(fifo_src_reg & FIFO_WATERMARK))
	{

		// FIFO sample size
		fifo_unread_samples = fifo_src_reg & FIFO_FSS_MASK;

		// Reading the FIFO source register returns the number of unread samples in the FIFO.
		// Each sample consists of data from three channels (x,y,z)
		// Each data is 16 bits
		// Hence total bytes to be read : (fifo_unread_samples*fifo_channels_per_sample*fifo_bytes_per_channel)

		// Read FIFO
		ret = imu_fifo_read_all(values, (fifo_unread_samples*fifo_channels_per_sample*fifo_bytes_per_channel));

		_register_write(REG_CTRL_3, INT1_AOI1);

		imu_wtm_intr_en = false;

	}
	else if(imu_wtm_intr_en == false)
	{
		// AOI intr occurred but FIFO not full, wait for WTM INT
		_register_write(REG_CTRL_3, INT1_FIFO_WATERMARK);//INT1_FIFO_OVERRUN);
		imu_wtm_intr_en = true;

	}

	return ret;

}


inline void imu_enable_hres()
{
	uint8_t reg;

	_register_read(REG_CTRL_4, &reg);
	_register_write(REG_CTRL_4, reg | HIGHRES );

}

inline void imu_disable_hres()
{
	uint8_t reg;

	_register_read(REG_CTRL_4, &reg);
	reg &= ~HIGHRES;
	_register_write(REG_CTRL_4, reg);
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
	nrf_gpio_cfg_output(IMU_VDD_EN);
	nrf_gpio_pin_clear(IMU_VDD_EN);
#endif
}

inline void imu_power_off()
{
#ifdef PLATFORM_HAS_IMU_VDD_CONTROL
	nrf_gpio_cfg_output(IMU_VDD_EN);
	nrf_gpio_pin_set(IMU_VDD_EN);
#endif
}

inline void imu_fifo_enable()
{
	uint8_t reg;

	_register_read(REG_CTRL_5, &reg);
	reg |= FIFO_EN;
	_register_write(REG_CTRL_5, reg);

}

inline void imu_set_fifo_mode(enum imu_fifo_mode fifo_mode, uint8_t fifo_trigger, uint8_t wtm_threshold)
{
	uint8_t reg = 0;

	reg = (fifo_mode << ACCEL_FIFO_MODE_OFFSET) |
			(wtm_threshold & FIFO_WATERMARK_THRESHOLD) |
			( (fifo_trigger << FIFO_TRIGGER_SEL_POS) & FIFO_TRIGGER_SELECTION_MASK);

	// Set watermark threshold for FIFO
	_register_write(REG_FIFO_CTRL, reg); //IMU_WTM_THRESHOLD );

}


inline void imu_fifo_disable()
{
	uint8_t reg;

	_register_read(REG_CTRL_5, &reg);
	reg &= ~FIFO_EN;
	_register_write(REG_CTRL_5, reg);

}

// Read all bytes from FIFO
static uint16_t imu_fifo_read_all(uint16_t* values, uint32_t bytes_to_read)
{
	uint16_t bytes_read = 0;

	uint8_t buf[1] = { SPI_Read(REG_ACC_X_LO)};

	// Enable multiple byte read
	buf[0] |= 0x40;

	bytes_read = spi_xfer(&_spi_context, 1, buf, bytes_to_read, (uint8_t*) values);
	BOOL_OK(bytes_read == bytes_to_read);

#ifdef IMU_MODULE_DEBUG
 	for(uint8_t j=0;j<bytes_read/2;j+=3){
		//PRINTS("IMU: ");
		for(uint8_t i=0;i<3;i++){
			uint8_t temp = ((values[j+i] & 0xFF00) >> 8);
			PRINT_BYTE(&temp,sizeof(uint8_t));
			PRINT_BYTE((uint8_t*)&values[j+i],sizeof(uint8_t));
			PRINTS(" ");


		}
		PRINTS("\r\n");
	}
#endif

	return bytes_read;

}

inline uint8_t imu_read_fifo_src_reg()
{
	uint8_t int_src = 0;

	_register_read(REG_FIFO_SRC, &int_src);

	return int_src;
}

inline void imu_enable_intr()
{
#ifdef IMU_USE_PIN_INT1

	// interrupts are enabled on INT 1 pin
	_register_write(REG_CTRL_3, INT1_AOI1);


	// Disable INT 1 function on INT 2 pin
	_register_write(REG_CTRL_6, 0x00 | INT_ACTIVE_LOW );

#endif
#ifdef IMU_USE_PIN_INT2

	// interrupts are not enabled in INT 1 pin
	_register_write(REG_CTRL_3, 0x00);

	// Enable INT 1 function on INT 2 pin
	_register_write(REG_CTRL_6, INT1_OUTPUT_ON_LINE_2 | INT_ACTIVE_LOW);

#endif
}

inline void imu_disable_intr()
{

	// interrupts are disabled on INT 1 pin
	_register_write(REG_CTRL_3, 0x00);

	// Disable INT 1 function on INT 2 pin
	_register_write(REG_CTRL_6, 0x00);


}

int32_t imu_init_low_power(enum SPI_Channel channel, enum SPI_Mode mode, 
		uint8_t miso, uint8_t mosi, uint8_t sclk,
		uint8_t nCS,
		enum imu_hz sampling_rate,
		enum imu_accel_range acc_range, uint16_t wom_threshold)
{
	int32_t err;
	uint8_t reg;

	nrf_gpio_cfg_output(nCS);
    nrf_gpio_cfg_output(miso);
    nrf_gpio_cfg_output(mosi);
    nrf_gpio_cfg_output(sclk);

	imu_power_off();
    nrf_gpio_pin_clear(nCS);
    nrf_gpio_pin_clear(mosi);
    nrf_gpio_pin_clear(miso);
    nrf_gpio_pin_clear(sclk);
    nrf_delay_us(20);
	imu_power_on();
    nrf_delay_us(240);

	err = spi_init(channel, mode, miso, mosi, sclk, nCS, &_spi_context);
	if (err != 0) {
		PRINTS("Could not configure SPI bus for IMU\r\n");
		return err;
	}

	imu_power_on();

	// Check for valid Chip ID
	uint8_t whoami_value = 0xA5;
	_register_read(REG_WHO_AM_I, &whoami_value);
	_register_write(REG_CTRL_4, 0x00);

	_register_read(REG_WHO_AM_I, &whoami_value);


	if (whoami_value != DEVICE_ID) {
		DEBUG("Invalid IMU ID found. Expected 0x33, got 0x", whoami_value);
		APP_ASSERT(0);
	}

	// Reset chip (Reboot memory content)
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
	imu_set_fifo_mode(IMU_FIFO_STREAM_MODE, FIFO_TRIGGER_SEL_INT1, IMU_WTM_THRESHOLD);

	imu_wtm_intr_en = false;
#else
	imu_fifo_disable();

#endif

	int16_t values[3];
	imu_accel_reg_read(values);
	(void) values;

	imu_clear_interrupt_status();

#ifdef IMU_ENABLE_LOW_POWER
	imu_enter_low_power_mode();
#else

	imu_enter_normal_mode();

#endif

	imu_enable_intr();

	return err;
}

#define ST_CHANGE_MIN						((uint16_t)17)
#define ST_CHANGE_MAX						((uint16_t)360)
#define	SELF_TEST_SAMPLE_SIZE				(32UL)
#define SELF_TEST_PASS(x)	(((uint8_t)(x) >= ST_CHANGE_MIN) && ((uint8_t)(x) <= ST_CHANGE_MAX) )

int imu_self_test(void){


	int16_t values[SELF_TEST_SAMPLE_SIZE][3];
	int16_t values_st[SELF_TEST_SAMPLE_SIZE][3];
	int16_t values_avg[3];
	int16_t values_st_avg[3];

	PRINTS("Self test start\r\n");

    imu_power_off();
    nrf_delay_ms(20);
    imu_power_on();

    nrf_delay_ms(20);
    imu_reset();

	// Disable interrupts
	imu_disable_intr();

	// Disable low power mode
	imu_lp_disable();

	// Reset FIFO
	imu_set_fifo_mode(IMU_FIFO_BYPASS_MODE, FIFO_TRIGGER_SEL_INT1, IMU_WTM_THRESHOLD);

	// Enable FIFO mode
	imu_set_fifo_mode(IMU_FIFO_FIFO_MODE, FIFO_TRIGGER_SEL_INT1, SELF_TEST_SAMPLE_SIZE-1);

	// Poll for wtm flag
	while(!(imu_read_fifo_src_reg() & FIFO_WATERMARK));

	uint32_t bytes_read = 0;
	// Read samples
	bytes_read = imu_fifo_read_all(&values[0][0],SELF_TEST_SAMPLE_SIZE*3*2);

	// Calculate average for each axis
	for(uint8_t ch=0;ch<3;ch++)
	{
		uint8_t count = 0;
		values_avg[ch] = 0;
		for(uint8_t i=5;i<bytes_read/3/2;i++)
		{
			values_avg[ch] += values[i][ch];
			count++;
		}
		values_avg[ch] /= count;
	}

#ifdef IMU_MODULE_DEBUG
	PRINTS("AVG: ");
 	for(uint8_t i=0;i<3;i++){
 		int16_t diff = values_avg[i];
			uint8_t temp = ((diff & 0xFF00) >> 8);
			PRINT_BYTE(&temp,sizeof(uint8_t));
			PRINT_BYTE((uint8_t*)&diff,sizeof(uint8_t));
			PRINTS(" ");
	}
 	PRINTS("\r\n");
#endif

 	nrf_delay_ms(20);

	// Enable self-test mode
	imu_self_test_enable();

	nrf_delay_ms(20);

	int16_t values_st_avg_2[3] = {0};

	// TODO loop added to check if delay helps in self test, remove if not needed
	for(uint8_t j=0;j<3;j++)
	{
		// Reset FIFO
		imu_set_fifo_mode(IMU_FIFO_BYPASS_MODE, FIFO_TRIGGER_SEL_INT1, IMU_WTM_THRESHOLD);

		// Enable FIFO mode
		imu_set_fifo_mode(IMU_FIFO_FIFO_MODE, FIFO_TRIGGER_SEL_INT1, SELF_TEST_SAMPLE_SIZE-1);

		//nrf_delay_ms(20);

		// Poll for wtm flag
		while(!(imu_read_fifo_src_reg() & FIFO_WATERMARK));

		// Read samples with self-test enabled
		bytes_read = imu_fifo_read_all(&values_st[0][0],SELF_TEST_SAMPLE_SIZE*3*2);


		// calculate average for each axis
		for(uint8_t ch=0;ch<3;ch++)
		{
			uint8_t count = 0;
			values_st_avg[ch] = 0;
			for(uint8_t i=5;i<bytes_read/3/2;i++)
			{
				values_st_avg[ch] += values_st[i][ch];
				count++;
			}
			values_st_avg[ch] /= count;
		}

		values_st_avg_2[0] += values_st_avg[0];
		values_st_avg_2[1] += values_st_avg[1];
		values_st_avg_2[2] += values_st_avg[2];

	}

	values_st_avg_2[0] /= 3;
	values_st_avg_2[1] /= 3;
	values_st_avg_2[2] /= 3;

#ifdef IMU_MODULE_DEBUG
	PRINTS("ST AVG: ");
 	for(uint8_t i=0;i<3;i++){
		int16_t diff = values_st_avg_2[i];
		uint8_t temp = ((diff & 0xFF00) >> 8);
		PRINT_BYTE(&temp,sizeof(uint8_t));
		PRINT_BYTE((uint8_t*)&diff,sizeof(uint8_t));
		PRINTS(" ");
	}
 	PRINTS("\r\n");
#endif

	// Disable self-test mode
	imu_self_test_disable();

	nrf_delay_ms(20);

#ifdef IMU_MODULE_DEBUG
	PRINTS("DIFF: ");
 	for(uint8_t i=0;i<3;i++){
		uint16_t diff = 0;
		diff = abs(values_st_avg_2[i] - values_avg[i]);
		PRINT_BYTE(&diff,sizeof(uint8_t));
		PRINTS(" ");
	}
 	PRINTS("\r\n");


	PRINTS("Self test end\r\n");
#endif

	// Calculate self-test output change Output_st_enabled[LSB] - Ouput_st_disbled[LSB]

	// If ST output change is with 17 and 360 - Self-test pass, else fail
	if((SELF_TEST_PASS(abs(values_st_avg_2[0] - values_avg[0]))) &&
		(SELF_TEST_PASS(abs(values_st_avg_2[1] - values_avg[1]))) &&
		(SELF_TEST_PASS(abs(values_st_avg_2[2] - values_avg[2]))))
	{
		PRINTS("Pass\r\n");
		return 0;
	}else{
		PRINTS("Fail\r\n");
		return -1;
	}

}
