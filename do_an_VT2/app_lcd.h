#ifndef APP_LCD_H
#define APP_LCD_H

#include <stdint.h> // Để dùng uint32_t

// Khai báo hàm khởi tạo màn hình
void memlcd_app_init(void);

// --- SỬA DÒNG NÀY (Thêm tham số thứ 3: interval_ms) ---
void memlcd_update_sensor(float temp, float hum, uint32_t interval_ms);

#endif // APP_LCD_H
