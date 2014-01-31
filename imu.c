// vi:sw=4:ts=4

#include <app_timer.h>
#include <spi.h>
#include <simple_uart.h>
#include <util.h>
#include <device_params.h>
#include <app_error.h>
#include <nrf_delay.h>
#include <string.h>

#include "imu.h"
#include "mpu_6500_registers.h"
#include "watchdog.h"

#define BUF_SIZE 4

static enum SPI_Channel _chan = SPI_Channel_Invalid;

static struct imu_settings _settings = {
	.wom_threshold = 35,
	.inactive_sample_rate = IMU_HZ_15_63,
    .active_sample_rate = IMU_HZ_31_25,
	.active_sensors = IMU_SENSORS_ACCEL|IMU_SENSORS_GYRO,
	.active = true,
};

static inline void
_register_read(MPU_Register_t register_address, uint8_t* const out_value)
{
	uint8_t buf[2];
	buf[0] = SPI_Read(register_address);

	bool success = spi_xfer(_chan, IMU_SPI_nCS, 2, buf, buf);
	APP_ASSERT(success);

	*out_value = buf[1];
}

static inline void
_register_write(MPU_Register_t register_address, uint8_t value)
{
	uint8_t buf[2] = { SPI_Write(register_address), value };

	bool success = spi_xfer(_chan, IMU_SPI_nCS, 2, buf, buf);
	APP_ASSERT(success);
}

unsigned
imu_get_sampling_interval(enum imu_hz hz)
{
    // The following line is a fast way to convert an enum imu_hz sample rate
    // to the number of milliseconds that sample rate uses between
    // samples.
	//
    // If you look at the imu_hz enum values, they range from [0..11],
    // representing sample rates of [0.25Hz, 0.49Hz.. 500Hz], doubling
    // with each step. If we table out the enum value vs the cycle
    // time in milliseconds, working backwards because that's easier,
    // we get:
	//
	// enum  sample rate (hz)  milliseconds
	// ----  ----------------  ------------
    // 11    500               2
    // 10    250               4
    // 9     125               8
    // 8     62.50             16
    // 7     31.25             32
    // 6     15.63             64
    // 5     7.81              128
	// 4     3.91              256
	// 3     1.95              512
	// 2     0.98              1024
	// 1     0.49              2048
	// 0     0.25              4096
	//
	// ... so, hey, look, that looks a lot like a predicatable
	// pattern. It turns out that each enum step doubles the
	// time. Working backwards from table and starting at the
	// bottom-right, we see an enum of 0 is 4096 milliseconds; moving
	// up to the next line of 2048 milliseconds is an enum of 1; 1024
	// milliseconds is an enum of 2; etc. So, the generic formnula is
	// 4096 >> enum. Simple.
	//
	// (We could use an array for this too, but it seems more prudent
	// to spend a couple of extra cycles instead of precious RAM
	// space.)

	return (unsigned)4096U >> (unsigned)hz;
}

uint16_t
imu_fifo_bytes_available() {
	union uint16_bits fifo_count;

	_register_read(MPU_REG_FIFO_CNT_HI, &fifo_count.bytes[1]);
	_register_read(MPU_REG_FIFO_CNT_LO, &fifo_count.bytes[0]);

	return fifo_count.value;
}

uint16_t
imu_accel_reg_read(uint8_t *buf) {
	_register_read(MPU_REG_ACC_X_LO, buf++);
	_register_read(MPU_REG_ACC_X_HI, buf++);
	_register_read(MPU_REG_ACC_Y_LO, buf++);
	_register_read(MPU_REG_ACC_Y_HI, buf++);
	_register_read(MPU_REG_ACC_Z_LO, buf++);
	_register_read(MPU_REG_ACC_Z_HI, buf++);

	return 6;
}

