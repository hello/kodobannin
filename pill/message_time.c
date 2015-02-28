#include <stddef.h>
#include <nrf_soc.h>
#include <string.h>
#include "app_timer.h"
#include "message_led.h"
#include "message_time.h"
#include "util.h"
#include "platform.h"
#include "app.h"
#include "shake_detect.h"
#include "message_led.h"
#include "timedfifo.h"
#include "led.h"
#include <ble.h>
#include "hble.h"
#include "battery.h"

#include "ble.h"
#include "hble.h"

#ifdef ANT_STACK_SUPPORT_REQD
#include "message_ant.h"
#endif

typedef struct{
    uint64_t nonce;
    MotionPayload_t payload[TF_CONDENSED_BUFFER_SIZE];
    uint16_t magic_bytes; //this must be here at the end of the struct,  or Jackson will kill you
}__attribute__((packed)) MSG_ANT_EncryptedMotionData_t;

static struct{
    MSG_Base_t base;
    bool initialized;
    struct hlo_ble_time ble_time;  // Keep it here for debug, can see if pill crashed or not.
    MSG_Central_t * central;
    app_timer_id_t timer_id;
    MSG_Data_t * user_cb;
    uint32_t uptime;
    uint8_t reed_states;
    uint8_t in_ship_state;
}self;

static char * name = "TIME";

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

#ifdef ANT_STACK_SUPPORT_REQD
#define MOTION_DATA_MAGIC 0x5A5A
static void _send_available_data_ant(){

    //allocate memory
    MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANT_PillData_t) +  // ANT packet headers
        sizeof(MSG_ANT_EncryptedMotionData_t));

    //if allocation worked
    if(data_page) {
        //ant data comes from the data page (allocated above, and freed at the end of this function)
        MSG_ANT_PillData_t* ant_data =(MSG_ANT_PillData_t*) &data_page->buf;

        //motion_data is a pointer to the blob of data that antdata->payload points to
        //the goal is to fill out the motion_data pointer
        MSG_ANT_EncryptedMotionData_t* motion_data = (MSG_ANT_EncryptedMotionData_t *)ant_data->payload;

        //zero out my blob
        memset(&data_page->buf, 0, data_page->len);

        ant_data->version = ANT_PROTOCOL_VER;
        ant_data->type = ANT_PILL_DATA_ENCRYPTED;
        ant_data->UUID = GET_UUID_64();
        ant_data->payload_len = sizeof(MSG_ANT_EncryptedMotionData_t);

        //fill out the motion data payload
        if(TF_GetCondensed(motion_data->payload, TF_CONDENSED_BUFFER_SIZE))
        {
            uint8_t pool_size = 0;

            //magic is pre-defined, assign it.
            motion_data->magic_bytes = MOTION_DATA_MAGIC;

            //do the nonce stuff (encrypting it all)  
            if(NRF_SUCCESS == sd_rand_application_bytes_available_get(&pool_size)){
                uint8_t nonce[8] = {0};

                //this payload struct is packed (packed attributed in GCC)
                uint8_t * after_the_nonce = (uint8_t *)&motion_data->payload;

                sd_rand_application_vector_get(nonce, (pool_size > sizeof(nonce) ? sizeof(nonce) : pool_size));

                aes128_ctr_encrypt_inplace(after_the_nonce, sizeof(MotionPayload_t) + sizeof(motion_data->magic_bytes), get_aes128_key(), nonce);
                
                memcpy((uint8_t*)&motion_data->nonce, nonce, sizeof(nonce));

                //this sends out the data
                self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){ANT,1}, data_page);

                /*
                 * //Uncomment me to debug
                 * 
                 *self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){UART,1}, data_page);

                 *MSG_Data_t * dupe = MSG_Base_Dupe(data_page);
                 *if(dupe){
                 *    ant_data = (MSG_ANT_PillData_t *)&dupe->buf;
                 *    motion_data = (MSG_ANT_EncryptedMotionData_t*)ant_data->payload;
                 *    uint8_t * after_the_nonce = (uint8_t *)&motion_data->payload;
                 *    aes128_ctr_encrypt_inplace(after_the_nonce, sizeof(MotionPayload_t) + sizeof(motion_data->magic_bytes), get_aes128_key(), nonce);
                 *    self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){UART,1}, dupe);
                 *    MSG_Base_ReleaseDataAtomic(dupe);
                 *}
                 */
            }else{
                //pools closed
            }
        }else{
            // Morpheus should fake data if there is nothing received for that minute.
        }
        MSG_Base_ReleaseDataAtomic(data_page);
    }
}

