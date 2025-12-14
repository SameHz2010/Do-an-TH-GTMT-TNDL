#include <string.h>
#include "custom_adv.h"

// Helper: Float to Int16 (25.5 -> 2550)
static int16_t convert_float_to_int16(float value) {
  return (int16_t)(value * 100);
}

void fill_adv_packet(CustomAdv_t *pData, uint8_t flags, uint16_t companyID,
                     uint32_t student_id, float temp, float hum, char *name)
{
  // Reset bộ nhớ để tránh rác
  memset(pData, 0, sizeof(CustomAdv_t));

  // 1. Flags
  pData->len_flags = 0x02;
  pData->type_flags = 0x01;
  pData->val_flags = flags;

  // 2. Manufacturer Data Header
  // Length = Type(1) + Company(2) + Student(4) + Temp(2) + Hum(2) = 11 bytes
  pData->len_manuf = 11;
  pData->type_manuf = 0xFF; // Manufacturer Specific Data Type

  // 3. Company ID
  pData->company_LO = companyID & 0xFF;
  pData->company_HI = (companyID >> 8) & 0xFF;

  // 4. Student ID
  pData->student_id_3 = student_id & 0xFF;
  pData->student_id_2 = (student_id >> 8) & 0xFF;
  pData->student_id_1 = (student_id >> 16) & 0xFF;
  pData->student_id_0 = (student_id >> 24) & 0xFF;

  // 5. Temp & Hum
  int16_t i_temp = convert_float_to_int16(temp);
  int16_t i_hum = convert_float_to_int16(hum);

  pData->temp_LO = i_temp & 0xFF;
  pData->temp_HI = (i_temp >> 8) & 0xFF;
  pData->hum_LO = i_hum & 0xFF;
  pData->hum_HI = (i_hum >> 8) & 0xFF;

  // 6. Name
  int n = strlen(name);
  if (n > NAME_MAX_LENGTH) n = NAME_MAX_LENGTH; // Cắt tên nếu quá dài

  pData->type_name = 0x09; // Complete Local Name
  pData->len_name = 1 + n; // Type + Char length
  strncpy(pData->name, name, n);

  // 7. Calculate Total Size (Used for API call)
  // Size = Flags(3) + Manuf(1+1+11) + Name(1+1+n)
  pData->data_size = 3 + (2 + pData->len_manuf) + (1 + pData->len_name);

  app_log("ADV Init: Size=%d, T=%.2f, H=%.2f\r\n", pData->data_size, temp, hum);
}

void start_adv(CustomAdv_t *pData, uint8_t advertising_set_handle)
{
  sl_status_t sc;

  // Cài đặt dữ liệu quảng cáo
  sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle,
                                        0, // 0 = Advertising packets
                                        pData->data_size,
                                        (const uint8_t *)pData);
  if (sc != SL_STATUS_OK) {
      app_log("ERR: Set ADV data failed 0x%04x\r\n", sc);
      return;
  }

  // Bắt đầu quảng cáo
  sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                     sl_bt_legacy_advertiser_connectable);

  if (sc == SL_STATUS_OK) {
      app_log("BLE Advertising Started!\r\n");
  } else {
      app_log("ERR: Start ADV failed 0x%04x\r\n", sc);
  }
}

void update_adv_data(CustomAdv_t *pData, uint8_t advertising_set_handle,
                     float temp, float hum)
{
  sl_status_t sc;

  // Chuyển đổi dữ liệu mới
  int16_t i_temp = convert_float_to_int16(temp);
  int16_t i_hum = convert_float_to_int16(hum);

  // Cập nhật vào Struct
  pData->temp_LO = i_temp & 0xFF;
  pData->temp_HI = (i_temp >> 8) & 0xFF;
  pData->hum_LO = i_hum & 0xFF;
  pData->hum_HI = (i_hum >> 8) & 0xFF;

  sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle,
                                        0,
                                        pData->data_size,
                                        (const uint8_t *)pData);

  // --- SỬA ĐOẠN NÀY ---
  if (sc == SL_STATUS_OK) {
      // Tách số để in ra log (vì %f không hoạt động)
      int t_int = (int)temp;
      int t_frac = (int)((temp - t_int) * 100);
      int h_int = (int)hum;
      int h_frac = (int)((hum - h_int) * 100);

      app_log("BLE Updated: T=%d.%02d, H=%d.%02d\r\n", t_int, t_frac, h_int, h_frac);
  } else {
      app_log("ERR: Update ADV failed 0x%04x\r\n", sc);
  }
  // --------------------
}
