#include "httpd.h"
#include "fs.h"
#include "fsdata.h"
#include "esp_common.h"
#include "lwip/tcp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "uart.h"

#define GET_LEN 128
#define HOST_LEN 80
typedef struct {
    char get_cmd[GET_LEN+1];
    char host[HOST_LEN+1];
    int16_t port;
    uint32_t valid;
} user_config_t;

static user_config_t user_config;
static uint8_t wifi_status=0;
static xSemaphoreHandle ledSemaphore;
static xQueueHandle xUARTQueue;
#define BUFSIZE 200
#define DBG printf
#define TASK_DELAY_MS(m) vTaskDelay(m/portTICK_RATE_MS)
#define PRIV_PARAM_START_SEC            0x7B
#define CONFIG_VALID 0xDEADBEEF


static ICACHE_FLASH_ATTR int
save_user_config(user_config_t *config)
{
    config->valid=CONFIG_VALID;
    spi_flash_erase_sector(PRIV_PARAM_START_SEC);
    spi_flash_write((PRIV_PARAM_START_SEC) * SPI_FLASH_SEC_SIZE,
                        (uint32 *)config, sizeof(user_config_t));
    return 0;
}


static ICACHE_FLASH_ATTR int
read_user_config(user_config_t *config)
{
    DBG ("size to read is %d\n\r", sizeof(user_config_t));
    spi_flash_read((PRIV_PARAM_START_SEC) * SPI_FLASH_SEC_SIZE,
                        (uint32 *)config, sizeof(user_config_t));
    DBG ("config->valid=0x%04x\r\n", config->valid);
    if (config->valid != CONFIG_VALID)
    {
        config->get_cmd[0] = '\0';
        config->host[0] = '\0';
        config->port = 80;
        return -1;
    }
    return 0;
}



static ICACHE_FLASH_ATTR int
configure(void)
{
    char ch;
    int ret; 
    static struct station_config st_config;
    wifi_station_get_config(&st_config);
    do
    {
        printf("\r\nConfiguration:\r\n");
        printf("1: Wifi SSID [%s]\r\n", st_config.ssid);
        printf("2: Wifi Password [%s]\r\n", st_config.password);
        printf("3: HTTP host[%s]\r\n", user_config.host);
        printf("4: HTTP port[%d]\r\n", user_config.port);
        printf("5: HTTP path/query [%s]\r\n", user_config.get_cmd);
        printf("0: Exit configuration\r\n");
        // ch = uart_getchar();
        xQueueReceive(xUARTQueue, &ch, -1);
        switch (ch)
        {
        case '1':
            printf("Enter Wifi SSID: ");
            uart_gets(&xUARTQueue, st_config.ssid, 32);
            break;
        case '2':
            printf("Enter Wifi Password: ");
            uart_gets(&xUARTQueue, st_config.password, 64);
            break;
        case '3':
            printf("Enter HTTP host: ");
            uart_gets(&xUARTQueue, user_config.host, HOST_LEN+1);
            break;
        case '4':
            printf("Enter HTTP port: ");
            char buf[6];
            uart_gets(&xUARTQueue, buf, 6);
            user_config.port = atoi(buf);
            break;
        case '5':
            printf("Enter HTTP path and query string: ");
            uart_gets(&xUARTQueue, user_config.get_cmd, GET_LEN+1);
            break;
        case '0':
            DBG("setting config [%s] [%s]\n", st_config.ssid, st_config.password);
            wifi_station_set_config(&st_config);
            save_user_config(&user_config);
            break;
        default:
            printf("Invalid choice\r\n");
        }
    } while (ch != '0');
    printf("System will now restart\r\n");
    system_restart();
}

static ICACHE_FLASH_ATTR void
check_input(void *pvParameters)
{
    char ch;
    xQueueHandle xUARTQueue = *(xQueueHandle *)pvParameters;
    
    for(;;)
    {
        wifi_status = wifi_station_get_connect_status();
        if (wifi_status == STATION_GOT_IP)
        {
            printf("Hit 'c' to configure, 't' to test\r\n");
        }
        else
        {
            printf("Hit 'c' to configure\r\n");
        }
        // ch = uart_getchar(); // wait forever
        xQueueReceive(xUARTQueue, &ch, -1);
        // wifi_station_set_auto_connect(0);
        // printf("lololol");
        switch(ch)
        {
        case 'c':
        case 'C':
            configure();
            break;
        case 't':
        case 'T':
            if (wifi_status == STATION_GOT_IP) 
            {
                printf("connected!\n");
                    // if(send_command())
                    // {
                    //     printf("Send command failed!\r\n");
                    // }
            }
            else
            {
                printf("Not connected\r\n");
                TASK_DELAY_MS(500);
            }
            break;
        case 0: // This shouldn't happen?
            printf("Error reading serial data\r\n");
            break;
        default:
            printf("Invalid key %d %c\r\n", ch, ch);
            // uart_rx_flush();
            TASK_DELAY_MS(500);
        }
    }
}



void user_init(void) {
    bool ret;
    lwip_init();
    httpd_init();

    xUARTQueue = xQueueCreate(128, sizeof(char));
    uart_init_new();
    UART_intr_handler_register(&uart0_rx_intr_handler, &xUARTQueue);
    ETS_UART_INTR_ENABLE();
    
    wifi_set_opmode(STATIONAP_MODE);

   wifi_station_set_auto_connect(0);
    // wifi_station_set_reconnect_policy(0);

    printf("Wifi Button example program. \r\n");
    if (!read_user_config(&user_config))
    {
        ret = wifi_set_opmode(STATIONAP_MODE);
        DBG("wifi_set_opmode returns %d op_mode now %d\r\n", ret, wifi_get_opmode());
        wifi_station_set_auto_connect(1);
    }
    else
    {
        printf ("No valid config\r\n");
    }

    xTaskCreate(check_input, "input", 256, &xUARTQueue, 3, NULL);

}