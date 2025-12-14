/***************************************************************************//**
 * @file app.c
 * @brief Node 1: Gateway Mode (Vừa Quảng bá sensor mình, vừa Quét RSSI node khác)
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app.h"
#include "dht20.h"
#include "sl_sleeptimer.h"
#include "app_log.h"
#include "app_lcd.h"
#include "sl_bt_api.h"
#include "custom_adv.h"
#include "sl_iostream.h"
#include "sl_iostream_handles.h"
#include "sl_simple_button_instances.h"

// FIX GLIB
#include "glib.h"
#include "dmd.h"
extern GLIB_Context_t glibContext;

// --- ĐỊNH NGHĨA CHU KỲ ---
#define STEP_RES_100_MILLI          0
#define STEP_RES_1_SEC              ((1) << 6)
#define STEP_RES_10_SEC             ((2) << 6)
#define STEP_RES_10_MIN             ((3) << 6)
#define STEP_RES_BIT_MASK           0xC0

#define SET_1_SEC(x)                (uint8_t)(STEP_RES_1_SEC | ((x) & (0x3F)))
#define SET_10_SEC(x)               (uint8_t)(STEP_RES_10_SEC | ((x) & (0x3F)))

#define EX_B0_PRESS                 ((1) << 5)

// --- CẤU HÌNH ---
static uint32_t measure_interval_ms = 1000;
static uint32_t adv_interval_ms = 100;

static uint8_t advertising_set_handle = 0xff;
static CustomAdv_t myAdvData;

// === QUAN TRỌNG: ĐÂY LÀ NODE 1 ===
static uint32_t myNodeID = 1;

static uint32_t last_measure_tick = 0;
static char input_buffer[64];
static uint8_t input_pos = 0;

static float current_temp = 0.0f;
static float current_hum = 0.0f;

static uint8_t period_idx = 0;
static uint8_t periods[] = {
    SET_1_SEC(1),
    SET_1_SEC(10),
    SET_10_SEC(6),
    0
};

// Hàm phụ trợ tính thời gian
static uint32_t parse_period_to_ms(uint8_t interval) {
  switch (interval & STEP_RES_BIT_MASK) {
    case STEP_RES_100_MILLI: return 100 * (interval & (~STEP_RES_BIT_MASK));
    case STEP_RES_1_SEC:     return 1000 * (interval & (~STEP_RES_BIT_MASK));
    case STEP_RES_10_SEC:    return 10000 * (interval & (~STEP_RES_BIT_MASK));
    case STEP_RES_10_MIN:    return 600000 * (interval & (~STEP_RES_BIT_MASK));
    default:                 return 0;
  }
}

// Xử lý nút nhấn
void sl_button_on_change(const sl_button_t *handle) {
  if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
      if (handle == &sl_button_btn0) {
          sl_bt_external_signal(EX_B0_PRESS);
      }
  }
}

// Xử lý lệnh UART cấu hình
void process_input_command(char *cmd) {
  if (strncmp(cmd, "SET_P=", 6) == 0) {
      int val = atoi(cmd + 6);
      if (val >= 100) {
          measure_interval_ms = val;
          app_log(">> CAU HINH: Chu ky = %lu ms\n", measure_interval_ms);
          memlcd_update_sensor(current_temp, current_hum, measure_interval_ms);
      }
  }
}

void check_uart_input(void) {
  char c;
  size_t bytes_read;
  if (sl_iostream_read(sl_iostream_vcom_handle, &c, 1, &bytes_read) == SL_STATUS_OK && bytes_read > 0) {
      if (c == '\n' || c == '\r') {
          if (input_pos > 0) {
              input_buffer[input_pos] = '\0';
              process_input_command(input_buffer);
              input_pos = 0;
          }
      } else if (input_pos < sizeof(input_buffer) - 1) {
          input_buffer[input_pos++] = c;
      }
  }
}

// === MAIN INIT ===
void app_init(void) {
  app_log("\n=======================================\n");
  app_log("    NODE 1: GATEWAY (ADV + SCAN)\n");
  app_log("=======================================\n");

  memlcd_app_init();
  memlcd_update_sensor(0.0, 0.0, measure_interval_ms);

  if (dht20_init() == SL_STATUS_OK) {
      app_log("[SUCCESS] DHT20 Init OK\n");
  } else {
      app_log("[ERROR] DHT20 Init Failed\n");
  }
  sl_simple_button_enable(&sl_button_btn0);
}

// === XỬ LÝ SỰ KIỆN BLE ===
void sl_bt_on_event(sl_bt_msg_t *evt) {
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {

    // 1. KHI KHỞI ĐỘNG
    case sl_bt_evt_system_boot_id:
      app_log("[BLE] Booted. Starting ADV & SCANner...\n");

      // --- A. BẮT ĐẦU QUẢNG BÁ (NHIỆM VỤ CŨ) ---
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      uint32_t adv_tim = (uint32_t)(adv_interval_ms * 1.6);
      sl_bt_advertiser_set_timing(advertising_set_handle, adv_tim, adv_tim, 0, 0);

      fill_adv_packet(&myAdvData, FLAG_VALUE, COMPANY_ID, myNodeID, 0.0f, 0.0f, "DHT20_1");
      start_adv(&myAdvData, advertising_set_handle);

      // --- B. BẮT ĐẦU QUÉT (NHIỆM VỤ MỚI) ---
      // Lệnh bắt đầu quét
      sc = sl_bt_scanner_start(sl_bt_scanner_scan_phy_1m, sl_bt_scanner_discover_generic);

      if (sc == SL_STATUS_OK) {
          app_log(">> Scanner Started OK!\n");
      } else {
          app_log(">> Scanner Error: 0x%x\n", sc);
      }
      break;

      // 2. KHI QUÉT THẤY THIẾT BỊ (ĐÃ SỬA TÊN SỰ KIỆN CHO GSDK MỚI)
      // Dùng legacy_advertisement_report thay vì scan_report
    case sl_bt_evt_scanner_legacy_advertisement_report_id:
      {
        // Lấy dữ liệu gói tin (Cấu trúc mới)
        uint8_t *data = evt->data.evt_scanner_legacy_advertisement_report.data.data;
        uint8_t len = evt->data.evt_scanner_legacy_advertisement_report.data.len;
        int8_t rssi = evt->data.evt_scanner_legacy_advertisement_report.rssi;

        // Lọc tìm Company ID 0x02FF (Định dạng Little Endian: FF 02)
        // Data: [Length][Type 0xFF][CompanyLO 0xFF][CompanyHI 0x02][NodeID]...

        // Duyệt qua gói tin để tìm pattern Manufacturer Data
        for (int i = 0; i < len - 4; i++) {
            // Tìm chuỗi: Type=0xFF, CompanyLO=0xFF, CompanyHI=0x02
            if (data[i] == 0xFF && data[i+1] == 0xFF && data[i+2] == 0x02) {

                // Byte tiếp theo (i+3) chính là Node ID (Byte thấp nhất)
                uint8_t detected_id = data[i+3];

                // Chỉ lấy Node 2 và Node 3 để gửi lên PC
                if (detected_id == 2 || detected_id == 3) {
                    // Gửi định dạng "ID RSSI" lên cổng COM
                    // App PC sẽ đọc dòng này
                    app_log("%d %d\r\n", detected_id, rssi);
                }
                break; // Tìm thấy rồi thì thoát vòng lặp
            }
        }
      }
      break;

    case sl_bt_evt_connection_closed_id:
      sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & EX_B0_PRESS) {
          period_idx++;
          if (period_idx >= sizeof(periods)) period_idx = 0;
          measure_interval_ms = parse_period_to_ms(periods[period_idx]);
          memlcd_update_sensor(current_temp, current_hum, measure_interval_ms);
          if (measure_interval_ms > 0) last_measure_tick = 0;
      }
      break;

    default: break;
  }
}

// === VÒNG LẶP CHÍNH (ĐỌC CẢM BIẾN NODE 1) ===
void app_process_action(void) {
  check_uart_input();

  if (measure_interval_ms > 0) {
      uint32_t current_tick = sl_sleeptimer_get_tick_count();
      uint32_t tick_diff = sl_sleeptimer_tick_to_ms(current_tick - last_measure_tick);

      if (tick_diff >= measure_interval_ms) {
          last_measure_tick = current_tick;

          float temp = 0.0f;
          float hum = 0.0f;

          // Node 1 vẫn đọc cảm biến và hiển thị bình thường
          if (dht20_read(&temp, &hum) == SL_STATUS_OK) {
              current_temp = temp;
              current_hum = hum;

              // Cập nhật LCD
              memlcd_update_sensor(temp, hum, measure_interval_ms);

              // Cập nhật gói tin quảng bá của chính mình
              if (advertising_set_handle != 0xff) {
                  update_adv_data(&myAdvData, advertising_set_handle, temp, hum);
              }
          }
      }
  }
}
