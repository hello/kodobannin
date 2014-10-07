#include "gpio_nor.h"

inline void gpio_cfg_d0s1_output_disconnect(uint32_t pin_number)
{
    /*lint -e{845} // A zero has been given as right argument to operator '|'" */
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
                                            | (GPIO_PIN_CNF_DRIVE_D0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                                            | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
                                            | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
                                            | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
    nrf_gpio_pin_clear(pin_number);
}

inline void gpio_cfg_d0s1_output_disconnect_pull(uint32_t pin_number, nrf_gpio_pin_pull_t pin_pull)
{
    /*lint -e{845} // A zero has been given as right argument to operator '|'" */
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
                                            | (GPIO_PIN_CNF_DRIVE_D0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                                            | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
                                            | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
                                            | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
    nrf_gpio_pin_clear(pin_number);
}


inline void gpio_cfg_s0s1_output_connect(uint32_t pin_number, uint8_t init_val)
{
    /*lint -e{845} // A zero has been given as right argument to operator '|'" */
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
                                            | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                                            | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
                                            | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
                                            | (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
    nrf_gpio_pin_write(pin_number, init_val);
}


inline void gpio_input_disconnect(uint32_t pin_number)
{
    /*lint -e{845} // A zero has been given as right argument to operator '|'" */
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
                                            | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos)
                                            | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
                                            | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
                                            | (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos);
}

inline void gpio_cfg_h0d1_opendrain_output_connect(uint32_t pin_number, uint8_t init_val, nrf_gpio_pin_pull_t pin_pull)
{
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) \
        |(GPIO_PIN_CNF_DRIVE_H0D1 << GPIO_PIN_CNF_DRIVE_Pos)    \
        |(pin_pull << GPIO_PIN_CNF_PULL_Pos)  \
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) \
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
    nrf_gpio_pin_write(pin_number, init_val);
}

inline void gpio_cfg_d0s1_opendrain_output_connect(uint32_t pin_number, uint8_t init_val, nrf_gpio_pin_pull_t pin_pull)
{
    NRF_GPIO->PIN_CNF[pin_number] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) \
        |(GPIO_PIN_CNF_DRIVE_D0S1 << GPIO_PIN_CNF_DRIVE_Pos)    \
        |(pin_pull << GPIO_PIN_CNF_PULL_Pos)  \
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) \
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
    nrf_gpio_pin_write(pin_number, init_val);
}