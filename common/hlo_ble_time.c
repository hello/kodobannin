// vi:noet:sw=4 ts=4

#include "hlo_ble_time.h"

#ifdef DEBUG
void hlo_ble_time_printf(struct hlo_ble_time* ble_time)
{
    printf("%d/%d/%d %d:%d:%d", ble_time->month, ble_time->day, ble_time->year, ble_time->hours, ble_time->minutes, ble_time->seconds);
}
#endif
