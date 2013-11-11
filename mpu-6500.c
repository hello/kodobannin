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
	PRINTS("\r\n");
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
imu_accel_reg_read(uint8_t *buf) {
	bool good = 0;

	good = register_read(MPU_REG_ACC_X_LO, buf++);
	APP_ERROR_CHECK(!good);
	good = register_read(MPU_REG_ACC_X_HI, buf++);
	APP_ERROR_CHECK(!good);
	good = register_read(MPU_REG_ACC_Y_LO, buf++);
	APP_ERROR_CHECK(!good);
	good = register_read(MPU_REG_ACC_Y_HI, buf++);
	APP_ERROR_CHECK(!good);
	good = register_read(MPU_REG_ACC_Z_LO, buf++);
	APP_ERROR_CHECK(!good);
	good = register_read(MPU_REG_ACC_Z_HI, buf++);
	APP_ERROR_CHECK(!good);

	return 6;
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

#if 0
	// read chip rev
	/*
	#define MPU6500_MEM_REV_ADDR    (0x17)
    if (mpu_read_mem(MPU6500_MEM_REV_ADDR, 1, &rev))
        return -1;
    if (rev == 0x1)
        st.chip_cfg.accel_half = 0;
    else {
        log_e("Unsupported software product rev %d.\n", rev);
        return -1;
    }
    */

    // if using DMP, disable first 3k of FIFO memory
    /*
    // MPU6500 shares 4kB of memory between the DMP and the FIFO. Since the
    // first 3kB are needed by the DMP, we'll use the last 1kB for the FIFO.
    
    data[0] = BIT_FIFO_SIZE_1024 | 0x8;
    if (i2c_write(st.hw->addr, st.reg->accel_cfg2, 1, data))
        return -1;
    */

    //now do
    /*
       if (mpu_set_gyro_fsr(2000))
        return -1;
    if (mpu_set_accel_fsr(2))
        return -1;
    if (mpu_set_lpf(42))
        return -1;
    if (mpu_set_sample_rate(50))
        return -1;
    if (mpu_configure_fifo(0))
        return -1;

    if (int_param)
        reg_int_cb(int_param);
    // Already disabled by setup_compass.
    if (mpu_set_bypass(0))
        return -1;

    mpu_set_sensors(0);
    */
    // Init Gyro
	PRINTS("Gyro config\n");
	buf[0] = SPI_Write(MPU_REG_GYRO_CFG);
	buf[1] = (GYRO_CFG_RATE_2k_DPS << GYRO_CFG_RATE_OFFET);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	// Init accel
	PRINTS("Accel config\n");
	buf[0] = SPI_Write(MPU_REG_ACC_CFG);
	buf[1] = ACCEL_CFG_SCALE_16G;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	//Zero out FIFO
	// Init FIFO
	PRINTS("FIFO config\n");
	buf[0] = SPI_Write(MPU_REG_FIFO_EN);
	buf[1] = 0;//FIFO_EN_QUEUE_GYRO_X | FIFO_EN_QUEUE_GYRO_Y | FIFO_EN_QUEUE_GYRO_Z | FIFO_EN_QUEUE_ACCEL;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	// Reset FIFO and clear regs
	PRINTS("FIFO / buffer reset\n");
	buf[0] = SPI_Write(MPU_REG_USER_CTL);
	buf[1] = USR_CTL_FIFO_RST | USR_CTL_SIG_RST;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	// Reset FIFO, disable i2c, and clear regs
	PRINTS("FIFO / buffer reset\n");
	buf[0] = SPI_Write(MPU_REG_USER_CTL);
	buf[1] = USR_CTL_FIFO_EN | USR_CTL_I2C_DIS | USR_CTL_FIFO_RST | USR_CTL_SIG_RST;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');	

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

	// Init FIFO
	PRINTS("FIFO config\n");
	buf[0] = SPI_Write(MPU_REG_FIFO_EN);
	buf[1] = FIFO_EN_QUEUE_GYRO_X | FIFO_EN_QUEUE_GYRO_Y | FIFO_EN_QUEUE_GYRO_Z | FIFO_EN_QUEUE_ACCEL;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');
#endif

#if 1
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
	buf[1] = (ACCEL_CFG_SCALE_8G << ACCEL_CFG_SCALE_OFFSET);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

	// Init Gyro
	PRINTS("Gyro config\n");
	buf[0] = SPI_Write(MPU_REG_GYRO_CFG);
	buf[1] = (GYRO_CFG_RATE_2k_DPS << GYRO_CFG_RATE_OFFET);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');

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
	buf[1] = FIFO_EN_QUEUE_ACCEL | FIFO_EN_QUEUE_GYRO_X | FIFO_EN_QUEUE_GYRO_Y | FIFO_EN_QUEUE_GYRO_Z;// | FIFO_EN_QUEUE_ACCEL;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	PRINTC('\n');


#endif

	return 0;
}

