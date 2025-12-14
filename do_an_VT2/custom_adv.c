#include <string.h>
#include "custom_adv.h"

// Helper chuyển float sang int (25.5 -> 2550)
static int16_t convert_float_to_int16(float value) {
  return (int16_t)(value * 100);
}

void fill_adv_packet(CustomAdv_t *pData, uint8_t flags, uint16_t companyID,
                     uint32_t node_id, float temp, float hum, char *name)
{
  // Reset toàn bộ bộ nhớ struct về 0
  memset(pData, 0, sizeof(CustomAdv_t));

  // 1. Flags (3 bytes)
  pData->len_flags = 0x02;
  pData->type_flags = 0x01;
  pData->val_flags = flags;

  // 2. Header Manuf
  // Len = Type(1) + Company(2) + NodeID(4) + Temp(2) + Hum(2) = 11 bytes
  pData->len_manuf = 11;
  pData->type_manuf = 0xFF;

  // 3. Company ID
  pData->company_LO = companyID & 0xFF;
  pData->company_HI = (companyID >> 8) & 0xFF;

  // 4. Node ID (4 bytes)
  pData->node_id_0 = node_id & 0xFF;
  pData->node_id_1 = (node_id >> 8) & 0xFF;
  pData->node_id_2 = (node_id >> 16) & 0xFF;
  pData->node_id_3 = (node_id >> 24) & 0xFF;

  // 5. Sensor Data (4 bytes)
  int16_t i_temp = convert_float_to_int16(temp);
  int16_t i_hum = convert_float_to_int16(hum);

  pData->temp_LO = i_temp & 0xFF;
  pData->temp_HI = (i_temp >> 8) & 0xFF;
  pData->hum_LO = i_hum & 0xFF;
  pData->hum_HI = (i_hum >> 8) & 0xFF;

  // 6. Name
  int n = strlen(name);
  if (n > NAME_MAX_LENGTH) n = NAME_MAX_LENGTH;

  pData->type_name = 0x09;
  pData->len_name = 1 + n;
  strncpy(pData->name, name, n);

  // 7. TÍNH KÍCH THƯỚC (QUAN TRỌNG: Đã sửa công thức)
  // Flags(3) + Manuf(1 byte Len + 11 byte Data) + Name(1 byte Len + 1 byte Type + n byte Chữ)
  pData->data_size = 3 + (1 + pData->len_manuf) + (1 + pData->len_name);

  app_log("ADV Init: NodeID=%lu (Size=%d)\r\n", node_id, pData->data_size);
}

void start_adv(CustomAdv_t *pData, uint8_t advertising_set_handle)
{
  sl_status_t sc;
  sc = sl_bt_legacy_advertiser_set_data(advertising_set_handle,
                                        0,
                                        pData->data_size,
                                        (const uint8_t *)pData);

  if(sc == SL_STATUS_OK) {
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
  } else {
      app_log("ERR: Set Data Failed 0x%04x\r\n", sc);
  }
}

void update_adv_data(CustomAdv_t *pData, uint8_t advertising_set_handle,
                     float temp, float hum)
{
  int16_t i_temp = convert_float_to_int16(temp);
  int16_t i_hum = convert_float_to_int16(hum);

  // Cập nhật giá trị mới
  pData->temp_LO = i_temp & 0xFF;
  pData->temp_HI = (i_temp >> 8) & 0xFF;
  pData->hum_LO = i_hum & 0xFF;
  pData->hum_HI = (i_hum >> 8) & 0xFF;

  // Gửi cập nhật
  sl_bt_legacy_advertiser_set_data(advertising_set_handle, 0, pData->data_size, (const uint8_t *)pData);
}
