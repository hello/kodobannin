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
    reg &= 0xf0;
    
    reg |= ( sampling_rate << 4);
    _register_write(REG_CTRL_1, reg);

}

unsigned imu_get_sampling_interval(enum imu_hz hz)
{
    switch (hz) {
        case IMU_HZ_0:
        return 0;
        break;
        case IMU_HZ_10:
        return 10;
        case IMU_HZ_25:
        return 25;
        
        default:
        return 0;
    }
}

uint16_t imu_accel_reg_read(uint16_t *values) {
    uint8_t * buf = (uint8_t*)values;
	_register_read(REG_ACC_X_LO, buf++);
	_register_read(REG_ACC_X_HI, buf++);
	_register_read(REG_ACC_Y_LO, buf++);
	_register_read(REG_ACC_Y_HI, buf++);
	_register_read(REG_ACC_Z_LO, buf++);
	_register_read(REG_ACC_Z_HI, buf++);
    
    uint8_t reg;
    _register_read(REG_CTRL_4, &reg);
    if( reg & HIGHRES ) { //convert to match the 6500...
        values[0] *= 16;
        values[1] *= 16;
        values[2] *= 16;
    } else {
        values[0] *= 262;
        values[1] *= 262;
        values[2] *= 262;
    }
    
	return 6;
}

inline void imu_set_accel_range(enum imu_accel_range range)
{
    _register_write(REG_CTRL_4, range << ACCEL_CFG_SCALE_OFFSET);
}

inline uint8_t imu_clear_interrupt_status()
{
    // clear the interrupt by reading INT_SRC register
    uint8_t int_source;
    _register_read(REG_INT1_SRC, &int_source);

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
}

void imu_enter_low_power_mode()
{
    uint8_t reg;
    _register_read(REG_CTRL_4, &reg);
    reg &= ~HIGHRES;
    _register_write(REG_CTRL_4, reg);
    
    _register_read(REG_CTRL_1, &reg);
    _register_write(REG_CTRL_1, reg | LOW_POWER_MODE);
}


void imu_wom_set_threshold(uint16_t microgravities)
{
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

	// Reset chip
	imu_reset();

	// Check for valid Chip ID
	uint8_t whoami_value;
	_register_read(REG_WHO_AM_I, &whoami_value);

	if (whoami_value != CHIP_ID) {
		DEBUG("Invalid IMU ID found. Expected 0x33, got 0x", whoami_value);
		APP_ASSERT(0);
	}

    // Init interrupts
    imu_set_accel_range(acc_range);
    imu_wom_set_threshold(wom_threshold);
    imu_set_accel_freq(sampling_rate);
    
    _register_write(REG_CTRL_4, (BLOCKDATA_UPDATE | HIGHPASS_AOI_INT1));
 // _register_write(REG_CTRL_6, (INT_ACTIVE)); // active low
    _register_write(REG_INT1_CFG, 0x3F); //all axis
    _register_write(REG_CTRL_5, (LATCH_INTERRUPT1));
    _register_write(REG_CTRL_3, (INT1_AOI1));
    
    imu_enter_low_power_mode();

	return err;
}


