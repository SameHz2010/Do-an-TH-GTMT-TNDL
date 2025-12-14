#include "dht20.h"
#include "sl_i2cspm_instances.h"
#include "em_i2c.h"
#include "sl_sleeptimer.h"
#include "app_log.h"

// Hàm scan I2C bus
void i2c_scan(void)
{
  app_log("\n=== I2C Bus Scan ===\n");
  app_log("Scanning addresses 0x01 to 0x7F...\n");
  bool found = false;

  for (uint8_t addr = 1; addr < 127; addr++) {
      I2C_TransferSeq_TypeDef seq;
      uint8_t dummy;

      seq.addr = addr << 1;
      seq.flags = I2C_FLAG_READ;
      seq.buf[0].data = &dummy;
      seq.buf[0].len = 1;

      I2C_TransferReturn_TypeDef ret = I2CSPM_Transfer(sl_i2cspm_sensor, &seq);

      if (ret == i2cTransferDone) {
          if (addr == DHT20_ADDR) {
              app_log("  [FOUND] Device at 0x%02X <- DHT20\n", addr);
          } else {
              app_log("  [FOUND] Device at 0x%02X\n", addr);
          }
          found = true;
      }
  }

  if (!found) {
      app_log("  No devices found!\n");
      app_log("  Check your connections:\n");
      app_log("    - SCL on PB0 (EXP Pin 7)\n");
      app_log("    - SDA on PB1 (EXP Pin 9)\n");
      app_log("    - Pull-up resistors (4.7k ohm)\n");
      app_log("    - Power: 3.3V\n");
  }
  app_log("===================\n\n");
}

// Kiểm tra DHT20 có kết nối không
bool dht20_is_connected(void)
{
  I2C_TransferSeq_TypeDef seq;
  uint8_t dummy;

  seq.addr = DHT20_ADDR << 1;
  seq.flags = I2C_FLAG_READ;
  seq.buf[0].data = &dummy;
  seq.buf[0].len = 1;

  return (I2CSPM_Transfer(sl_i2cspm_sensor, &seq) == i2cTransferDone);
}

// Soft reset DHT20
sl_status_t dht20_soft_reset(void)
{
  uint8_t reset_cmd = 0xBA;
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;

  app_log("Sending soft reset to DHT20...\n");

  seq.addr = DHT20_ADDR << 1;
  seq.flags = I2C_FLAG_WRITE;
  seq.buf[0].data = &reset_cmd;
  seq.buf[0].len = 1;

  ret = I2CSPM_Transfer(sl_i2cspm_sensor, &seq);
  if (ret != i2cTransferDone) {
      app_log("Soft reset failed\n");
      return SL_STATUS_FAIL;
  }

  // Đợi 20ms sau reset
  sl_sleeptimer_delay_millisecond(20);

  app_log("Soft reset complete\n");
  return SL_STATUS_OK;
}

// Khởi tạo DHT20
sl_status_t dht20_init(void)
{
  uint8_t status;
  I2C_TransferSeq_TypeDef seq;
  I2C_TransferReturn_TypeDef ret;

  // Kiểm tra kết nối
  if (!dht20_is_connected()) {
      app_log("ERROR: DHT20 not found at address 0x%02X\n", DHT20_ADDR);
      return SL_STATUS_NOT_FOUND;
  }

  app_log("DHT20 detected at 0x%02X\n", DHT20_ADDR);

  // Soft reset sensor
  dht20_soft_reset();

  // Đợi sensor ổn định sau khi power-on/reset
  sl_sleeptimer_delay_millisecond(100);

  // Đọc trạng thái
  seq.addr = DHT20_ADDR << 1;
  seq.flags = I2C_FLAG_READ;
  seq.buf[0].data = &status;
  seq.buf[0].len = 1;

  ret = I2CSPM_Transfer(sl_i2cspm_sensor, &seq);
  if (ret != i2cTransferDone) {
      app_log("ERROR: Cannot read DHT20 status\n");
      return SL_STATUS_FAIL;
  }

  app_log("DHT20 status register: 0x%02X\n", status);
  app_log("  - Bit[7] (Busy): %d\n", (status >> 7) & 1);
  app_log("  - Bit[5:4] (Mode): %d\n", (status >> 4) & 3);
  app_log("  - Bit[3] (CAL enable): %d\n", (status >> 3) & 1);

  // Kiểm tra calibration (bit 3 phải = 1)
  if ((status & 0x08) == 0) {
      app_log("DHT20 not calibrated, sending init command...\n");

      // Gửi lệnh khởi tạo: 0xBE 0x08 0x00
      uint8_t init_cmd[3] = {0xBE, 0x08, 0x00};
      seq.flags = I2C_FLAG_WRITE;
      seq.buf[0].data = init_cmd;
      seq.buf[0].len = 3;

      ret = I2CSPM_Transfer(sl_i2cspm_sensor, &seq);
      if (ret != i2cTransferDone) {
          app_log("ERROR: Calibration command failed\n");
          return SL_STATUS_FAIL;
      }

      // Đợi calibration hoàn thành
      sl_sleeptimer_delay_millisecond(10);

      // Đọc lại status để xác nhận
      seq.flags = I2C_FLAG_READ;
      seq.buf[0].data = &status;
      seq.buf[0].len = 1;
      ret = I2CSPM_Transfer(sl_i2cspm_sensor, &seq);

      app_log("DHT20 status after calibration: 0x%02X\n", status);

      if ((status & 0x08) == 0) {
          app_log("WARNING: Calibration bit still not set!\n");
      } else {
          app_log("Calibration successful!\n");
      }
  }

  return SL_STATUS_OK;
}