uint16_t
imu_read_regs(uint8_t *buf) {
	_register_read(MPU_REG_ACC_X_LO, buf++);
	_register_read(MPU_REG_ACC_X_HI, buf++);
	_register_read(MPU_REG_ACC_Y_LO, buf++);
	_register_read(MPU_REG_ACC_Y_HI, buf++);
	_register_read(MPU_REG_ACC_Z_LO, buf++);
	_register_read(MPU_REG_ACC_Z_HI, buf++);

	_register_read(MPU_REG_GYRO_X_LO, buf++);
	_register_read(MPU_REG_GYRO_X_HI, buf++);
	_register_read(MPU_REG_GYRO_Y_LO, buf++);
	_register_read(MPU_REG_GYRO_Y_HI, buf++);
	_register_read(MPU_REG_GYRO_Z_LO, buf++);
	_register_read(MPU_REG_GYRO_Z_HI, buf++);

	return 12;
}

void _reset_sensors()
{
    uint8_t fifo_register, power_management_2_register;
    _register_read(MPU_REG_FIFO_EN, &fifo_register);
	_register_read(MPU_REG_PWR_MGMT_2, &power_management_2_register);

    fifo_register &= ~(FIFO_EN_QUEUE_ACCEL|FIFO_EN_QUEUE_GYRO_X|FIFO_EN_QUEUE_GYRO_Y|FIFO_EN_QUEUE_GYRO_Z);
    power_management_2_register |= PWR_MGMT_2_ACCEL_X_DIS|PWR_MGMT_2_ACCEL_Y_DIS|PWR_MGMT_2_ACCEL_Z_DIS|PWR_MGMT_2_GYRO_X_DIS|PWR_MGMT_2_GYRO_Y_DIS|PWR_MGMT_2_GYRO_Z_DIS;


    if(_settings.active_sensors & IMU_SENSORS_ACCEL) {
        fifo_register |= FIFO_EN_QUEUE_ACCEL;
		power_management_2_register &= ~(PWR_MGMT_2_ACCEL_X_DIS|PWR_MGMT_2_ACCEL_Y_DIS|PWR_MGMT_2_ACCEL_Z_DIS);
    }

    if(_settings.active_sensors & IMU_SENSORS_GYRO) {
        fifo_register |= FIFO_EN_QUEUE_GYRO_X|FIFO_EN_QUEUE_GYRO_Y|FIFO_EN_QUEUE_GYRO_Z;
		power_management_2_register &= ~(PWR_MGMT_2_GYRO_X_DIS|PWR_MGMT_2_GYRO_Y_DIS|PWR_MGMT_2_GYRO_Z_DIS);
    }

    _register_write(MPU_REG_PWR_MGMT_2, power_management_2_register);
	_register_write(MPU_REG_USER_CTL, USR_CTL_FIFO_EN|USR_CTL_FIFO_RST);
    _register_write(MPU_REG_FIFO_EN, fifo_register);
}

static
void _clear_interrupt_status_register()
{
    // Oddly, you clear the interrupt status register by _reading_ it,
    // not writing to it. Read that again for impact.
    //
    // If you have INT_CFG_CLR_ON_STS set in the MPU_REG_INT_CFG
    // register (which we do), then you must read MPU_REG_INT_STS to
    // clear the interrupt status. If INT_CFG_CLR_ANY_READ is active,
    // then the interrupt status is cleared by reading _any_ register,
    // which is even more whacko.

    uint8_t int_status;
    _register_read(MPU_REG_INT_STS, &int_status);
}

void imu_set_sensors(enum imu_sensor_set sensors)
{
    if(_settings.active_sensors == sensors) {
        return;
    }

    _settings.active_sensors = sensors;

    _reset_sensors();

	DEBUG("IMU: sensors set to ", sensors);
}

void
imu_get_settings(struct imu_settings *settings)
{
	*settings = _settings;
}

uint16_t
imu_fifo_read(uint16_t count, uint8_t *buf) {
	uint16_t avail;

	avail = imu_fifo_bytes_available();

	if (avail < count)
		count = avail;

	if (count == 0)
		return 0;

	count = spi_read_multi(_chan, IMU_SPI_nCS, SPI_Read(MPU_REG_FIFO), count, buf);

	_clear_interrupt_status_register();

	return count;
}

