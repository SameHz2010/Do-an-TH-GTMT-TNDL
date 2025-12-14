#ifndef DHT20_H
#define DHT20_H

#include "sl_status.h"
#include <stdbool.h>

#define DHT20_ADDR 0x38

// Các hàm chính
sl_status_t dht20_init(void);
sl_status_t dht20_read(float *temperature, float *humidity);
sl_status_t dht20_soft_reset(void);

// Hàm tiện ích
void i2c_scan(void);
bool dht20_is_connected(void);

#endif // DHT20_H
