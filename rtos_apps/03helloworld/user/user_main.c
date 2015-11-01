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
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "uart.h"

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

	xTaskCreate(helloworld, "hw", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
	
}
