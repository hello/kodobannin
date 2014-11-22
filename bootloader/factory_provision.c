#include "factory_provision.h"
#include "device_info.h"
#include "pstorage.h"
#include "util.h"
#include "nrf_error.h"
#include "pstorage_platform.h"
#include "nrf_sdm.h"

static pstorage_handle_t        __attribute__((aligned(4))) m_info_handle;
static device_info_t __attribute__((aligned(4))) test_info;
static volatile uint32_t status;
volatile uint32_t code;

static void
_pstorage_cb(pstorage_handle_t * handle, uint8_t op_code, uint32_t result, uint8_t * p_data, uint32_t data_len){
    APP_ERROR_CHECK(result);
    switch(op_code){
        case PSTORAGE_CLEAR_OP_CODE:
            break;
        case PSTORAGE_STORE_OP_CODE:
            status = 0;
            break;
        default:
            /*SIMPRINTS("Unknown Opcode");*/
            break;
    }
}


static void wait_for_events(void){
    for (;;)
    {
        //handle the event that is generated before this
        app_sched_execute();
        if(!status){
            return;
        }
        // Wait in low power state for any events.
        uint32_t err_code = sd_app_evt_wait();
        APP_ERROR_CHECK(err_code);

    }
}
bool factory_needs_provisioning(void){
    return !validate_device_fast(DEVICE_INFO_ADDRESS);
}
void _gen_key(void * data, uint16_t size){
    uint32_t err_code;
    generate_new_device(&test_info);

    pstorage_raw_clear(&m_info_handle, sizeof(test_info));

    pstorage_raw_store(&m_info_handle, 
            (uint8_t *)&test_info, 
            sizeof(test_info),
            0);
}

uint32_t factory_provision_start(void){
    uint32_t err_code = NRF_SUCCESS;
    pstorage_module_param_t info_params;
    status = 1;

    /*
     *SIMPRINT_HEX(&test_info, sizeof(test_info));
     */

    info_params.cb          = _pstorage_cb;
    info_params.block_size  = sizeof(device_info_t);
    info_params.block_count = 1;

    m_info_handle.block_id = DEVICE_INFO_ADDRESS;

    err_code = pstorage_init();
    if (err_code != NRF_SUCCESS)    
    {
        return err_code;
    }
    err_code = pstorage_raw_register(&info_params, &m_info_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    app_sched_event_put(NULL, 0, _gen_key);
    wait_for_events();
    return NRF_SUCCESS;
}

uint8_t * decrypt_key(void){
    decrypt_device(DEVICE_INFO_ADDRESS, &test_info);
    return test_info.factory_info.device_aes;

}
