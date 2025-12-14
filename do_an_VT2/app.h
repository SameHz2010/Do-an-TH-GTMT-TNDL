#ifndef APP_H
#define APP_H

#include <stdint.h> // Để dùng uint32_t

void app_init(void);
void app_process_action(void);
void memlcd_update_sensor(float temp, float hum, uint32_t interval_ms);

#endif // APP_H
