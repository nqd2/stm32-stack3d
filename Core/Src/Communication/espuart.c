#include "Communication/espuart.h"
#include <string.h>

static UART_HandleTypeDef *p_huart = NULL;
static DMA_HandleTypeDef *p_hdma = NULL;
static espuart_callback_t p_callback = NULL;

static uint8_t rx_dma_buf[ESPUART_RX_BUF_SIZE];
static uint16_t rx_read_idx = 0;

void espuart_init(UART_HandleTypeDef *huart, DMA_HandleTypeDef *hdma, espuart_callback_t callback)
{
    p_huart = huart;
    p_hdma = hdma;
    p_callback = callback;
    rx_read_idx = 0;

    memset(rx_dma_buf, 0, sizeof(rx_dma_buf));

    // Clear any pending UART error/status flags (PE, FE, NE, ORE)
    // On STM32F4, this is done by reading SR followed by DR
    volatile uint32_t tmpreg;
    tmpreg = p_huart->Instance->SR;
    tmpreg = p_huart->Instance->DR;
    (void)tmpreg;

    // Start UART DMA reception in circular mode
    HAL_UART_Receive_DMA(p_huart, rx_dma_buf, ESPUART_RX_BUF_SIZE);
}

void espuart_poll(void)
{
    if (p_huart == NULL || p_hdma == NULL) return;

    // Get current write position of DMA
    uint16_t dma_write_idx = (uint16_t)(ESPUART_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(p_hdma)) % ESPUART_RX_BUF_SIZE;

    // Process all bytes between rx_read_idx and dma_write_idx
    while (rx_read_idx != dma_write_idx)
    {
        uint8_t byte_val = rx_dma_buf[rx_read_idx];
        rx_read_idx = (uint16_t)((rx_read_idx + 1U) % ESPUART_RX_BUF_SIZE);

        if (p_callback != NULL)
        {
            p_callback(&byte_val, 1);
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart != p_huart) return;

    // If an error occurs (such as an overrun error), the HAL library
    // disables DMA. We must restart DMA reception to recover.
    volatile uint32_t tmpreg;
    tmpreg = p_huart->Instance->SR;
    tmpreg = p_huart->Instance->DR;
    (void)tmpreg;

    HAL_UART_Receive_DMA(p_huart, rx_dma_buf, ESPUART_RX_BUF_SIZE);
}