void
imu_fifo_clear()
{
	// [TODO]: This will overflow the stack!

	uint8_t null_buffer[IMU_FIFO_CAPACITY];

	imu_fifo_read(IMU_FIFO_CAPACITY, null_buffer);
}

/*
int mpu_read_mem(unsigned short mem_addr, unsigned short length,
        unsigned char *data)
{
    unsigned char tmp[2];

    if (!data)
        return -1;
    if (!st.chip_cfg.sensors)
        return -1;

    tmp[0] = (unsigned char)(mem_addr >> 8);
    tmp[1] = (unsigned char)(mem_addr & 0xFF);

    // Check bank boundaries.
    if (tmp[1] + length > st.hw->bank_size)
        return -1;

    if (i2c_write(st.hw->addr, st.reg->bank_sel, 2, tmp))
        return -1;
    if (i2c_read(st.hw->addr, st.reg->mem_r_w, length, data))
        return -1;
    return 0;
}
*/

static void _continuous() __attribute__((unused));
static void
_continuous()
{
	union fifo_buffer {
		uint8_t bytes[IMU_FIFO_CAPACITY];
		uint32_t uint32s[1024];

		int16_t values[2048];
	};

	union fifo_buffer buffer;

	for(;;) {
		uint16_t bufsize = imu_fifo_read(IMU_FIFO_CAPACITY, buffer.bytes);
		DEBUG("bufsize: ", bufsize);

		unsigned i = 0;
		union generic_pointer p;
		for(p.pi16 = buffer.values; p.p8 < buffer.bytes+bufsize; p.pi16++) {
			*p.pi16 = bswap16(*p.pi16);
			PRINT_HEX(p.pi16, sizeof(*p.pi16));
			i++;
			if(i == 6) {
				PRINTS("\r\n");
				i = 0;
			}
		}

		PRINTS("\r\n");

		watchdog_pet();

		nrf_delay_ms(1000);

		watchdog_pet();
	}
}

static void _self_test() __attribute__((unused));
static void _self_test()
{
	// This code is unused and untested; it's been checked in for
	// safe-keeping. You will probably have to futz with it to get it
	// to work.

	//_register_write(MPU_REG_ACC_CFG, ACCEL_CFG_SCALE_8G);

	//_register_write(MPU_REG_FIFO_EN, SENSORS);

	// See page 11 of the MPU-6500 Register Map descriptions for this algorithm

	uint8_t self_test_accels[3];
	memset(self_test_accels, 0x55, sizeof(self_test_accels));

	_register_read(MPU_REG_ACC_SELF_TEST_X, &self_test_accels[0]);
	_register_read(MPU_REG_ACC_SELF_TEST_Y, &self_test_accels[1]);
	_register_read(MPU_REG_ACC_SELF_TEST_Z, &self_test_accels[2]);
	DEBUG("X/Y/Z self-test accels: ", self_test_accels);

	imu_fifo_clear();

	unsigned c;

	// These factory_trims values are _HARD-ENCODED_ to Board9 (Andre's
	// dev board), because calculating them properly requires
	// real-number arithmetic (see page 12 of the register PDF). We'll
	// need to figure out a way to do this properly if we want to do a
	// real self-test.
	uint8_t factory_trims[3] = { 15, 22, 37 };

	DEBUG("X/Y/Z factory trims: ", factory_trims);

#define SELF_TEST_SAMPLE_COUNT 50 // 100 Hz * 50 = .5 seconds

	const unsigned sample_count = SELF_TEST_SAMPLE_COUNT * 3;

	int16_t non_self_test_data[sample_count];
	memset(non_self_test_data, 0x55, sizeof(non_self_test_data));
	imu_fifo_read(sample_count*sizeof(int16_t), (uint8_t*)non_self_test_data);
	DEBUG("non_self_test_data: ", non_self_test_data);

	_register_write(MPU_REG_ACC_CFG, ACCEL_CFG_X_TEST|ACCEL_CFG_Y_TEST|ACCEL_CFG_Z_TEST);

	int16_t self_test_data[sample_count];
	memset(self_test_data, 0x55, sizeof(self_test_data));
	imu_fifo_read(sample_count*sizeof(int16_t), (uint8_t*)self_test_data);
	DEBUG("self_test_data: ", self_test_data);

	int16_t self_test_responses[sample_count];
	memset(self_test_responses, 0x55, sizeof(self_test_responses));
	unsigned i;
	for(i = 0, c = 0; i < sample_count; i++, c++) {
		c = c == 2 ? 0 : c;

		const uint16_t delta = self_test_data[i]-non_self_test_data[i];
		self_test_responses[i] = delta;

		if((delta >> 1) > factory_trims[c]) {
			//DEBUG("WARNING: Acc self-test out of range: channel = ", c);
			//DEBUG("WARNING: Acc self-test out of range: delta   = ", delta);
		}
	}

	DEBUG("Self-test responses: ", self_test_responses);
}