sl_status_t dht20_read(float *temperature, float *humidity)
{
  app_log("\n\t\tNHIET DO & DO AM\n\n");
    uint8_t trigger_cmd[3] = {0xAC, 0x33, 0x00};
    uint8_t rx_buffer[7];
    I2C_TransferSeq_TypeDef seq;
    I2C_TransferReturn_TypeDef ret;

    // Gửi lệnh trigger measurement
    seq.addr = DHT20_ADDR << 1;
    seq.flags = I2C_FLAG_WRITE;
    seq.buf[0].data = trigger_cmd;
    seq.buf[0].len = 3;

    ret = I2CSPM_Transfer(sl_i2cspm_sensor, &seq);
    if (ret != i2cTransferDone) {
        return SL_STATUS_TRANSMIT;
    }

    // Đợi đo xong (80ms)
    sl_sleeptimer_delay_millisecond(80);

    // Đọc 7 bytes dữ liệu
    seq.flags = I2C_FLAG_READ;
    seq.buf[0].data = rx_buffer;
    seq.buf[0].len = 7;

    ret = I2CSPM_Transfer(sl_i2cspm_sensor, &seq);
    if (ret != i2cTransferDone) {
        return SL_STATUS_RECEIVE;
    }

    // In raw data
    app_log("\t Raw: %02X %02X %02X %02X %02X %02X %02X\n",
            rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3],
            rx_buffer[4], rx_buffer[5], rx_buffer[6]);

    // Kiểm tra busy bit
    if (rx_buffer[0] & 0x80) {
        return SL_STATUS_NOT_READY;
    }

    // Trích xuất RH_Code (20 bits)
    uint32_t RH_Code = ((uint32_t)rx_buffer[1] << 12) |
                       ((uint32_t)rx_buffer[2] << 4) |
                       ((uint32_t)rx_buffer[3] >> 4);

    // Trích xuất Temp_Code (20 bits)
    uint32_t Temp_Code = (((uint32_t)rx_buffer[3] & 0x0F) << 16) |
                         ((uint32_t)rx_buffer[4] << 8) |
                         ((uint32_t)rx_buffer[5]);

    app_log("\t RH_Code: 0x%05lX (%lu)\n", RH_Code, RH_Code);
    app_log("\t Temp_Code: 0x%05lX (%lu)\n", Temp_Code, Temp_Code);

    // Tính toán với kiểm tra
    float hum_value = ((float)RH_Code / 1048576.0f) * 100.0f;
    float temp_value = ((float)Temp_Code / 1048576.0f) * 200.0f - 50.0f;

    // In giá trị tính được
    app_log("\t Humidity: %d.%02d%%, Temperature: %d.%02d C\n\n",
            (int)hum_value,
            (int)((hum_value - (int)hum_value) * 100),
            (int)temp_value,
            (int)((temp_value - (int)temp_value) * 100));

    // Gán giá trị
    *humidity = hum_value;
    *temperature = temp_value;

    return SL_STATUS_OK;
}
