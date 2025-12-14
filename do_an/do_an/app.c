/***************************************************************************//**
 * @file app.c
 * @brief Logic chính của chương trình
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

// FIX GLIB: Dùng extern để "mượn" biến từ app_lcd.c
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
static uint32_t measure_interval_ms = 1000; // Mặc định 1s
static uint32_t adv_interval_ms = 100;

static uint8_t advertising_set_handle = 0xff;
static CustomAdv_t myAdvData;
static uint32_t myStudentID = 22207070;

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

static uint32_t parse_period_to_ms(uint8_t interval) {
    switch (interval & STEP_RES_BIT_MASK) {
        case STEP_RES_100_MILLI: return 100 * (interval & (~STEP_RES_BIT_MASK));
        case STEP_RES_1_SEC:     return 1000 * (interval & (~STEP_RES_BIT_MASK));
        case STEP_RES_10_SEC:    return 10000 * (interval & (~STEP_RES_BIT_MASK));
        case STEP_RES_10_MIN:    return 600000 * (interval & (~STEP_RES_BIT_MASK));
        default:                 return 0;
    }
}

void sl_button_on_change(const sl_button_t *handle) {
    if (sl_button_get_state(handle) == SL_SIMPLE_BUTTON_PRESSED) {
        if (handle == &sl_button_btn0) {
            sl_bt_external_signal(EX_B0_PRESS);
        }
    }
}

void process_input_command(char *cmd) {
  if (strncmp(cmd, "SET_P=", 6) == 0) {
      int val = atoi(cmd + 6);
      if (val >= 100) {
          measure_interval_ms = val;
          app_log(">> CAU HINH UART: Chu ky = %lu ms\n", measure_interval_ms);
          memlcd_update_sensor(current_temp, current_hum, measure_interval_ms);
      }
  }
  else if (strncmp(cmd, "SET_ADV=", 8) == 0) {
      int val = atoi(cmd + 8);
      if (val >= 32 && advertising_set_handle != 0xff) {
          adv_interval_ms = val;
          uint32_t adv_tim = (uint32_t)(val * 1.6);
          sl_bt_advertiser_set_timing(advertising_set_handle, adv_tim, adv_tim, 0, 0);
          app_log(">> CAU HINH UART: BLE ADV = %lu ms\n", adv_interval_ms);
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

// === ĐÂY LÀ HÀM MAIN.C CẦN TÌM ===
void app_init(void) {
  app_log("\n=======================================\n");
  app_log("    HE THONG GIAM SAT MOI TRUONG\n");
  app_log("=======================================\n");

  memlcd_app_init(); // Gọi hàm bên app_lcd.c
  memlcd_update_sensor(0.0, 0.0, measure_interval_ms);

  if (dht20_init() == SL_STATUS_OK) {
      app_log("[SUCCESS] DHT20 Init OK\n");
  } else {
      app_log("[ERROR] DHT20 Init Failed\n");
  }
  sl_simple_button_enable(&sl_button_btn0);
}

void sl_bt_on_event(sl_bt_msg_t *evt) {
  sl_status_t sc;
  switch (SL_BT_MSG_ID(evt->header)) {
    case sl_bt_evt_system_boot_id:
      app_log("[BLE] System Booted\n");
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      uint32_t adv_tim = (uint32_t)(adv_interval_ms * 1.6);
      sl_bt_advertiser_set_timing(advertising_set_handle, adv_tim, adv_tim, 0, 0);

      fill_adv_packet(&myAdvData, FLAG_VALUE, COMPANY_ID, myStudentID, 0.0f, 0.0f, "DHT20_BLE");
      start_adv(&myAdvData, advertising_set_handle);
      break;

    case sl_bt_evt_connection_closed_id:
      sl_bt_legacy_advertiser_start(advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      break;

    case sl_bt_evt_system_external_signal_id:
      if (evt->data.evt_system_external_signal.extsignals & EX_B0_PRESS) {
          period_idx++;
          if (period_idx >= sizeof(periods)) period_idx = 0;

          measure_interval_ms = parse_period_to_ms(periods[period_idx]);
          app_log(">> Nut nhan: Mode %d (%lu ms)\n", period_idx, measure_interval_ms);

          memlcd_update_sensor(current_temp, current_hum, measure_interval_ms);

          if (measure_interval_ms > 0) last_measure_tick = 0;
      }
      break;

    default: break;
  }
}

// === ĐÂY LÀ HÀM MAIN.C CẦN TÌM ===
void app_process_action(void) {
  check_uart_input();

  if (measure_interval_ms > 0) {
      uint32_t current_tick = sl_sleeptimer_get_tick_count();
      uint32_t tick_diff = sl_sleeptimer_tick_to_ms(current_tick - last_measure_tick);

      if (tick_diff >= measure_interval_ms) {
          last_measure_tick = current_tick;

          float temp = 0.0f;
          float hum = 0.0f;

          if (dht20_read(&temp, &hum) == SL_STATUS_OK) {
              current_temp = temp;
              current_hum = hum;

              int t_int = (int)temp;
              int t_frac = (int)((temp - t_int) * 100); if(t_frac < 0) t_frac = -t_frac;
              int h_int = (int)hum;
              int h_frac = (int)((hum - h_int) * 100);

              app_log("DATA:T=%d.%02d,H=%d.%02d\n", t_int, t_frac, h_int, h_frac);

              memlcd_update_sensor(temp, hum, measure_interval_ms);

              if (advertising_set_handle != 0xff) {
                  update_adv_data(&myAdvData, advertising_set_handle, temp, hum);
              }
          } else {
              app_log("ERR: Read Fail\n");
          }
      }
  }
}
