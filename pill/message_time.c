#include <stddef.h>
#include <nrf_soc.h>
#include <string.h>
#include "app_timer.h"
#include "message_led.h"
#include "message_time.h"
#include "message_imu.h"
#include "util.h"
#include "platform.h"
#include "app_info.h"
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

#ifdef PLATFORM_HAS_REED
#include "nrf_gpiote.h"
#include "nrf_gpio.h"
#include <app_gpiote.h>
#endif

static struct{
    MSG_Base_t base;
    bool initialized;
    const MSG_Central_t * central;
    app_timer_id_t timer_id_1sec;
    app_timer_id_t timer_id_1min;
    uint32_t uptime;
    uint32_t minutes;
    uint32_t last_wakeup;
    uint32_t onesec_runtime;
    uint8_t reed_states;
    uint8_t in_ship_state;
}self;

static char * name = "TIME";


static app_gpiote_user_id_t _gpiote_user;


static MSG_Status
_init(void){
    APP_OK(app_gpiote_user_enable(_gpiote_user));
    return SUCCESS;
}

static MSG_Status
_destroy(void){
    APP_OK(app_gpiote_user_disable(_gpiote_user));
    return SUCCESS;
}

static MSG_Status
_flush(void){
    return SUCCESS;
}

#ifdef ANT_STACK_SUPPORT_REQD
static void _send_available_data_ant(){
    MotionPayload_t motion[1];
    if(TF_GetCondensed(motion)){
        MSG_Data_t * data = AllocateEncryptedAntPayload(ANT_PILL_DATA_ENCRYPTED, motion, sizeof(motion));
        PRINTF("data len %d, pwr %d\r\n", data->len, motion->max);
        if(data){
            self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){ANT,1}, data);
            self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){UART,MSG_UART_HEX}, data);
            MSG_Base_ReleaseDataAtomic(data);
        }
    }else{
        PRINTS("No Motion Recorded\r\n");
    }
}

static void _send_heartbeat_data_ant(){
    pill_heartbeat_t heartbeat = {0};
    heartbeat.battery_level = battery_get_percent_cached();
    heartbeat.uptime_sec = self.uptime;
    heartbeat.firmware_build = FIRMWARE_VERSION_8BIT;
    MSG_Data_t* data_page = AllocateAntPayload(ANT_PILL_HEARTBEAT,&heartbeat , sizeof(pill_heartbeat_t));
    if(data_page){
        PRINTF("HB battery %d uptime %d fw %d\r\n", heartbeat.battery_level, heartbeat.uptime_sec, heartbeat.firmware_build);
        self.central->dispatch((MSG_Address_t){TIME,1}, (MSG_Address_t){ANT,1}, data_page);
        MSG_Base_ReleaseDataAtomic(data_page);
    }
}

#endif
static void _update_uptime() {
    uint32_t current_time = 0;
    uint32_t time_diff = 0;
    app_timer_cnt_get(&current_time);
    
    app_timer_cnt_diff_compute(current_time, self.last_wakeup, &time_diff);
    time_diff /= APP_TIMER_TICKS( 1000, APP_TIMER_PRESCALER );
    self.uptime += time_diff;
    PRINTF("uptime now %d\r\n", self.uptime );
    self.last_wakeup = current_time;
}

void _send_data_test(void){
    PRINTS("Sending\r\n");
    _send_available_data_ant();
    _send_heartbeat_data_ant();
}
static void _1min_timer_handler(void * ctx) {
    _update_uptime();
    self.minutes += 1;
    
    fix_imu_interrupt(); // look for imu int stuck low

#ifdef ANT_ENABLE
    _send_available_data_ant();
    if(self.minutes % HEARTBEAT_INTERVAL_MIN == 0) { // update percent battery capacity
        if( !self.in_ship_state ){
            _send_heartbeat_data_ant();
        }
        battery_update_level(); // Vmcu(), Vbat(ref), Vrgb(offset), Vbat(rel)
    }
    // else { // since no ant queue, will be first come, only served
    if(self.minutes % BATT_MEASURE_INTERVAL_MIN == 0) { // update minimum battery measurement
        battery_update_droop(); // Vmcu(), Vbat(ref), Vrgb(offset), Vbat(min)
    }
#endif
    TF_TickOneMinute();
    top_of_meas_minute();
}

#define POWER_STATE_MASK 0x7
//this one needs to be the max of all the requirements in the 1sec timer...
#define MAX_1SEC_TIMER_RUNTIME  10