static void _send_heartbeat_data_ant(){
    // TODO: Jackson please do review & test this.
    // I packed all the struct and I am not sure it will work as expected.
    MSG_Data_t* data_page = MSG_Base_AllocateDataAtomic(sizeof(MSG_ANT_PillData_t) + sizeof(pill_heartbeat_t));
    if(data_page){
        pill_heartbeat_t heartbeat = {0};

        heartbeat.battery_level = battery_get_percent_cached();
        heartbeat.uptime_sec = self.uptime;
        heartbeat.firmware_version = FIRMWARE_VERSION_8BIT;
        
        memset(&data_page->buf, 0, data_page->len);
        MSG_ANT_PillData_t* ant_data = &data_page->buf;
        ant_data->version = ANT_PROTOCOL_VER;
        ant_data->type = ANT_PILL_HEARTBEAT;
        ant_data->UUID = GET_UUID_64();
        memcpy(ant_data->payload, &heartbeat, sizeof(heartbeat));
        self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){ANT,1}, data_page);
        MSG_Base_ReleaseDataAtomic(data_page);
    }
}

#endif

#define POWER_STATE_MASK 0x7

static void _timer_handler(void * ctx){
    uint8_t value; // carry;
    uint8_t current_reed_state = 0;
    self.ble_time.monotonic_time += 1000;  // Just keep it for current data collection task.
    self.uptime += 1;

    value = fix_imu_interrupt(); // look for imu int stuck low
    if (value) // look for imu int stuck low
    {
        battery_set_percent_cached(BATTERY_EXCEPTION_BASE + value); // notify missing interrupt(s)
    }

    TF_TickOneSecond(self.ble_time.monotonic_time);
#ifdef ANT_ENABLE
    if(get_tick() == 0)
    {
        _send_available_data_ant();
    }
 // else { // since no ant queue, will be first come, only served
    if(self.uptime % HEARTBEAT_INTERVAL_SEC == 0) { // update percent battery capacity
        if( !self.in_ship_state ){
            _send_heartbeat_data_ant();
        }
        battery_update_level(); // Vmcu(), Vbat(ref), Vrgb(offset), Vbat(rel)
    }
 // else { // since no ant queue, will be first come, only served
    if(self.uptime % BATT_MEASURE_INTERVAL_SEC == 0) { // update minimum battery measurement
        battery_update_droop(); // Vmcu(), Vbat(ref), Vrgb(offset), Vbat(min)
    }
#endif
    
    ShakeDetectDecWindow();
    
    if(self.user_cb){
        MSG_TimeCB_t * cb = &((MSG_TimeCommand_t*)(self.user_cb->buf))->param.wakeup_cb;
        if(cb->cb(&self.ble_time,1000,cb->ctx)){
            MSG_Base_ReleaseDataAtomic(self.user_cb);
            self.user_cb = NULL;
        }
    }
#ifdef PLATFORM_HAS_VLED
    if(led_booster_is_free()) {
        current_reed_state = (uint8_t)led_check_reed_switch();
    }
#else
    current_reed_state = (uint8_t)led_check_reed_switch();
#endif
    self.reed_states = ((self.reed_states << 1) + (current_reed_state & 0x1)) & POWER_STATE_MASK;
    PRINT_HEX(&self.reed_states, 1);
    PRINTS("\r");

    if(self.reed_states == POWER_STATE_MASK && self.in_ship_state == 0){
        PRINTS("Going into Ship Mode");
        _send_heartbeat_data_ant(); // form ant packet
        battery_update_level(); // make next battery reading
        self.in_ship_state = 1;
        self.central->unloadmod(MSG_IMU_GetBase());
        sd_ble_gap_adv_stop();
        self.central->dispatch((MSG_Address_t){TIME,0}, (MSG_Address_t){LED,LED_PLAY_ENTER_FACTORY_MODE},NULL);
    }else if(self.reed_states == 0x00 && self.in_ship_state == 1){
        PRINTS("Going into User Mode");
        _send_heartbeat_data_ant(); // form ant packet
        battery_update_level(); // make next battery reading
        self.in_ship_state = 0;
        self.central->loadmod(MSG_IMU_GetBase());
        hble_advertising_start();
    } else if(self.reed_states && self.reed_states != POWER_STATE_MASK){
        battery_update_droop();
    }
}

