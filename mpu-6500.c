// vi:sw=4:ts=4

#include <spi.h>
#include <simple_uart.h>
#include <util.h>
#include <device_params.h>
#include <mpu-6500.h>
#include <app_error.h>
#include <nrf_delay.h>

#define BUF_SIZE 4

#define SENSORS FIFO_EN_QUEUE_ACCEL
//#define SENSORS (FIFO_EN_QUEUE_GYRO_X | FIFO_EN_QUEUE_GYRO_Y | FIFO_EN_QUEUE_GYRO_Z | FIFO_EN_QUEUE_ACCEL)

static enum SPI_Channel chan = SPI_Channel_Invalid;

static inline void
_register_read(MPU_Register_t register_address, uint8_t* const out_value)
{
	uint8_t buf[2];
	buf[0] = SPI_Read(register_address);

	bool success = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	APP_ASSERT(success);

	*out_value = buf[1];
}

static inline void
_register_write(MPU_Register_t register_address, uint8_t value)
{
	uint8_t buf[2] = { SPI_Write(register_address), value };

	bool success = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	APP_ASSERT(success);
}

static void
_fifo_reset()
{
	_register_write(MPU_REG_INT_EN, 0);
	_register_write(MPU_REG_FIFO_EN, 0);
	_register_write(MPU_REG_USER_CTL, 0);

	_register_write(MPU_REG_USER_CTL, USR_CTL_FIFO_RST);
	_register_write(MPU_REG_USER_CTL, USR_CTL_FIFO_EN);
	nrf_delay_ms(50);

	_register_write(MPU_REG_INT_EN, INT_EN_FIFO_OVRFLO);
	_register_write(MPU_REG_INT_EN, 0x1);
	_register_write(MPU_REG_INT_EN, 0);

	_register_write(MPU_REG_FIFO_EN, SENSORS);
}

static void
_uart_debug(const uint32_t result, const uint8_t *buf, const uint32_t len) {
	int i;
	PRINT_HEX(&result, 4);
	PRINTS(": ");
	for (i = 0; i < len; i++)
		PRINT_HEX(&buf[i], 1);
	PRINTS("\r\n");
}

