#include <spi.h>
#include <simple_uart.h>
#include <util.h>
#include <device_params.h>
#include <mpu-6500.h>
#include <app_error.h>
#include <nrf_delay.h>

#define BUF_SIZE 4

static enum SPI_Channel chan = SPI_Channel_Invalid;
static uint8_t buf[BUF_SIZE];

static inline bool
register_read(MPU_Register_t register_address, uint8_t* const out_value)
{
	uint8_t buf[2];
	buf[0] = SPI_Read(register_address);

	bool success = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	if(success) {
		*out_value = buf[1];
	}

	return success;
}

static inline bool
register_write(MPU_Register_t register_address, uint8_t value)
{
	uint8_t buf[2] = { SPI_Write(register_address), value };

	return spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
}

static void
imu_uart_debug(const uint32_t result, const uint8_t *buf, const uint32_t len) {
	int i;
	PRINT_HEX(&result, 4);
	PRINTS(": ");
	for (i = 0; i < len; i++)
		PRINT_HEX(&buf[i], 1);
	PRINTC('\r');PRINTC('\n');
}

uint16_t
imu_get_fifo_count() {
	uint16_t count;
	uint32_t err;

	APP_ERROR_CHECK(chan == SPI_Channel_Invalid);

	buf[0] = SPI_Read(MPU_REG_FIFO_CNT_HI);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	APP_ERROR_CHECK(!err);

	count = buf[1] << 8;

	buf[0] = SPI_Read(MPU_REG_FIFO_CNT_LO);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	APP_ERROR_CHECK(!err);

	count |= buf[1];

	return count;
}

uint16_t
imu_fifo_read(uint16_t count, uint8_t *buf) {
	uint16_t avail;

	avail = imu_get_fifo_count();

	if (count > avail)
		count = avail;

	if (count == 0)
		return 0;
	
	return spi_read_multi(chan, IMU_SPI_nCS, SPI_Read(MPU_REG_FIFO), count, buf);
}

void
imu_reset_fifo() {
	uint32_t err;
	// Reset FIFO, disable i2c, and clear regs
	PRINTS("FIFO / buffer reset\n");
	buf[0] = SPI_Write(MPU_REG_USER_CTL);
	buf[1] = USR_CTL_FIFO_RST | USR_CTL_SIG_RST;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');
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
imu_accelerometer_continuous()
{
#define P_LSB16(p_int16) ((uint8_t*)(p_int16)+0)
#define P_MSB16(p_int16) ((uint8_t*)(p_int16)+1)

	int16_t values[3];

	for(;;) {
		register_read(MPU_REG_ACC_X_HI, (uint8_t*)&values[0]+1);
		register_read(MPU_REG_ACC_X_LO, P_LSB16(&values[0]));
		register_read(MPU_REG_ACC_Y_HI, P_LSB16(&values[1]));
		register_read(MPU_REG_ACC_Y_LO, P_MSB16(&values[1]));
		register_read(MPU_REG_ACC_Z_HI, P_LSB16(&values[2]));
		register_read(MPU_REG_ACC_Z_LO, P_MSB16(&values[2]));

		PRINT_HEX(&values[0], sizeof(values[0]));
		PRINT_HEX(&values[1], sizeof(values[1]));
		PRINT_HEX(&values[2], sizeof(values[2]));

		PRINTS("\r\n");
	}
}

/*
static void imu_accelerometer_self_test()
{
	bool success = false;

	//register_write(MPU_REG_ACC_CFG, ACCEL_CFG_SCALE_8G);

	//register_write(MPU_REG_FIFO_EN, FIFO_EN_QUEUE_ACCEL);

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

	success = register_write(MPU_REG_ACC_CFG, AX_ST_EN|AY_ST_EN|AZ_ST_EN);
	APP_ERROR_CHECK(!success);

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
	uint32_t err;
	chan = channel;

	// Reset procedure as per "MPU-6500 Register Map and Descriptions Revision 2.0"
	// page 43

	// Reset chip
	PRINTS("Chip reset\n");
	buf[0] = SPI_Write(MPU_REG_PWR_MGMT_1);
	buf[1] = MPU_REG_PWR_MGMT_1_RESET;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);

	nrf_delay_ms(100);

	PRINTS("Chip wakeup\n");
	buf[0] = SPI_Write(MPU_REG_PWR_MGMT_1);
	buf[1] = 0;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);

	// Check for valid Chip ID
	simple_uart_putstring((const uint8_t *)"MPU-6500 Chip ID\n");

	buf[0] = SPI_Read(MPU_REG_WHO_AM_I);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);

	// print response to uart
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	if (buf[1] != CHIP_ID) {
		PRINTS("Invalid MPU-6500 ID found. Expected 0x70, got 0x");
		serial_print_hex(&buf[1], 1);
		return -1;
	}

	// Reset buffers
	PRINTS("Signal reset\n");
	buf[0] = SPI_Write(MPU_REG_SIG_RST);
	buf[1] = 0xFF;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);

	nrf_delay_ms(100);

	// Init interrupts
	PRINTS("Int Init\n");
	buf[0] = SPI_Write(MPU_REG_INT_CFG);
	buf[1] = INT_CFG_ACT_HI | INT_CFG_PUSH_PULL | INT_CFG_LATCH_OUT | INT_CFG_CLR_ON_STS | INT_CFG_BYPASS_EN;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	// Config interrupts
	PRINTS("Int config\n");
	buf[0] = SPI_Write(MPU_REG_INT_EN);
	buf[1] = INT_EN_FIFO_OVRFLO;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	// Init accel
	PRINTS("Accel config\n");
	buf[0] = SPI_Write(MPU_REG_ACC_CFG);
	buf[1] = ACCEL_CFG_SCALE_8G;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	bool success = false;
	success = register_write(MPU_REG_SAMPLE_RATE_DIVIDER, 9); // 1000=(1+9) = 100 Hz
	APP_ERROR_CHECK(!success);

	/*
	// Init Gyro
	PRINTS("Gyro config\n");
	buf[0] = SPI_Write(MPU_REG_GYRO_CFG);
	buf[1] = (GYRO_CFG_RATE_2k_DPS << GYRO_CFG_RATE_OFFET);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');
	*/

	// Reset FIFO, disable i2c, and clear regs
	PRINTS("FIFO / buffer reset\n");
	buf[0] = SPI_Write(MPU_REG_USER_CTL);
	buf[1] = USR_CTL_FIFO_EN | USR_CTL_I2C_DIS | USR_CTL_FIFO_RST | USR_CTL_SIG_RST;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	// Init FIFO
	PRINTS("FIFO config\n");
	buf[0] = SPI_Write(MPU_REG_FIFO_EN);
	buf[1] = FIFO_EN_QUEUE_ACCEL; // | FIFO_EN_QUEUE_GYRO_X | FIFO_EN_QUEUE_GYRO_Y | FIFO_EN_QUEUE_GYRO_Z;// | FIFO_EN_QUEUE_ACCEL;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');


#endif

	return 0;
}

