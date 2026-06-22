#include "Communication/espi2c.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static I2C_HandleTypeDef *p_hi2c = NULL;
static UART_HandleTypeDef *p_huart = NULL;
static espi2c_callback_t p_callback = NULL;

static uint8_t rx_byte = 0;
static uint8_t rx_queue[ESPI2C_MAX_BUFFER_SIZE];
static volatile uint16_t queue_head = 0;
static volatile uint16_t queue_tail = 0;
static volatile uint8_t rearm_required = 0;

static HAL_StatusTypeDef arm_byte_receive(void)
{
    HAL_StatusTypeDef status = HAL_I2C_Slave_Receive_IT(p_hi2c, &rx_byte, 1);
    rearm_required = (status == HAL_OK) ? 0U : 1U;
    return status;
}

static void debug_log(const char *format, ...)
{
    if (p_huart == NULL) return;
    
    char buffer[128];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    if (len > 0)
    {
        HAL_UART_Transmit(p_huart, (uint8_t*)buffer, len, 1000);
    }
}

void espi2c_init(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart, espi2c_callback_t callback)
{
    p_hi2c = hi2c;
    p_huart = huart;
    p_callback = callback;

    queue_head = 0;
    queue_tail = 0;
    rearm_required = 0;

    HAL_StatusTypeDef status = arm_byte_receive();
    debug_log("\r\n[espi2c] Initialized byte receiver, status=%d\r\n", status);
}

void espi2c_poll(void)
{
    if (p_hi2c == NULL) return;

    if (rearm_required != 0U)
    {
        arm_byte_receive();
    }

    while (queue_tail != queue_head)
    {
        uint16_t tail = queue_tail;
        uint8_t byte_val = rx_queue[tail];
        queue_tail = (uint16_t)((tail + 1U) % ESPI2C_MAX_BUFFER_SIZE);

        if (p_callback != NULL)
        {
            p_callback(&byte_val, 1);
        }
    }
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != p_hi2c) return;

    uint16_t next_head = (uint16_t)((queue_head + 1U) % ESPI2C_MAX_BUFFER_SIZE);
    if (next_head != queue_tail)
    {
        rx_queue[queue_head] = rx_byte;
        queue_head = next_head;
    }

    arm_byte_receive();
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c != p_hi2c) return;

    CLEAR_BIT(p_hi2c->Instance->CR1, I2C_CR1_ACK);
    rearm_required = 1U;
}