static void
_imu_set_low_pass_filter(enum imu_hz hz)
{
	// [TODO]: register constants here should be enums
	uint8_t gyro_config;
	_register_read(MPU_REG_GYRO_CFG, &gyro_config);
	gyro_config &= (~GYRO_CFG_FCHOICE_B_MASK) | GYRO_CFG_FCHOICE_11;
	_register_write(MPU_REG_GYRO_CFG, gyro_config);

	uint8_t config_register;
	_register_read(MPU_REG_CONFIG, &config_register);
	config_register &= ~CONFIG_LPF_B_MASK;

	uint8_t accel_config2;
	_register_read(MPU_REG_ACC_CFG2, &accel_config2);
	accel_config2 &= ~ACCEL_CFG2_LPF_B_MASK;

	switch(hz) {
	case IMU_HZ_0_25:
    case IMU_HZ_0_49:
    case IMU_HZ_0_98:
    case IMU_HZ_1_95:
    case IMU_HZ_3_91:
    case IMU_HZ_7_81:
        config_register |= CONFIG_LPF_1kHz_5bw;
        accel_config2 |= ACCEL_CFG2_LPF_1kHz_5bw;
		break;
    case IMU_HZ_15_63:
        config_register |= CONFIG_LPF_1kHz_10bw;
        accel_config2 |= ACCEL_CFG2_LPF_1kHz_10bw;
        break;
    case IMU_HZ_31_25:
        config_register |= CONFIG_LPF_1kHz_20bw;
        accel_config2 |= ACCEL_CFG2_LPF_1kHz_20bw;
        break;
    case IMU_HZ_62_50:
        config_register |= CONFIG_LPF_1kHz_41bw;
        accel_config2 |= ACCEL_CFG2_LPF_1kHz_41bw;
        break;
    case IMU_HZ_125:
        config_register |= CONFIG_LPF_1kHz_92bw;
        accel_config2 |= ACCEL_CFG2_LPF_1kHz_92bw;
        break;
	case IMU_HZ_250:
        config_register |= CONFIG_LPF_1kHz_184bw;
        accel_config2 |= ACCEL_CFG2_LPF_1kHz_184bw;
        break;
    case IMU_HZ_500:
        config_register |= CONFIG_LPF_1kHz_184bw;
        accel_config2 |= ACCEL_CFG2_LPF_1kHz_460bw;
		break;
	default:
		DEBUG("_imu_set_low_pass_filter has impossible hz: ", hz);
		APP_ASSERT(0);
		break;
	}

    _register_write(MPU_REG_CONFIG, config_register);
    _register_write(MPU_REG_ACC_CFG2, accel_config2);
}

static void
_reset_active_sample_rate()
{
	// The logic here is largely copied from mpu_set_sample_rate() in
	// inv_mpu.c. Using their source code to get things working seems
	// far more effective than reading their documentation.

	uint8_t interval = imu_get_sampling_interval(_settings.active_sample_rate);

    _register_write(MPU_REG_SAMPLE_RATE_DIVIDER, interval-1);

	_imu_set_low_pass_filter(_settings.active_sample_rate);
}

