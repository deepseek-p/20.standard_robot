/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       referee_usart_task.c/h
  * @brief      RM referee system data solve.
  * @note
  * @history
  *  Version    Date            Author          Modification
  *  V1.0.0     Nov-11-2019     RM              1. done
  ****************************(C) COPYRIGHT 2019 DJI****************************
  */
#include "referee_usart_task.h"
#include "main.h"
#include "cmsis_os.h"

#include "bsp_usart.h"
#include "detect_task.h"

#include "CRC8_CRC16.h"
#include "fifo.h"
#include "protocol.h"
#include "referee.h"
#include "rm_ui.h"
#include "wifi_bridge.h"
#include "uart_mode.h"
#if VT03_ENABLE
#include "vt03_link.h"
#endif

#if USART6_REFEREE
static void referee_unpack_fifo_data(void);
#endif

extern UART_HandleTypeDef huart6;

uint8_t usart6_buf[2][USART_RX_BUF_LENGHT];

fifo_s_t referee_fifo;
uint8_t referee_fifo_buf[REFEREE_FIFO_BUF_LENGTH];
unpack_data_t referee_unpack_obj;

#if WIFI_BRIDGE_ENABLE
uint8_t uart1_rx_buf[256];
volatile uint16_t uart1_rx_head = 0u;
volatile uint16_t uart1_rx_tail = 0u;
#endif

void referee_usart_task(void const *argument)
{
    (void)argument;

#if USART6_REFEREE
    init_referee_struct_data();
    rm_ui_init();
    fifo_s_init(&referee_fifo, referee_fifo_buf, REFEREE_FIFO_BUF_LENGTH);
    usart6_init(usart6_buf[0], usart6_buf[1], USART_RX_BUF_LENGHT);
#else
    // VT03 mode: use RXNE byte interrupt parser, no DMA double buffer setup.
    __HAL_UART_CLEAR_OREFLAG(&huart6);
    __HAL_UART_ENABLE_IT(&huart6, UART_IT_RXNE);
#endif

    while (1)
    {
#if USART6_REFEREE
        referee_unpack_fifo_data();
        rm_ui_update();
#endif
        osDelay(10);
    }
}

#if USART6_REFEREE
static void referee_unpack_fifo_data(void)
{
    uint8_t byte = 0;
    uint8_t sof = HEADER_SOF;
    unpack_data_t *p_obj = &referee_unpack_obj;

    while (fifo_s_used(&referee_fifo))
    {
        byte = fifo_s_get(&referee_fifo);
        switch (p_obj->unpack_step)
        {
        case STEP_HEADER_SOF:
        {
            if (byte == sof)
            {
                p_obj->unpack_step = STEP_LENGTH_LOW;
                p_obj->protocol_packet[p_obj->index++] = byte;
            }
            else
            {
                p_obj->index = 0;
            }
        }
        break;

        case STEP_LENGTH_LOW:
        {
            p_obj->data_len = byte;
            p_obj->protocol_packet[p_obj->index++] = byte;
            p_obj->unpack_step = STEP_LENGTH_HIGH;
        }
        break;

        case STEP_LENGTH_HIGH:
        {
            p_obj->data_len |= (byte << 8);
            p_obj->protocol_packet[p_obj->index++] = byte;

            if (p_obj->data_len < (REF_PROTOCOL_FRAME_MAX_SIZE - REF_HEADER_CRC_CMDID_LEN))
            {
                p_obj->unpack_step = STEP_FRAME_SEQ;
            }
            else
            {
                p_obj->unpack_step = STEP_HEADER_SOF;
                p_obj->index = 0;
            }
        }
        break;

        case STEP_FRAME_SEQ:
        {
            p_obj->protocol_packet[p_obj->index++] = byte;
            p_obj->unpack_step = STEP_HEADER_CRC8;
        }
        break;

        case STEP_HEADER_CRC8:
        {
            p_obj->protocol_packet[p_obj->index++] = byte;

            if (p_obj->index == REF_PROTOCOL_HEADER_SIZE)
            {
                if (verify_CRC8_check_sum(p_obj->protocol_packet, REF_PROTOCOL_HEADER_SIZE))
                {
                    p_obj->unpack_step = STEP_DATA_CRC16;
                }
                else
                {
                    p_obj->unpack_step = STEP_HEADER_SOF;
                    p_obj->index = 0;
                }
            }
        }
        break;

        case STEP_DATA_CRC16:
        {
            if (p_obj->index < (REF_HEADER_CRC_CMDID_LEN + p_obj->data_len))
            {
                p_obj->protocol_packet[p_obj->index++] = byte;
            }
            if (p_obj->index >= (REF_HEADER_CRC_CMDID_LEN + p_obj->data_len))
            {
                p_obj->unpack_step = STEP_HEADER_SOF;
                p_obj->index = 0;

                if (verify_CRC16_check_sum(p_obj->protocol_packet, REF_HEADER_CRC_CMDID_LEN + p_obj->data_len))
                {
                    referee_data_solve(p_obj->protocol_packet);
                }
            }
        }
        break;

        default:
        {
            p_obj->unpack_step = STEP_HEADER_SOF;
            p_obj->index = 0;
        }
        break;
        }
    }
}
#endif