extern int imu_self_test();
static void _1sec_timer_handler(void * ctx){
    PRINTS("*");
    _update_uptime();

    uint8_t current_reed_state = 0;
    self.onesec_runtime += 1;

#ifdef PLATFORM_HAS_VLED
#include "led_booster_timer.h"
    if(led_booster_is_free()) {
        current_reed_state = (uint8_t)led_check_reed_switch();
    }
#else
    uint32_t gpio_pin_state;
    if(NRF_SUCCESS == app_gpiote_pins_state_get(_gpiote_user, &gpio_pin_state)){
        //PRINTS("RAW ");
        //PRINT_HEX(&gpio_pin_state, sizeof(gpio_pin_state));
        current_reed_state = (gpio_pin_state & (1<<LED_REED_ENABLE))!=0;
    }
    //PRINTS("REED ");
    PRINT_HEX(&current_reed_state, 1);
    //PRINTS("\r\n");
#endif
    self.reed_states = ((self.reed_states << 1) + (current_reed_state & 0x1)) & POWER_STATE_MASK;
    PRINT_HEX(&self.reed_states, 1);
    PRINTS("\r\n");

    if(self.reed_states == POWER_STATE_MASK && self.in_ship_state == 0){
        PRINTS("Going into Ship Mode");
        _send_heartbeat_data_ant(); // form ant packet
        battery_update_level(); // make next battery reading
        self.in_ship_state = 1;
        self.central->unloadmod(MSG_IMU_GetBase());
        sd_ble_gap_adv_stop();
        self.central->dispatch((MSG_Address_t){TIME,0}, (MSG_Address_t){LED,LED_PLAY_ENTER_FACTORY_MODE},NULL);
        self.central->dispatch( ADDR(TIME, 0), ADDR(TIME, MSG_TIME_STOP_PERIODIC), NULL);
        APP_OK(app_gpiote_user_enable(_gpiote_user));
    }else if(self.reed_states == 0x00 && self.in_ship_state == 1){
        PRINTS("Going into User Mode");
        _send_heartbeat_data_ant(); // form ant packet
        battery_update_level(); // make next battery reading
        self.in_ship_state = 0;
        self.central->loadmod(MSG_IMU_GetBase());
        hble_advertising_start();
        self.central->dispatch( ADDR(TIME, 0), ADDR(TIME, MSG_TIME_SET_START_1MIN), NULL);
        APP_OK(app_gpiote_user_enable(_gpiote_user));
        ShakeDetectFactoryTest();
    } else if(self.reed_states && self.reed_states != POWER_STATE_MASK){
        battery_update_droop();
    }
    
    if( self.onesec_runtime > MAX_1SEC_TIMER_RUNTIME ) {
        app_timer_stop(self.timer_id_1sec);
        self.onesec_runtime = 0;
        PRINTS("\nStopping 1sec\r\n");
        APP_OK(app_gpiote_user_enable(_gpiote_user));
    }
}

static MSG_Status _send(MSG_Address_t src, MSG_Address_t dst, MSG_Data_t * data){
    switch(dst.submodule){
        default:
        case MSG_TIME_PING:
            break;
        case MSG_TIME_STOP_PERIODIC:
            PRINTS("STOP_HEARTBEAT\r\n");
            app_timer_stop(self.timer_id_1min);
            app_timer_stop(self.timer_id_1sec);
            break;
        case MSG_TIME_SET_START_1SEC:
            {
                uint32_t ticks;
                ticks = APP_TIMER_TICKS(1000,APP_TIMER_PRESCALER);
                app_timer_stop(self.timer_id_1sec);
                app_timer_start(self.timer_id_1sec, ticks, NULL);
                self.onesec_runtime = 0;
            }
            break;
        case MSG_TIME_SET_START_1MIN:
        {
            uint32_t ticks;
            PRINTS("PERIODIC 1 MIN\r\n");
            ticks = APP_TIMER_TICKS(60000,APP_TIMER_PRESCALER);
            app_timer_stop(self.timer_id_1min);
            app_timer_start(self.timer_id_1min, ticks, NULL);
        }
            break;
    }
    return SUCCESS;
}

static void _reed_gpiote_process(uint32_t event_pins_low_to_high, uint32_t event_pins_high_to_low)
{
    APP_OK(app_gpiote_user_disable(_gpiote_user));
    PRINTS("REED INT\r\n");
    self.central->dispatch( ADDR(TIME, 0), ADDR(TIME, MSG_TIME_SET_START_1SEC), NULL);
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
        self.minutes = 0;
        self.last_wakeup = 0;
        self.onesec_runtime = 0;
      
        if(app_timer_create(&self.timer_id_1sec,APP_TIMER_MODE_REPEATED,_1sec_timer_handler) == NRF_SUCCESS
           && app_timer_create(&self.timer_id_1min,APP_TIMER_MODE_REPEATED,_1min_timer_handler) == NRF_SUCCESS
          ){
            self.initialized = 1;
            TF_Initialize();

#if defined(PLATFORM_HAS_REED) && !defined(PLATFORM_HAS_VLED)
            nrf_gpio_cfg_input(LED_REED_ENABLE, NRF_GPIO_PIN_NOPULL);
            APP_OK(app_gpiote_user_register(&_gpiote_user, 1 << LED_REED_ENABLE, 1 << LED_REED_ENABLE, _reed_gpiote_process));
            APP_OK(app_gpiote_user_disable(_gpiote_user));
            
#elif defined(PLATFORM_HAS_REED)
            APP_OK(1);
#endif

        }
    }
    return &self.base;
}

