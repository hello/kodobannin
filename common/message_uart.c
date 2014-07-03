#include "nrf.h"
#include "message_uart.h"
#include "util.h"

static struct{
    MSG_Base_t base;
    MSG_Base_t * parent;
    bool initialized;
}self;
void UART0_IRQHandler(void)
{
    uint8_t m_rx_byte;
    if (NRF_UART0->EVENTS_RXDRDY != 0)
    {

        // Clear UART RX event flag
        NRF_UART0->EVENTS_RXDRDY  = 0;
        m_rx_byte                 = (uint8_t)NRF_UART0->RXD;    
        //NRF_UART0->TXD = (uint8_t)m_rx_byte;
        
    }
    /*
    // TX not ready yet 
    // Handle transmission.
    if (NRF_UART0->EVENTS_TXDRDY != 0)
    {
        // Clear UART TX event flag.
        NRF_UART0->EVENTS_TXDRDY = 0;
    }
    */
    // Handle errors.
    if (NRF_UART0->EVENTS_ERROR != 0)
    {
        uint32_t       error_source;

        // Clear UART ERROR event flag.
        NRF_UART0->EVENTS_ERROR = 0;
        
        // Clear error source.
        error_source        = NRF_UART0->ERRORSRC;
        NRF_UART0->ERRORSRC = error_source;

    }
}

static MSG_Status
_init(void){
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    return SUCCESS;
}
static MSG_Status
_flush(void){
    return SUCCESS;
}
static MSG_Status
_send(MSG_Data_t * data){
    return SUCCESS;
}
static void
_uart_event_handler(app_uart_evt_t * evt){
    PRINTS("Event");
}

MSG_Base_t * MSG_Uart_Init(const app_uart_comm_params_t * params, MSG_Base_t * central){
    uint32_t err;
    self.parent = central;
    NRF_UART0->INTENCLR = 0xffffffffUL;
    NRF_UART0->INTENSET = (UART_INTENSET_RXDRDY_Set << UART_INTENSET_RXDRDY_Pos) |
            //disabed tx interrupt for now for cross compatibility of simple_uart
            //will be reimplemented later
            //            (UART_INTENSET_TXDRDY_Set << UART_INTENSET_TXDRDY_Pos) |
                          (UART_INTENSET_ERROR_Set << UART_INTENSET_ERROR_Pos);

    NVIC_ClearPendingIRQ(UART0_IRQn);
    NVIC_SetPriority(UART0_IRQn, APP_IRQ_PRIORITY_LOW);
    NVIC_EnableIRQ(UART0_IRQn);
    return &self.base;
}
