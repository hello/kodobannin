#include <spi.h>
#include <simple_uart.h>
#include <util.h>
#include <device_params.h>

#define BUF_SIZE 16
#define CHIP_ID 0x70

enum MPU_Registers {
	MPU_REG_WHO_AM_I = 0x75
};

static enum SPI_Channel chan;
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

uint32_t
imu_init(enum SPI_Channel channel) {
	uint32_t err;
	chan = channel;

	simple_uart_putstring((const uint8_t *)"MPU-6500 Chip ID\n");

	buf[0] = SPI_Read(MPU_REG_WHO_AM_I);
	err = spi_xfer(chan, IMU_SPI_nCS, 2, buf, buf);

	// print response to uart
	imu_uart_debug(err, buf, 2);

	if (buf[1] != CHIP_ID) {
		simple_uart_putstring((const uint8_t *)"Invalid MPU-6500 ID found. Expected 0x70, got 0x");
		serial_print_hex(&buf[1], 1);
		simple_uart_put('\n');
		return -1;
	}

	return 0;
}

