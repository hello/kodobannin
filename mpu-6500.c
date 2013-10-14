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

static void
imu_uart_debug(const uint32_t result, const uint8_t *buf, const uint32_t len) {
	int i;
	serial_print_hex((uint8_t *)&result, 4);
	simple_uart_putstring((const uint8_t *)": ");
	for (i = 0; i < len; i++)
		serial_print_hex((uint8_t *)&buf[i], 1);
	simple_uart_put('\n');
}

uint16_t
imu_get_fifo_count() {
	uint16_t count;
	uint32_t err;

	APP_ERROR_CHECK(chan != SPI_Channel_Invalid);

	buf[0] = SPI_Read(MPU_REG_FIFO_CNT_HI);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);

	count = buf[1] << 8;

	buf[0] = SPI_Read(MPU_REG_FIFO_CNT_LO);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);

	count |= buf[1];

	return count;
}

uint32_t
imu_init(enum SPI_Channel channel) {
	uint32_t err;
	chan = channel;

	// Check for valid Chip ID
	simple_uart_putstring((const uint8_t *)"MPU-6500 Chip ID\n");

	buf[0] = SPI_Read(MPU_REG_WHO_AM_I);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);

	// print response to uart
	imu_uart_debug(err, buf, 2);
	simple_uart_put('\n');

	if (buf[1] != CHIP_ID) {
		simple_uart_putstring((const uint8_t *)"Invalid MPU-6500 ID found. Expected 0x70, got 0x");
		serial_print_hex(&buf[1], 1);
		return -1;
	}

	// Reset procedure as per "MPU-6500 Register Map and Descriptions Revision 2.0"
	// page 43

	// Reset chip
	simple_uart_putstring((const uint8_t *)"Chip reset\n");
	buf[0] = SPI_Write(MPU_REG_SIG_RST);
	buf[1] = MPU_REG_PWR_MGMT_1_RESET;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);

	nrf_delay_ms(100);

	// Reset buffers
	simple_uart_putstring((const uint8_t *)"Signal reset\n");
	buf[0] = SPI_Write(MPU_REG_SIG_RST);
	buf[1] = 0xFF;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);

	nrf_delay_ms(100);

	// Init interrupts
	simple_uart_putstring((const uint8_t *)"Int Init\n");
	buf[0] = SPI_Write(MPU_REG_INT_CFG);
	buf[1] = INT_CFG_ACT_HI | INT_CFG_PUSH_PULL | INT_CFG_LATCH_OUT | INT_CFG_CLR_ON_STS | INT_CFG_BYPASS_EN;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	simple_uart_put('\n');

	// Config interrupts
	simple_uart_putstring((const uint8_t *)"Int config\n");
	buf[0] = SPI_Write(MPU_REG_INT_EN);
	buf[1] = INT_EN_FIFO_OVRFLO;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	simple_uart_put('\n');

	// Reset FIFO, disable i2c, and clear regs
	simple_uart_putstring((const uint8_t *)"FIFO / buffer reset\n");
	buf[0] = SPI_Write(MPU_REG_USER_CTL);
	buf[1] = USR_CTL_FIFO_EN | USR_CTL_I2C_DIS | USR_CTL_FIFO_RST | USR_CTL_SIG_RST;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	simple_uart_put('\n');

	// Init FIFO
	simple_uart_putstring((const uint8_t *)"FIFO config\n");
	buf[0] = SPI_Write(MPU_REG_FIFO_EN);
	buf[1] = FIFO_EN_QUEUE_GYRO_X | FIFO_EN_QUEUE_GYRO_Y | FIFO_EN_QUEUE_GYRO_Z | FIFO_EN_QUEUE_ACCEL;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	simple_uart_put('\n');

	// Init accel
	simple_uart_putstring((const uint8_t *)"Accel config\n");
	buf[0] = SPI_Write(MPU_REG_ACC_CFG);
	buf[1] = ACCEL_CFG_SCALE_2G;
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	simple_uart_put('\n');

	// Init Gyro
	simple_uart_putstring((const uint8_t *)"Gyro config\n");
	buf[0] = SPI_Write(MPU_REG_GYRO_CFG);
	buf[1] = (GYRO_CFG_RATE_250_DPS << GYRO_CFG_RATE_OFFET);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);
	imu_uart_debug(err, buf, 2);
	simple_uart_put('\n');


	return 0;
}