void USART6_IRQHandler(void)
{
#if USART6_VT03
    uint32_t sr = USART6->SR;

    if ((sr & USART_SR_RXNE) != 0u)
    {
        uint8_t byte = (uint8_t)(USART6->DR & 0xFFu);
        vt03_parse_byte(byte);
    }
    else if ((sr & USART_SR_ORE) != 0u)
    {
        volatile uint8_t dump = (uint8_t)(USART6->DR & 0xFFu);
        (void)dump;
    }
#else
    if (USART6->SR & UART_FLAG_IDLE)
    {
        __HAL_UART_CLEAR_PEFLAG(&huart6);

        static uint16_t this_time_rx_len = 0;

        if ((huart6.hdmarx->Instance->CR & DMA_SxCR_CT) == RESET)
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR |= DMA_SxCR_CT;
            __HAL_DMA_ENABLE(huart6.hdmarx);
            fifo_s_puts(&referee_fifo, (char *)usart6_buf[0], this_time_rx_len);
            detect_hook(REFEREE_TOE);
        }
        else
        {
            __HAL_DMA_DISABLE(huart6.hdmarx);
            this_time_rx_len = USART_RX_BUF_LENGHT - __HAL_DMA_GET_COUNTER(huart6.hdmarx);
            __HAL_DMA_SET_COUNTER(huart6.hdmarx, USART_RX_BUF_LENGHT);
            huart6.hdmarx->Instance->CR &= ~(DMA_SxCR_CT);
            __HAL_DMA_ENABLE(huart6.hdmarx);
            fifo_s_puts(&referee_fifo, (char *)usart6_buf[1], this_time_rx_len);
            detect_hook(REFEREE_TOE);
        }
    }
#endif
}

#if USART1_VT03
void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;
    if (sr & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE))
    {
        (void)USART1->DR;
        return;
    }
    if (sr & USART_SR_RXNE)
    {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFFu);
        vt03_parse_byte(byte);
    }
}
#elif WIFI_BRIDGE_ENABLE
void USART1_IRQHandler(void)
{
    if ((USART1->SR & USART_SR_RXNE) != 0u)
    {
        uint8_t byte = (uint8_t)(USART1->DR & 0xFFu);
        uint16_t next = (uint16_t)((uart1_rx_head + 1u) % (uint16_t)sizeof(uart1_rx_buf));

        if (next != uart1_rx_tail)
        {
            uart1_rx_buf[uart1_rx_head] = byte;
            uart1_rx_head = next;
        }
    }
}

uint16_t uart1_rx_available(void)
{
    uint16_t head = uart1_rx_head;
    uint16_t tail = uart1_rx_tail;

    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(sizeof(uart1_rx_buf) - tail + head);
}

uint8_t uart1_rx_read_byte(void)
{
    uint8_t byte;

    byte = uart1_rx_buf[uart1_rx_tail];
    uart1_rx_tail = (uint16_t)((uart1_rx_tail + 1u) % (uint16_t)sizeof(uart1_rx_buf));
    return byte;
}
#endif

