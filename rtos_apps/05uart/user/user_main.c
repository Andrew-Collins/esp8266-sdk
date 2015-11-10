/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/12/1, v1.0 create this file.
*******************************************************************************/
#include "esp_common.h"

/******************************************************************************
 * FunctionName : user_init
 * Description  : Buffers input characters from UART and outputs them when a
 *                 carriage return is received. 
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart.h"
#include "esp_common.h"

// see eagle_soc.h for these definitions
#define LED_GPIO 2
#define LED_GPIO_MUX PERIPHS_IO_MUX_GPIO2_U
#define LED_GPIO_FUNC FUNC_GPIO2

// This was defined in the old SDK.
#ifndef GPIO_OUTPUT_SET
#define GPIO_OUTPUT_SET(gpio_no, bit_value) \
    gpio_output_set(bit_value<<gpio_no, ((~bit_value)&0x01)<<gpio_no, 1<<gpio_no,0)
#endif

/*
 * this task will blink an LED
 */
void blinky(void *pvParameters)
{
	const portTickType xDelay = 500 / portTICK_RATE_MS;
	static uint8_t state=0;
	PIN_FUNC_SELECT(LED_GPIO_MUX, LED_GPIO_FUNC);
	for(;;)
	{
		GPIO_OUTPUT_SET(LED_GPIO, state);
		state ^=1;
		vTaskDelay( xDelay);
	}
}


/*
 * this task will print the message
 */
void helloworld(void *pvParameters)
{
	const portTickType xDelay = 1000 / portTICK_RATE_MS;
	for(;;)
	{
		printf("Hello World!\n");
		vTaskDelay( xDelay);
	}
}

/*
 * This is entry point for user code
 */
void ICACHE_FLASH_ATTR
user_init(void)
{
	UART_WaitTxFifoEmpty(UART0);
        UART_WaitTxFifoEmpty(UART1);

	UART_ConfigTypeDef uart_config;
    	uart_config.baud_rate    = BIT_RATE_9600;
	uart_config.data_bits     = UART_WordLength_8b;
        uart_config.parity          = USART_Parity_None;
        uart_config.stop_bits     = USART_StopBits_1;
        uart_config.flow_ctrl      = USART_HardwareFlowControl_None;
        uart_config.UART_RxFlowThresh = 120;
        uart_config.UART_InverseMask = UART_None_Inverse;
        UART_ParamConfig(UART0, &uart_config);

	UART_SetPrintPort(UART0);

    UART_IntrConfTypeDef uart_intr;
    uart_intr.UART_IntrEnMask = UART_RXFIFO_TOUT_INT_ENA | UART_FRM_ERR_INT_ENA | UART_RXFIFO_FULL_INT_ENA | UART_TXFIFO_EMPTY_INT_ENA;
    uart_intr.UART_RX_FifoFullIntrThresh = 10;
    uart_intr.UART_RX_TimeOutIntrThresh = 2;
    uart_intr.UART_TX_FifoEmptyIntrThresh = 20;
    UART_IntrConfig(UART0, &uart_intr);

    UART_SetPrintPort(UART0);
    UART_intr_handler_register(uart0_rx_intr_handler);
    ETS_UART_INTR_ENABLE();

	
	xTaskCreate(blinky, "bl", configMINIMAL_STACK_SIZE, NULL, 3, NULL);
	// xTaskCreate(helloworld, "hw", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
	
}
