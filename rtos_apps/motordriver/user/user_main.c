/*
The MIT License (MIT)

Copyright (c) 2014 Matt Callow

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
/*
	The obligatory blinky demo using FreeRTOS
	Blink an LED on GPIO pin 2
*/
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "lwip/err.h"

#define MOTOR_RIGHT_SPEED 5
#define MOTOR_LEFT_SPEED 4

#define MOTOR_RIGHT_DIRECTION 0
#define MOTOR_LEFT_DIRECTION 2

// This was defined in the old SDK.
#ifndef GPIO_OUTPUT_SET
#define GPIO_OUTPUT_SET(gpio_no, bit_value) \
    gpio_output_set(bit_value<<gpio_no, ((~bit_value)&0x01)<<gpio_no, 1<<gpio_no,0)
#endif

void enable_motors(void);
void disable_motors(void);

/*
 * this task will blink an LED
 */
void blinky(void *pvParameters)
{
	const portTickType xDelay = 1000 / portTICK_RATE_MS;
	static uint8_t state=0;

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);

	GPIO_OUTPUT_SET(MOTOR_RIGHT_SPEED, 0);
	GPIO_OUTPUT_SET(MOTOR_LEFT_SPEED, 0);
	
	// Use this module as a wifi access point
	wifi_set_opmode(0x03); // AP + STATION Mode

	// Create a socket server on port 8080 for users to connect to
	int sock_server, c, sock_client, read_size;
	struct sockaddr_in serv_addr, client_addr;
	char client_message[10];
	
	sock_server = socket(AF_INET, SOCK_STREAM, 0);
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(8080);
	
	bind(sock_server, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	listen(sock_server, 1); // 1 connection in the queue

	for(;;)	{
		sock_client = accept(sock_server, (struct sockaddr*)&client_addr, (socklen_t*)&c);
		
		while( (read_size = recv(sock_client, client_message, 10, 0)) > 0) {
			if(client_message[0] == 'F') {
				GPIO_OUTPUT_SET(MOTOR_RIGHT_DIRECTION, 1);
				GPIO_OUTPUT_SET(MOTOR_LEFT_DIRECTION, 1);
				enable_motors();
				vTaskDelay(100);
				disable_motors();
			} else if(client_message[0] == 'B') {
				GPIO_OUTPUT_SET(MOTOR_RIGHT_DIRECTION, 0);
				GPIO_OUTPUT_SET(MOTOR_LEFT_DIRECTION, 0);
				enable_motors();
				vTaskDelay(100);
				disable_motors();
			} else if(client_message[0] == 'L') {
				GPIO_OUTPUT_SET(MOTOR_RIGHT_DIRECTION, 1);
				GPIO_OUTPUT_SET(MOTOR_LEFT_DIRECTION, 0);
				enable_motors();
				vTaskDelay(100);
				disable_motors();
			} else if(client_message[0] == 'R') {
				GPIO_OUTPUT_SET(MOTOR_RIGHT_DIRECTION, 0);
				GPIO_OUTPUT_SET(MOTOR_LEFT_DIRECTION, 1);
				enable_motors();
				vTaskDelay(100);
				disable_motors();
			}
		} 
		vTaskDelay(xDelay);
	}
}

void enable_motors(){
	GPIO_OUTPUT_SET(MOTOR_RIGHT_SPEED, 1);
	GPIO_OUTPUT_SET(MOTOR_LEFT_SPEED, 1);
}

void disable_motors(){
	GPIO_OUTPUT_SET(MOTOR_RIGHT_SPEED, 0);
	GPIO_OUTPUT_SET(MOTOR_LEFT_SPEED, 0);
}

/*
 * This is entry point for user code
 */
void ICACHE_FLASH_ATTR
user_init(void)
{
	xTaskCreate(blinky, "bl", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}


// vim: ts=4 sw=4 noexpandtab
