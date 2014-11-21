#include "factory_provision.h"
#include "device_info.h"
#include "pstorage.h"

static pstorage_handle_t        m_info_handle;
static void key_callback_handler(pstorage_handle_t * handle, uint8_t op_code, uint32_t result, uint8_t * p_data, uint32_t data_len){
    APP_ERROR_CHECK(result);
}



uint32_t factory_provision_start(void){
    pstorage_module_param_t info_params;
    info_params.cb          = key_callback_handler;
    info_params.block_size  = 0x400;
    info_params.block_count = 1;
    m_info_handle.block_id = DEVICE_INFO_ADDRESS;

    err_code = pstorage_init();
    if (err_code != NRF_SUCCESS)    
    {
        return err_code;
    }

    err_code = pstorage_register(&info_params, &m_info_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
}

