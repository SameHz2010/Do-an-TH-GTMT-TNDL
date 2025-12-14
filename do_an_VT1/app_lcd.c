#include <stdio.h>
#include "sl_board_control.h"
#include "em_assert.h"
#include "glib.h"
#include "dmd.h"
#include "app_lcd.h"

// Biến toàn cục context màn hình
GLIB_Context_t glibContext;

void memlcd_app_init(void)
{
  uint32_t status;
  status = sl_board_enable_display(); EFM_ASSERT(status == SL_STATUS_OK);
  status = DMD_init(0); EFM_ASSERT(status == DMD_OK);
  status = GLIB_contextInit(&glibContext); EFM_ASSERT(status == GLIB_OK);

  glibContext.backgroundColor = White;
  glibContext.foregroundColor = Black;
  GLIB_clear(&glibContext);
  GLIB_setFont(&glibContext, (GLIB_Font_t *) &GLIB_FontNarrow6x8);

  // Dịch dòng khởi động xuống dòng 2 cho đồng bộ
  GLIB_drawStringOnLine(&glibContext, "SYSTEM BOOT...", 2, GLIB_ALIGN_CENTER, 0, 0, true);
  DMD_updateDisplay();
}

void memlcd_update_sensor(float temp, float hum, uint32_t interval_ms)
{
  char buf[40];
  GLIB_clear(&glibContext);

  // --- DÒNG 2: HIỂN THỊ CHU KỲ (Dịch từ 0 -> 2) ---
  if (interval_ms == 0) sprintf(buf, "CYCLE: NO UPDATE");
  else if (interval_ms >= 60000) sprintf(buf, "CYCLE: %lu min", interval_ms / 60000);
  else if (interval_ms >= 1000) sprintf(buf, "CYCLE: %lu sec", interval_ms / 1000);
  else sprintf(buf, "CYCLE: %lu ms", interval_ms);

  GLIB_drawStringOnLine(&glibContext, buf, 2, GLIB_ALIGN_CENTER, 0, 0, true);

  // --- DÒNG 4: TIÊU ĐỀ (Dịch từ 2 -> 4) ---
  GLIB_drawStringOnLine(&glibContext, "DHT20 MONITOR NODE 1", 4, GLIB_ALIGN_CENTER, 0, 0, true);

  // --- DÒNG 6 & 7: SỐ LIỆU (Dịch từ 4,5 -> 6,7) ---
  int t_int = (int)temp;
  int t_frac = (int)((temp - t_int) * 100); if(t_frac < 0) t_frac = -t_frac;
  int h_int = (int)hum;
  int h_frac = (int)((hum - h_int) * 100);

  // Nhiệt độ
  snprintf(buf, sizeof(buf), "Temp: %d.%02d C", t_int, t_frac);
  GLIB_drawStringOnLine(&glibContext, buf, 6, GLIB_ALIGN_LEFT, 5, 0, true);

  // Độ ẩm
  snprintf(buf, sizeof(buf), "Hum : %d.%02d %%", h_int, h_frac);
  GLIB_drawStringOnLine(&glibContext, buf, 7, GLIB_ALIGN_LEFT, 5, 0, true);

  DMD_updateDisplay();
}
