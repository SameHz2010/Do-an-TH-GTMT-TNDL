#ifndef _CUSTOM_ADV_H_
#define _CUSTOM_ADV_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sl_bt_api.h"
#include "app_assert.h"
#include "app_log.h"

#define NAME_MAX_LENGTH 10

  // Constants
#define FLAG_VALUE  0x06
#define COMPANY_ID  0x02FF

  // Cấu trúc gói tin (Sử dụng __attribute__((packed)) để không bị lệch byte)
  typedef struct __attribute__((packed))
  {
    // --- FLAGS ---
    uint8_t len_flags;
    uint8_t type_flags;
    uint8_t val_flags;

    // --- MANUFACTURER DATA ---
    uint8_t len_manuf;
    uint8_t type_manuf;
    uint8_t company_LO;
    uint8_t company_HI;

    // --- DATA CHÍNH (NODE ID thay cho Student ID) ---
    uint8_t node_id_0; // Byte cao
    uint8_t node_id_1;
    uint8_t node_id_2;
    uint8_t node_id_3; // Byte thấp (Chứa 1, 2, 3)

    // --- CẢM BIẾN ---
    uint8_t temp_LO;
    uint8_t temp_HI;
    uint8_t hum_LO;
    uint8_t hum_HI;

    // --- NAME ---
    uint8_t len_name;
    uint8_t type_name;
    char name[NAME_MAX_LENGTH];

    // --- SYSTEM USE ---
    uint8_t data_size; // Biến này quan trọng, lỗi của bạn báo thiếu biến này
  } CustomAdv_t;

  // Hàm chức năng
  void fill_adv_packet(CustomAdv_t *pData, uint8_t flags, uint16_t companyID,
                       uint32_t node_id, float temp, float hum, char *name);

  void start_adv(CustomAdv_t *pData, uint8_t advertising_set_handle);

  void update_adv_data(CustomAdv_t *pData, uint8_t advertising_set_handle,
                       float temp, float hum);

#ifdef __cplusplus
}
#endif

#endif // _CUSTOM_ADV_H_