void
imu_set_sample_rate(enum imu_hz hz)
{
	if(_settings.active_sample_rate == hz) {
		return;
	}

    _settings.active_sample_rate = hz;

    _reset_active_sample_rate();

	DEBUG("IMU: sample rate changed to enum imu_hz ", hz);
}

uint32_t
imu_timer_ticks_from_sample_rate(uint8_t hz)
{
	// [TODO]: Redo this to leave out ticks_headroom

    uint32_t bytes_per_sample = 0;

    if(_settings.active_sensors & IMU_SENSORS_ACCEL)
        bytes_per_sample += sizeof(int16_t)*3;

    if(_settings.active_sensors & IMU_SENSORS_GYRO)
        bytes_per_sample += sizeof(int16_t)*3;

	uint32_t samples_to_fill_fifo = IMU_FIFO_CAPACITY/bytes_per_sample;
    uint32_t ticks_per_sample = APP_TIMER_CLOCK_FREQ/hz;

	uint32_t ticks_to_fill_fifo = (ticks_per_sample * samples_to_fill_fifo) >> 1;

	const uint32_t ticks_headroom = 100000; // ~0.3s in timer ticks, rounded up to nearest 8. This leaves this long before the FIFO buffer overflows.

	uint32_t ticks = ticks_to_fill_fifo - ticks_headroom;

	DEBUG("IMU timer ticks: sample rate = ", hz);
	DEBUG("IMU timer ticks: ticks = ", ticks);

	return ticks;
}

void
imu_activate()
{
	// We _must_ wake up the chip from low-power mode if we want it to
	// write to the FIFO. It looks like the chip will never write to
	// the FIFO in low-power mode, even if you set all the register
	// bits asking it to do so.
	uint8_t power_management_1;
	_register_read(MPU_REG_PWR_MGMT_1, &power_management_1);
	_register_write(MPU_REG_PWR_MGMT_1, power_management_1 & ~PWR_MGMT_1_CYCLE);

    _reset_sensors();
	_reset_active_sample_rate();

	_settings.active = true;

    nrf_delay_ms(20); // The accelerometer takes 20ms to start up from sleep mode, according to page 10 of the MPU-6500 Production Specification. See Table 2 (Accelerometer Specifications), "ACCELEROMETER STARTUP TIME, From Sleep Mode".

}

bool
imu_is_active()
{
	return _settings.active;
}

static inline void
_reset_wom_threshold()
{
    _register_write(MPU_REG_WOM_THR, _settings.wom_threshold >> 2);
}

void imu_wom_set_threshold(uint16_t microgravities)
{
	_settings.wom_threshold = microgravities;

	_reset_wom_threshold();
}

void imu_deactivate()
{
    _register_write(MPU_REG_INT_EN, 0);

	_register_write(MPU_REG_FIFO_EN, 0);

    uint8_t user_control;
    _register_read(MPU_REG_USER_CTL, &user_control);
    _register_write(MPU_REG_USER_CTL, user_control & ~USR_CTL_FIFO_EN);

    _register_write(MPU_REG_PWR_MGMT_1, 0);

    // Figure 8 (page 29) of MPU-6500 v2.0.pdf.

    _register_write(MPU_REG_PWR_MGMT_2, PWR_MGMT_2_GYRO_X_DIS|PWR_MGMT_2_GYRO_Y_DIS|PWR_MGMT_2_GYRO_Z_DIS);
    _imu_set_low_pass_filter(IMU_HZ_250);
    _register_write(MPU_REG_INT_EN, INT_EN_WOM);
    _register_write(MPU_REG_ACCEL_INTEL_CTRL, ACCEL_INTEL_CTRL_EN|ACCEL_INTEL_CTRL_6500_MODE);
    _reset_wom_threshold();
    _register_write(MPU_REG_ACCEL_ODR, (uint8_t)_settings.inactive_sample_rate);

	{
        // We have to delay a certain amount of time _after_ setting
        // the CYCLE bit on but _before_ clearing the interrupt status
        // register. If we don't delay, WOM_INT (the wake-on-motion
        // interrupt) gets triggered immediately, even if no motion
        // has occurred. The amount of delay that's required appears
        // to depend on the sample rate chosen in the low-power
        // accelerometer mode: we delay for just over one sampling
        // interval's worth of milliseconds. "Just over" means we add
        // another millisecond or 10 to the sampling interval time, to
        // account for the hardware being stupid. (If we use delay for
        // exactly one sampling interval, the MPU-6500 still decides
        // to retrigger the wake-on-motion interrupt all the
        // time. Sigh.)

        _register_write(MPU_REG_PWR_MGMT_1, PWR_MGMT_1_CYCLE|PWR_MGMT_1_PD_PTAT);

        unsigned delay = imu_get_sampling_interval(_settings.inactive_sample_rate) + (12 - (unsigned)_settings.inactive_sample_rate);
        nrf_delay_ms(delay);

        _clear_interrupt_status_register();
	}

    _settings.active = false;

}