static MSG_Status _send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    if(data){
        uint32_t ticks;
        MSG_Base_AcquireDataAtomic(data);
        MSG_TimeCommand_t * tmp = data->buf;
        switch(tmp->cmd){
            default:
            case TIME_PING:
                PRINTS("TIMEPONG");
                break;
            case TIME_SYNC:
                PRINTS("SETTIME = ");
                self.ble_time.monotonic_time = tmp->param.ble_time.monotonic_time;
                //TF_Initialize(&tmp->param.ble_time);
                PRINT_HEX(&tmp->param.ble_time, sizeof(tmp->param.ble_time));
                break;
            case TIME_STOP_PERIODIC:
                PRINTS("STOP_HEARTBEAT");
                app_timer_stop(self.timer_id);
                break;
            case TIME_SET_1S_RESOLUTION:
                PRINTS("PERIODIC 1S");
                ticks = APP_TIMER_TICKS(1000,APP_TIMER_PRESCALER);
                app_timer_stop(self.timer_id);
                app_timer_start(self.timer_id, ticks, NULL);
                break;
            case TIME_SET_5S_RESOLUTION:
                PRINTS("PERIODIC 5S");
                ticks = APP_TIMER_TICKS(5000,APP_TIMER_PRESCALER);
                app_timer_stop(self.timer_id);
                app_timer_start(self.timer_id, ticks, NULL);
                break;
            case TIME_SET_WAKEUP_CB:
                MSG_Base_AcquireDataAtomic(data);
                MSG_Base_ReleaseDataAtomic(self.user_cb);
                self.user_cb = data;
        }

        MSG_Base_ReleaseDataAtomic(data);
    }
    return SUCCESS;
}

MSG_Base_t * MSG_Time_Init(const MSG_Central_t * central){
    if(!self.initialized){
        self.base.init = _init;
        self.base.destroy = _destroy;
        self.base.flush = _flush;
        self.base.send = _send;
        self.base.type = TIME;
        self.base.typestr = name;
        self.central = central;
        self.uptime = 0;

        memset(&self.ble_time, 0, sizeof(self.ble_time));
        
        if(app_timer_create(&self.timer_id,APP_TIMER_MODE_REPEATED,_timer_handler) == NRF_SUCCESS){
            self.initialized = 1;
            TF_Initialize(&self.ble_time);
        }
    }
    return &self.base;
}
MSG_Status MSG_Time_GetTime(struct hlo_ble_time * out_time){
    if(out_time){
        *out_time = self.ble_time;
        return SUCCESS;
    }else{
        return FAIL;
    }
}
MSG_Status MSG_Time_GetMonotonicTime(uint64_t * out_time){
    if(out_time){
        *out_time = self.ble_time.monotonic_time;
        return SUCCESS;
    }else{
        return FAIL;
    }
}

uint64_t* get_time()
{
    return &self.ble_time.monotonic_time;
}