uint16_t
imu_get_fifo_count() {
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

uint16_t
imu_fifo_read(uint16_t count, uint8_t *buf) {
	uint16_t avail;

	avail = imu_get_fifo_count();

	if (avail < count)
		count = avail;

	if (count == 0)
		return 0;

	DEBUG("count: ", count);
	return spi_read_multi(chan, IMU_SPI_nCS, SPI_Read(MPU_REG_FIFO), count, buf);
}

void
imu_reset_fifo() {
	_register_write(MPU_REG_USER_CTL, USR_CTL_FIFO_RST); // | USR_CTL_SIG_RST);
	_register_write(MPU_REG_USER_CTL, USR_CTL_FIFO_EN);
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

static void
_continuous()
{
	union fifo_buffer {
		uint8_t bytes[4096];
		uint32_t uint32s[1024];

		int16_t values[2048];
	};

	union fifo_buffer buffer;

	for(;;) {
		uint16_t bufsize = imu_fifo_read(4096, buffer.bytes);
		DEBUG("bufsize: ", bufsize);

		unsigned i = 0;
		union generic_pointer p;
		for(p.pi16 = buffer.values; p.p8 < buffer.bytes+bufsize; p.pi16++) {
			*p.pi16 = bswap16(*p.pi16);
			PRINT_HEX(p.pi16, sizeof(*p.pi16));
			i++;
			if(i == 3) {
				PRINTS("\r\n");
				i = 0;
			}
		}

		PRINTS("\r\n");

		uint8_t int_status;
		_register_read(MPU_REG_INT_STS, &int_status);
		if(int_status & INT_STS_FIFO_OVRFLO) {
		}

		nrf_delay_ms(8000);
	}
}

/*
static void imu_accelerometer_self_test()
{
	bool success = false;

	//_register_write(MPU_REG_ACC_CFG, ACCEL_CFG_SCALE_8G);

	//_register_write(MPU_REG_FIFO_EN, SENSORS);

	// See page 11 of the MPU-6500 Register Map descriptions for this algorithm

	uint8_t self_test_accels[3];
	memset(self_test_accels, 0x55, sizeof(self_test_accels));

	success = register_read(MPU_REG_ACC_SELF_TEST_X, &self_test_accels[0]);
	APP_ERROR_CHECK(!success);

	register_read(MPU_REG_ACC_SELF_TEST_Y, &self_test_accels[1]);
	APP_ERROR_CHECK(!success);

	register_read(MPU_REG_ACC_SELF_TEST_Z, &self_test_accels[2]);
	APP_ERROR_CHECK(!success);

	DEBUG("X/Y/Z self-test accels: ", self_test_accels);

	imu_reset_fifo();

	unsigned c;

	// These factory_trims values are _HARD-CODED_ to Board9 (Andre's
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
	imu_fifo_read(sample_count*sizeof(int16_t), non_self_test_data);
	DEBUG("non_self_test_data: ", non_self_test_data);

	_register_write(MPU_REG_ACC_CFG, AX_ST_EN|AY_ST_EN|AZ_ST_EN);

	int16_t self_test_data[sample_count];
	memset(self_test_data, 0x55, sizeof(self_test_data));
	imu_fifo_read(sample_count*sizeof(int16_t), self_test_data);
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
*/

uint32_t
imu_init(enum SPI_Channel channel) {
	chan = channel;

	// Reset procedure as per "MPU-6500 Register Map and Descriptions Revision 2.0"
	// page 43

	// Reset chip
	PRINTS("Chip reset\r\n");
	_register_write(MPU_REG_PWR_MGMT_1, PWR_MGMT_1_RESET);

	nrf_delay_ms(100);

	PRINTS("Chip wakeup\r\n");
	_register_write(MPU_REG_PWR_MGMT_1, 0);

	// Check for valid Chip ID
	uint8_t whoami_value;
	_register_read(MPU_REG_WHO_AM_I, &whoami_value);

	if (whoami_value != CHIP_ID) {
		DEBUG("Invalid MPU-6500 ID found. Expected 0x70, got 0x", whoami_value);
		return -1;
	}

	// Reset buffers
	_register_write(MPU_REG_SIG_RST, 0xFF);

	nrf_delay_ms(100);

	// Init interrupts
	_register_write(MPU_REG_INT_CFG, INT_CFG_ACT_HI | INT_CFG_PUSH_PULL | INT_CFG_LATCH_OUT | INT_CFG_CLR_ON_STS | INT_CFG_BYPASS_EN);

	// Config interrupts
	_register_write(MPU_REG_INT_EN, INT_EN_FIFO_OVRFLO);

	// Set sample rate div  F = (DPLF_Freq / (Sample Rate Div + 1))
	_register_write(MPU_REG_SAMPLE_RATE_DIVIDER, 9);

	// Init accel
	_register_write(MPU_REG_ACC_CFG, ACCEL_CFG_SCALE_2G);

	// Set Accel Low Pass Filter, and make FIFO size 4096
	_register_write(MPU_REG_ACC_CFG2, ACCEL_CFG2_FCHOICE_1 | ACCEL_CFG2_LPF_1kHz_41bw | ACCEL_CFG2_FIFO_SIZE_4096);

	// Set Gyro Low Pass Filter
	_register_write(MPU_REG_CONFIG, CONFIG_LPF_1kHz_41bw);

	//_register_write(MPU_REG_SAMPLE_RATE_DIVIDER, 9); // 1000=(1+9) = 100 Hz

	// Init Gyro
	_register_write(MPU_REG_GYRO_CFG, (GYRO_CFG_RATE_2k_DPS << GYRO_CFG_RATE_OFFET));

	// Init FIFO
	_register_write(MPU_REG_FIFO_EN, SENSORS); // FIFO_EN_QUEUE_GYRO_X | FIFO_EN_QUEUE_GYRO_Y | FIFO_EN_QUEUE_GYRO_Z); //

	// Reset FIFO, disable i2c, and clear regs
	_register_write(MPU_REG_USER_CTL, USR_CTL_FIFO_EN | USR_CTL_I2C_DIS | USR_CTL_FIFO_RST | USR_CTL_SIG_RST);

	_continuous();

	return 0;
}
