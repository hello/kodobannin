#include "micro-ecc/ecc.h"
#include <app_error.h>
#include <app_gpiote.h>
#include <nrf_gpio.h>
#include <nrf_delay.h>
#include <nrf_soc.h>
#include <app_timer.h>
#include <device_params.h>
#include <ble_err.h>
#include <ble_bas.h>
#include <simple_uart.h>
#include "sha.h"
#include <dfu_types.h>
#include <dfu_transport.h>
#include <nrf_sdm.h>
#include <ble_dfu.h>

#define IRQ_ENABLED             0x01           /**< Field identifying if an interrupt is enabled. */
#define MAX_NUMBER_INTERRUPTS   32             /**< Maximum number of interrupts available. */

#define APP_GPIOTE_MAX_USERS            2      /**< Number of GPIOTE users in total. Used by button module and dfu_transport_serial module (flow control). */

extern void ble_init(void);
extern void ble_advertising_start(void);
extern uint32_t ble_services_update_battery_level(uint8_t level);
extern uint32_t dfu_init_state(void);

static app_timer_id_t m_battery_timer_id; /**< Battery timer. */

/**@brief Perform battery measurement, and update Battery Level characteristic in Battery Service.
 */
static void battery_level_update(void)
{
    uint32_t err_code;
    uint8_t  battery_level;

    battery_level = 69;

    err_code = ble_services_update_battery_level(battery_level);
    if (
        (err_code != NRF_SUCCESS)
        &&
        (err_code != NRF_ERROR_INVALID_STATE)
        &&
        (err_code != BLE_ERROR_NO_TX_BUFFERS)
        &&
        (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
    )
    {
        APP_ERROR_HANDLER(err_code);
    }
}

/**@brief Battery measurement timer timeout handler.
 *
 * @details This function will be called each time the battery level measurement timer expires.
 *
 * @param[in]   p_context   Pointer used for passing some arbitrary information (context) from the
 *                          app_start_timer() call to the timeout handler.
 */
static void battery_level_meas_timeout_handler(void * p_context)
{
    (void)p_context;
    battery_level_update();
}

/**@brief Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void
timers_init(void)
{
    uint32_t err_code;

    // Initialize timer module
    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, false);

    // Create timers
    err_code = app_timer_create(&m_battery_timer_id,
                                APP_TIMER_MODE_REPEATED,
                                battery_level_meas_timeout_handler);
    APP_ERROR_CHECK(err_code);
}

static void
application_timers_start(void)
{
    uint32_t err_code;

    // Start application timers
    err_code = app_timer_start(m_battery_timer_id, BATTERY_LEVEL_MEAS_INTERVAL, NULL);
    APP_ERROR_CHECK(err_code);
}

static void
sha1_fw_area(uint8_t *hash)
{
    SHA_CTX ctx;
    uint32_t code_size    = DFU_IMAGE_MAX_SIZE_FULL;
    uint8_t *code_address = (uint8_t *)CODE_REGION_1_START;
    uint32_t *index = (uint32_t *)BOOTLOADER_REGION_START - DFU_APP_DATA_RESERVED - 4;

    // walk back to the end of the actual firmware
    while (*index-- == EMPTY_FLASH_MASK && code_size > 0)
        code_size -= 4;

    SHA1_Init(&ctx);
    SHA1_Update(&ctx, (void *)code_address, code_size);
    SHA1_Final(hash, &ctx);
}

static bool
verify_fw_sha1(uint8_t *valid_hash)
{
    uint8_t sha1[SHA1_DIGEST_LENGTH];
    uint8_t comp = 0;
    int i = 0;

    sha1_fw_area(sha1);

    for (i = 0; i < SHA1_DIGEST_LENGTH; i++)
        comp |= sha1[i] ^ valid_hash[i];

    return comp == 0;
}

/**@brief Function for disabling all interrupts before jumping from bootloader to application.
 */
static void
interrupts_disable(void)
{
    uint32_t interrupt_setting_mask;
    uint8_t  irq;

    // We start the loop from first interrupt, i.e. interrupt 0.
    irq                    = 0;
    // Fetch the current interrupt settings.
    interrupt_setting_mask = NVIC->ISER[0];
    
    for (; irq < MAX_NUMBER_INTERRUPTS; irq++)
    {
        if (interrupt_setting_mask & (IRQ_ENABLED << irq))
        {
            // The interrupt was enabled, and hence disable it.
            NVIC_DisableIRQ((IRQn_Type) irq);
        }
    }        
}

static void
boot_firmware(uint32_t addr) {
    asm("LDR   R2, [R0]");//Get App MSP.
    asm("MSR   MSP, R2"); //Set the main stack pointer to the applications MSP.
    asm("LDR   R3, [R0, #0x00000004]"); //Get application reset vector address.
    asm("BX    R3"); // No return - stack code is now activated only through SVC and plain interrupts.
}

void
bootloader_dfu_update_process(dfu_update_status_t update_status) {
    if (update_status.status_code == DFU_TIMEOUT)
    {
        // Timeout has occurred. Close the connection with the DFU Controller.
        uint32_t err_code = dfu_transport_close();
        APP_ERROR_CHECK(err_code);
    }
}

void
_start()
{
    //NRF_UICR_Type *uicr = NRF_UICR;
    //NRF_FICR_Type *ficr = NRF_FICR;
	uint32_t err_code;

    // XXX: get signature blob from App Data section
    uint8_t valid_fw[SHA1_DIGEST_LENGTH] = {0};

    APP_TIMER_INIT(APP_TIMER_PRESCALER, APP_TIMER_MAX_TIMERS, APP_TIMER_OP_QUEUE_SIZE, true);
    APP_GPIOTE_INIT(APP_GPIOTE_MAX_USERS);

    //leds
    nrf_gpio_cfg_output(21);
    nrf_gpio_cfg_output(22);
    nrf_gpio_cfg_output(23);

    nrf_gpio_pin_set(21);
    nrf_delay_ms(500);
    // measure and verify current firmware
    if (verify_fw_sha1(valid_fw)) {
        //nrf_gpio_pin_set(22);
        nrf_gpio_pin_clear(21);

        // boot FW if valid
        interrupts_disable();

        uint32_t err_code = sd_softdevice_forward_to_application();
        APP_ERROR_CHECK(err_code);
        
        boot_firmware(CODE_REGION_1_START);
    } else {
        nrf_gpio_pin_set(22);
        nrf_gpio_pin_clear(21);
        // else enter DFU over BLE mode
        err_code = dfu_init_state(); 
        if (err_code == NRF_SUCCESS)    
        {
            err_code = dfu_transport_update_start();
        }
    }
    // XXX: implement recovery from external flash

	/*
	nrf_gpio_cfg_output(GPIO_3v3_Enable);
	nrf_gpio_pin_set(GPIO_3v3_Enable);
	

	simple_uart_config(0, 9, 0, 10, false);
	//timers_init();
	ble_init();

	//< hack for when timers are disabled
	battery_level_update();

	//application_timers_start();

	ble_advertising_start();
	while(1) {
		err_code = sd_app_event_wait();
		APP_ERROR_CHECK(err_code);
	}
/*
	nrf_gpio_cfg_output(GPIO_3v3_Enable);
	nrf_gpio_pin_set(GPIO_3v3_Enable);
	nrf_gpio_cfg_output(GPIO_HRS_PWM);

	while (1){
		nrf_gpio_pin_set(GPIO_HRS_PWM);
		nrf_delay_ms(500);
		nrf_gpio_pin_clear(GPIO_HRS_PWM);
		nrf_delay_ms(500);
	}
*/
}