static void _low_power_setup() __attribute__((unused));
static void _low_power_setup()
{
    // This code isn't used at all (thus the unused attribute in the
    // function declaration); it's just here for safe-keeping.

    // Attempt to get low-power mode to work, from reverse-engineering inv_mpu.c.

    _register_write(MPU_REG_INT_EN, 0);
    _register_write(MPU_REG_USER_CTL, 0);
    _register_write(MPU_REG_PWR_MGMT_1, 0);
    _register_write(MPU_REG_PWR_MGMT_2, PWR_MGMT_2_GYRO_X_DIS|PWR_MGMT_2_GYRO_Y_DIS|PWR_MGMT_2_GYRO_Z_DIS);
    _register_write(MPU_REG_WOM_THR, 5);
    _register_write(MPU_REG_ACCEL_ODR, 5);
    _register_write(MPU_REG_ACCEL_INTEL_CTRL, ACCEL_INTEL_CTRL_EN|ACCEL_INTEL_CTRL_6500_MODE);
    _register_write(MPU_REG_PWR_MGMT_1, PWR_MGMT_1_CYCLE);
    _register_write(MPU_REG_INT_EN, INT_EN_WOM);
}

void
imu_init(enum SPI_Channel channel) {
	_chan = channel;

	// Reset procedure as per "MPU-6500 Register Map and Descriptions Revision 2.0"
	// page 43

	// Reset chip
	PRINTS("IMU reset\r\n");
	_register_write(MPU_REG_PWR_MGMT_1, PWR_MGMT_1_RESET);

	nrf_delay_ms(100);

	_register_write(MPU_REG_PWR_MGMT_1, 0);

	// Check for valid Chip ID
	uint8_t whoami_value;
	_register_read(MPU_REG_WHO_AM_I, &whoami_value);

	if (whoami_value != CHIP_ID) {
		DEBUG("Invalid MPU-6500 ID found. Expected 0x70, got 0x", whoami_value);
		APP_ASSERT(0);
	}

	// Reset buffers
	_register_write(MPU_REG_SIG_RST, 0xFF);

	nrf_delay_ms(100);

	// Init interrupts
	_register_write(MPU_REG_INT_CFG, INT_CFG_ACT_LO | INT_CFG_PUSH_PULL | INT_CFG_LATCH_OUT | INT_CFG_CLR_ON_STS | INT_CFG_BYPASS_EN);

	// Config interrupts
	// _register_write(MPU_REG_INT_EN, INT_EN_FIFO_OVRFLO);

	// Init accel
	_register_write(MPU_REG_ACC_CFG, ACCEL_CFG_SCALE_2G);

	// Init Gyro
	_register_write(MPU_REG_GYRO_CFG, (GYRO_CFG_RATE_2k_DPS << GYRO_CFG_RATE_OFFET));

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

	// Set up wake-on-motion stuff

    _reset_sensors();
    _reset_active_sample_rate();

	imu_deactivate();

	PRINTS("IMU: initialization done.\r\n");
}
