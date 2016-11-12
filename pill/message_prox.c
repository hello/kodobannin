#include "message_prox.h"
#include "platform.h"
#include "util.h"
#include "prox_i2c.h"
#include "twi_master.h"
#include "message_ant.h"
#include "app_timer.h"
#include "twi_master_config.h"
#include "pstorage.h"
#include "pill_gatt.h"

static char * name = "PROX";
static const MSG_Central_t * parent;
static MSG_Base_t base;
static app_timer_id_t timer_id_prox;
static pstorage_handle_t fs;

#define PROX_POLL_INTERVAL 15000 /*in ms*/
#define MSB_24_MASK (1<<23)

static void _send_available_prox_ant();

#define CALIBRATION_GOOD_MAGIC 0x5A5A /*reserved 0*/
typedef struct {
    uint16_t gain[2];
    int16_t  offset[2];
    uint16_t reserved[4];
}prox_calibration_t;
static prox_calibration_t  cal;

static int16_t get_offset(int32_t meas){
    return (-1 * meas) / (1<<19);
}
static uint16_t get_gain(uint32_t meas){
    return 0x2000;
}
static int _get_calibration(prox_calibration_t * out){
    pstorage_handle_t block;
    APP_OK(pstorage_block_identifier_get(&fs,0,&block));
    APP_OK(pstorage_load((uint8_t*)out, &block, sizeof(prox_calibration_t), 0));
    PRINT_HEX(out, sizeof(*out));
    if(out->reserved[0] == CALIBRATION_GOOD_MAGIC){
        return 0;
    }else{
        return 1;
    }
}
static void _notify_calibration_result(uint8_t good){
    if(good){
        hlo_ble_notify(0xD00D, "Pass", 4, NULL);
        _send_available_prox_ant();
        PRINTF("Calibration Pass\r\n");
    }else{
        hlo_ble_notify(0xD00D, "Fail", 4, NULL);
        PRINTF("Calibration Fail\r\n");
    }

}
static bool _is_in_range(int32_t value){
    int16_t msb = value >> 19;
    PRINTF("MSB %d\r\n", msb);
    if( msb < 13 && msb > -13 ){
        return true;
    }
    return false;
}
static void _do_prox_calibration(void){
    int32_t cap1 = 0;
    int32_t cap4 = 0;
    PRINTF("Prox Calibration\r\n");
    read_prox(&cap1, &cap4);
    PRINTF("Got [%d, %d]\r\n", cap1, cap4);
    if(_is_in_range(cap1) && _is_in_range(cap4)){
        //don't need to recalibrate
        PRINTF("Skipping Calibration [%u, %u]\r\n", cap1, cap4);
        _notify_calibration_result(1);
        return;
    }

    cal.offset[0] = get_offset(cap1);
    cal.offset[1] = get_offset(cap4);
    set_prox_offset(cal.offset[0], cal.offset[1]);

    cal.gain[0] = get_gain(cap1);
    cal.gain[1] = get_gain(cap4);
    set_prox_gain(cal.gain[0], cal.gain[1]);

    read_prox(&cap1, &cap4);
    PRINTF("Got [%d, %d]\r\n", cap1, cap4);
    //check if values are reasonable, if so, commit
    if(_is_in_range(cap1) && _is_in_range(cap4)){
        //ok
        //commit
        cal.reserved[0] = CALIBRATION_GOOD_MAGIC;
        pstorage_handle_t block;
        APP_OK(pstorage_block_identifier_get(&fs,0,&block));
        APP_OK(pstorage_store(&block,(uint8_t*)&cal, sizeof(cal), 0));
    }else{
        //doesn't work, reset
        _notify_calibration_result(0);
    }

}
static void _on_pstorage_error(pstorage_handle_t *  p_handle,
                                  uint8_t              op_code,
                                  uint32_t             result,
                                  uint8_t *            p_data,
                                  uint32_t             data_len){

    PRINTF("res %d\r\n", result);
    if(result == 0){
        _notify_calibration_result(1);
    }else{
        _notify_calibration_result(0);
    }
}
static MSG_Status _init(void){
    uint32_t ticks = 0;
    ticks = APP_TIMER_TICKS(PROX_POLL_INTERVAL,APP_TIMER_PRESCALER);
    PRINTF("PROX %u ms\r\n", PROX_POLL_INTERVAL);
    {
        pstorage_module_param_t opts = (pstorage_module_param_t){
            .cb = _on_pstorage_error,
            .block_size = sizeof(prox_calibration_t),
            .block_count = 1,
        };
        APP_OK(pstorage_register(&opts, &fs));
    }
    {
        //load calibration values
        prox_calibration_t c;
        if(0 == _get_calibration(&c)){
            PRINTF("got calibration\r\n");
            set_prox_offset(c.offset[0], c.offset[1]);
            set_prox_gain(c.gain[0], c.gain[1]);
        } else {
            PRINTF("fail to get calibration\r\n");
            //using default value
            /*
             *set_prox_offset(0x9, 0x6);
             *set_prox_gain(0xFFFF, 0xFFFF);
             */
        }
        /*
         *parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){PROX,PROX_CALIBRATE}, NULL);
         */
    }

    if(SUCCESS ==  init_prox()){
        PRINTF("Prox Load OK\r\n");
        app_timer_start(timer_id_prox, ticks, NULL);
        return SUCCESS;
    }else{
        PRINTF("Prox Load Fail\r\n");
        return FAIL;
    }
}
static MSG_Status _destroy(void){
    return SUCCESS;
}
static MSG_Status _flush(void){
    return SUCCESS;
}
static MSG_Status _on_message(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    PRINTS("PROX CMD\r\n");
    switch(dst.submodule){
        default:
        case PROX_PING:
            break;
        case PROX_READ:
            {
                int32_t data[2] = {0};
                read_prox(&data[0], &data[1]);
                int32_t msb[2] = {0};
                msb[0] = data[0] >> 19;
                msb[1] = data[1] >> 19;
                PRINTF("Prox MSB Read [%d, %d]\r\n", msb[0], msb[1]);
                _send_available_prox_ant();
            }
            break;
        case PROX_START_CALIBRATE:
            {
                _do_prox_calibration();
            }
            break;
        case PROX_ERASE_CALIBRATE:
            APP_OK(pstorage_clear(&fs, sizeof(prox_calibration_t)));
            break;
        case PROX_READ_REPLY_BLE:
            {
                uint32_t prox[2] = {0};
                read_prox(&prox[0], &prox[1]);
                hlo_ble_notify(0xD00D, (uint8_t*)prox, sizeof(prox), NULL);
            }
            break;
    }
    return SUCCESS;
}
static void _send_available_prox_ant(){
    pill_proxdata_t prox = (pill_proxdata_t){
        .cap = {0},
        .reserved = 0,
    };
    read_prox(&prox.cap[0], &prox.cap[1]);
    PRINTF("P1: %d, P4: %d \r\n", prox.cap[0], prox.cap[1]);
    {
        MSG_Data_t * data = AllocateEncryptedAntPayload(ANT_PILL_PROX_ENCRYPTED, &prox, sizeof(prox));
        if(data){
            parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){ANT,1}, data);
            /*
             *parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){UART,MSG_UART_HEX}, data);
             */
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
    {
        MSG_Data_t * data = AllocateAntPayload(ANT_PILL_PROX_PLAINTEXT, &prox, sizeof(prox));
        if(data){
            parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){ANT,1}, data);
            /*
             *parent->dispatch((MSG_Address_t){PROX,1}, (MSG_Address_t){UART,MSG_UART_HEX}, data);
             */
            MSG_Base_ReleaseDataAtomic(data);
        }
    }
}
static void _prox_timer_handler(void * ctx) {
    _send_available_prox_ant();
}
MSG_Base_t * MSG_Prox_Init(const MSG_Central_t * central){
	parent = central;
	base.init = _init;
	base.destroy = _destroy;
	base.flush = _flush;
	base.send = _on_message;
	base.type = PROX;
	base.typestr = name;

    twi_master_init();
    APP_OK(app_timer_create(&timer_id_prox,APP_TIMER_MODE_REPEATED, _prox_timer_handler));
    return &base;
}