<<<<<<< HEAD
int imu_self_test(void){
    
	if(true){
=======
int32_t imu_init_normal(enum SPI_Channel channel, enum SPI_Mode mode, 
			uint8_t miso, uint8_t mosi, 
			uint8_t sclk, uint8_t nCS, 
			enum imu_hz sampling_rate,
			enum imu_sensor_set active_sensors,
			enum imu_accel_range acc_range, enum imu_gyro_range gyro_range)
{
 	int32_t err;

	err = spi_init(channel, mode, miso, mosi, sclk, nCS, &_spi_context);
	if (err != 0) {
		PRINTS("Could not configure SPI bus for IMU\r\n");
		return err;
	}

	// Reset procedure as per "MPU-6500 Register Map and Descriptions Revision 2.0"
	// page 43

	// Reset chip
	imu_reset();

	_register_write(MPU_REG_PWR_MGMT_1, 0);

	// Check for valid Chip ID
	uint8_t whoami_value;
	_register_read(MPU_REG_WHO_AM_I, &whoami_value);

	if (whoami_value != CHIP_ID) {
		DEBUG("Invalid MPU-6500 ID found. Expected 0x70, got 0x", whoami_value);
		APP_ASSERT(0);
	}

	// Init interrupts
	_register_write(MPU_REG_INT_CFG, INT_CFG_ACT_LO | INT_CFG_PUSH_PULL | INT_CFG_LATCH_OUT | INT_CFG_CLR_ON_STS | INT_CFG_BYPASS_EN);

	// Config interrupts
	_register_write(MPU_REG_INT_EN, INT_EN_FIFO_OVRFLO);

	switch(active_sensors)
	{
		case IMU_SENSORS_ACCEL:
			imu_set_accel_range(acc_range);
			break;
		case IMU_SENSORS_ACCEL_GYRO:
			imu_set_accel_range(acc_range);
			imu_set_gyro_range(gyro_range);
			break;
	}

	// Init FIFO
	uint8_t fifo_size_bits;
	switch(IMU_FIFO_CAPACITY) {
	case 4096:
		fifo_size_bits = ACCEL_CFG2_FIFO_SIZE_4096;
		break;
	case 2048:
		fifo_size_bits = ACCEL_CFG2_FIFO_SIZE_2048;
		break;
	case 1024:
		fifo_size_bits = ACCEL_CFG2_FIFO_SIZE_1024;
		break;
	case 512:
		fifo_size_bits = ACCEL_CFG2_FIFO_SIZE_512;
		break;
	default:
		APP_ASSERT(0);
		break;
	}
	_register_write(MPU_REG_ACC_CFG2, fifo_size_bits);

    // Reset FIFO, disable i2c, and clear regs
    _register_write(MPU_REG_USER_CTL, USR_CTL_FIFO_EN | USR_CTL_I2C_DIS | USR_CTL_FIFO_RST | USR_CTL_SIG_RST);

    imu_enter_normal_mode(sampling_rate, active_sensors);

	return err;
}
int32_t
_stream_avg(int32_t prev, int16_t x,  int32_t n){
	if(n == 0){
		return x;
	}else{
		return (((prev * n) + (int32_t)x) / (n + 1));
	}
}
uint32_t _otp(uint8_t code){
	int i;
	uint32_t base = 33096;
	if(code == 1){
		return 1;
	}else{
		for(i = 0; i < code - 2; i++){
			base = (33096 *  base) >> 15;
		}
		return ((uint32_t)(2620 * base ) >> 15);
	}
}
static uint8_t
_pass_test(uint32_t st, uint8_t st_code){
	if(st_code != 0){
		uint32_t cmp = _otp(st_code);
		uint32_t res = st * 10000 / cmp;
		if(res > 5000 && res < 15000){
			return 1;
		}else{
			PRINTS("res ");
			PRINT_HEX(&res, 4);
			PRINTS("\r\n");
			PRINTS("code ");
			PRINT_HEX(&st_code, 1);
			PRINTS("\r\n");
			PRINTS("st ");
			PRINT_HEX(&st, 4);
			PRINTS("\r\n");
			PRINTS("cmp ");
			PRINT_HEX(&cmp, 4);
			PRINTS("\r\n");
			return 0;
		}
	}else{
		PRINTS("x");
		return 1;
	}

}
static inline int32_t
_abs(int32_t a){
	/*
	 *return ((a<0)?-a:a);
	 */
	return a;
}
int imu_self_test(void){
	int i;
	int16_t values[3] = {0};
	int32_t ax_os = 0, ay_os = 0, az_os = 0;
	int32_t ax_st_os = 0, ay_st_os = 0, az_st_os = 0;
	int32_t axst = 0, ayst = 0, azst = 0;
	uint8_t factory[3] = {0};
	imu_power_off();
	nrf_delay_ms(20);
	imu_power_on();
	nrf_delay_ms(20);
	imu_reset();
	/*
	 *imu_calibrate_zero();
	 */

	nrf_delay_ms(20);
	//gyro
	//_register_write(MPU_REG_CONFIG, CONFIG_LPF_1kHz_92bw);
	//accel
    _register_write(MPU_REG_ACC_CFG2, ACCEL_CFG2_LPF_1kHz_92bw);
	//gyro
    //_register_write(MPU_REG_GYRO_CFG, GYRO_CFG_SCALE_250_DPS);
	//accel
    _register_write(MPU_REG_ACC_CFG, ACCEL_CFG_SCALE_2G);
	nrf_delay_ms(20);
	for(i = 0; i < 200; i++){
		imu_accel_reg_read((uint8_t *)values);
		/*
		 *ax_os = _stream_avg(ax_os, (uint8_t)(values[0] & 0xFF), i);
		 *ay_os = _stream_avg(ay_os, (uint8_t)(values[1] & 0xFF), i);
		 *az_os = _stream_avg(az_os, (uint8_t)(values[2] & 0xFF), i);
		 */
		ax_os = _stream_avg(ax_os, values[0], i);
		ay_os = _stream_avg(ay_os, values[1], i);
		az_os = _stream_avg(az_os, values[2], i);
		nrf_delay_ms(1);
	}
	/*
	 *PRINTS("\r\nx: ");
	 *PRINT_HEX(&ax_os, 4);
	 *PRINTS("\r\ny: ");
	 *PRINT_HEX(&ay_os, 4);
	 *PRINTS("\r\nz: ");
	 *PRINT_HEX(&az_os, 4);
	 */

	//enable self test
    //_register_write(MPU_REG_GYRO_CFG, GYRO_CFG_X_TEST | GYRO_CFG_Y_TEST | GYRO_CFG_Z_TEST);
    _register_write(MPU_REG_ACC_CFG, ACCEL_CFG_X_TEST | ACCEL_CFG_Y_TEST | ACCEL_CFG_Z_TEST);
	nrf_delay_ms(20);
	memset(values, 0, sizeof(values));
	for(i = 0; i < 200; i++){
		imu_accel_reg_read((uint8_t *)values);
		/*
		 *ax_st_os = _stream_avg(ax_st_os, (uint8_t)(values[0] & 0xFF), i);
		 *ay_st_os = _stream_avg(ay_st_os, (uint8_t)(values[1] & 0xFF), i);
		 *az_st_os = _stream_avg(az_st_os, (uint8_t)(values[2] & 0xFF), i);
		 */
		ax_st_os = _stream_avg(ax_st_os, values[0], i);
		ay_st_os = _stream_avg(ay_st_os, values[1], i);
		az_st_os = _stream_avg(az_st_os, values[2], i);
		nrf_delay_ms(1);
	}

	/*
	 *PRINTS("\r\nst_x: ");
	 *PRINT_HEX(&ax_st_os, 4);
	 *PRINTS("\r\nst_y: ");
	 *PRINT_HEX(&ay_st_os, 4);
	 *PRINTS("\r\nst_z: ");
	 *PRINT_HEX(&az_st_os, 4);
	 */

	nrf_delay_ms(40);


	//diff
	axst = _abs(ax_st_os - ax_os);
	ayst = _abs(ay_st_os - ay_os);
	azst = _abs(az_st_os - az_os);

	/*
	 *PRINTS("\r\nast_x: ");
	 *PRINT_HEX(&axst, 4);
	 *PRINTS("\r\nast_y: ");
	 *PRINT_HEX(&ayst, 4);
	 *PRINTS("\r\nast_z: ");
	 *PRINT_HEX(&azst, 4);
	 */

	//factory
	_register_read(MPU_REG_ACC_SELF_TEST_X, &factory[0]);
	_register_read(MPU_REG_ACC_SELF_TEST_Y, &factory[1]);
	_register_read(MPU_REG_ACC_SELF_TEST_Z, &factory[2]);

	nrf_delay_ms(20);

	//calcualte OTP

	if(_pass_test(axst, factory[0]) &&
		_pass_test(ayst, factory[1]) &&
		_pass_test(azst, factory[2])){
>>>>>>> master
		PRINTS("Pass\r\n");
		return 0;
	}else{
		PRINTS("Fail\r\n");
		return -1;
	}

}
