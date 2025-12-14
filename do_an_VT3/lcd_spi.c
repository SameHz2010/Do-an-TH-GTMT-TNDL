/***************************************************************************//**
 * @file
 * @brief SPI abstraction used by memory lcd display
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/

#include <lcd_spi.h>
#include "sl_clock_manager.h"
#include "em_gpio.h"

sl_status_t sli_memlcd_spi_init(sli_memlcd_spi_handle_t *handle, int baudrate, USART_ClockMode_TypeDef mode)
{
  USART_InitSync_TypeDef init = USART_INITSYNC_DEFAULT;
  USART_TypeDef *usart = handle->usart;

  int usart_index = USART_NUM(usart);
  // take note if another driver has enable RX
  uint32_t rxpen = GPIO->USARTROUTE[usart_index].ROUTEEN & _GPIO_USART_ROUTEEN_RXPEN_MASK;


  sl_clock_manager_enable_bus_clock(SL_BUS_CLOCK_GPIO);
  sl_clock_manager_enable_bus_clock(handle->clock);

  GPIO_PinModeSet((GPIO_Port_TypeDef)handle->clk_port, handle->clk_pin, gpioModePushPull, 0);
  GPIO_PinModeSet((GPIO_Port_TypeDef)handle->mosi_port, handle->mosi_pin, gpioModePushPull, 0);

  init.baudrate = baudrate;
  init.clockMode = mode;
  init.msbf = true;

  USART_InitSync(usart, &init);

  GPIO->USARTROUTE[usart_index].TXROUTE = (handle->mosi_port << _GPIO_USART_TXROUTE_PORT_SHIFT)
                                          | (handle->mosi_pin << _GPIO_USART_TXROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[usart_index].CLKROUTE = (handle->clk_port << _GPIO_USART_CLKROUTE_PORT_SHIFT)
                                           | (handle->clk_pin << _GPIO_USART_CLKROUTE_PIN_SHIFT);
  GPIO->USARTROUTE[usart_index].ROUTEEN = GPIO_USART_ROUTEEN_TXPEN | GPIO_USART_ROUTEEN_CLKPEN | rxpen;

  return SL_STATUS_OK;
}

sl_status_t sli_memlcd_spi_shutdown(sli_memlcd_spi_handle_t *handle)
{
  USART_Enable(handle->usart, usartDisable);
  sl_clock_manager_disable_bus_clock(handle->clock);
  return SL_STATUS_OK;
}

sl_status_t sli_memlcd_spi_tx(sli_memlcd_spi_handle_t *handle, const void *data, unsigned len)
{
  const char *buffer = data;
  USART_TypeDef *usart = handle->usart;

  for (unsigned i = 0; i < len; i++) {
    USART_Tx(usart, SL_RBIT8(buffer[i]));
  }

  /* Note that at this point all the data is loaded into the fifo, this does
   * not necessarily mean that the transfer is complete. */
  return SL_STATUS_OK;
}

void sli_memlcd_spi_wait(sli_memlcd_spi_handle_t *handle)
{
  USART_TypeDef *usart = handle->usart;

  /* Wait for all transfers to finish */
  while (!(usart->STATUS & USART_STATUS_TXC))
    ;
}

void sli_memlcd_spi_rx_flush(sli_memlcd_spi_handle_t *handle)
{
  USART_TypeDef *usart = handle->usart;

  /* Read data until RXFIFO empty */
  while (usart->STATUS & USART_STATUS_RXDATAV) {
    USART_Rx(usart);
  }
}
