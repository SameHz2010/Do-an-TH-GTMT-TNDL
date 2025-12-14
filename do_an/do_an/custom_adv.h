#ifndef _CUSTOM_ADV_H_
#define _CUSTOM_ADV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sl_bt_api.h"
#include "app_assert.h"
#include "app_log.h"

#define NAME_MAX_LENGTH 14

// Define constants
#define FLAG_VALUE  0x06
#define COMPANY_ID  0x02FF // ID giả lập, hoặc dùng ID thật nếu có

// Quan trọng: __attribute__((packed)) giúp dữ liệu không bị chèn byte trống (padding)
typedef struct __attribute__((packed))
{
    // --- FLAGS (3 bytes) ---
    uint8_t len_flags;
    uint8_t type_flags;
    uint8_t val_flags;

    // --- MANUFACTURER DATA HEADER (2 bytes) ---
    uint8_t len_manuf;
    uint8_t type_manuf;

    // --- COMPANY ID (2 bytes) ---
    uint8_t company_LO;
    uint8_t company_HI;

    // --- CUSTOM DATA (8 bytes) ---
    // Student ID (4 bytes)
    uint8_t student_id_0;
    uint8_t student_id_1;
    uint8_t student_id_2;
    uint8_t student_id_3;

    // Temp & Hum (4 bytes - int16 format)
    uint8_t temp_LO;
    uint8_t temp_HI;
    uint8_t hum_LO;
    uint8_t hum_HI;

    // --- LOCAL NAME ---
    uint8_t len_name;
    uint8_t type_name;
    char name[NAME_MAX_LENGTH]; // Tên thiết bị

    // --- SYSTEM USE (Not sent OTA) ---
    uint8_t data_size;  // Kích thước thực tế để hàm API sử dụng
} CustomAdv_t;

void fill_adv_packet(CustomAdv_t *pData, uint8_t flags, uint16_t companyID,
                     uint32_t student_id, float temp, float hum, char *name);

void start_adv(CustomAdv_t *pData, uint8_t advertising_set_handle);

void update_adv_data(CustomAdv_t *pData, uint8_t advertising_set_handle,
                     float temp, float hum);

#ifdef __cplusplus
}
#endif

#endif // _CUSTOM_ADV_H_
