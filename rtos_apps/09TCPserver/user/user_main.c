// #include "esp8266/ets_sys.h"
// #include "osapi.h"
#include "user_config.h"
#include "espconn.h"
#include "uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "esp_common.h"
// #include "stdint.h"
// #include "c_types.h"

#define LED_GPIO 2
#define LED_GPIO_MUX PERIPHS_IO_MUX_GPIO2_U
#define LED_GPIO_FUNC FUNC_GPIO2

// This was defined in the old SDK.
#ifndef GPIO_OUTPUT_SET
#define GPIO_OUTPUT_SET(gpio_no, bit_value) \
    gpio_output_set(bit_value<<gpio_no, ((~bit_value)&0x01)<<gpio_no, 1<<gpio_no,0)
#endif


struct espconn esp_conn;
LOCAL esp_tcp esptcp;

static struct ip_info ipconfig;

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


#define SERVER_LOCAL_PORT   1112

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

/******************************************************************************
 * FunctionName : tcp_server_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_sent_cb(void *arg)
{
   //data sent successfully

    printf("tcp sent cb \r\n");
}


/******************************************************************************
 * FunctionName : tcp_server_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
   //received some data from tcp connection
   
   struct espconn *pespconn = arg;
   printf("tcp recv : %s \r\n", pusrdata);
   
   printf("%d\n",espconn_send(pespconn, pusrdata, length));
   
}

/******************************************************************************
 * FunctionName : tcp_server_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_discon_cb(void *arg)
{
   //tcp disconnect successfully
   
    printf("tcp disconnect succeed !!! \r\n");
}

/******************************************************************************
 * FunctionName : tcp_server_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_recon_cb(void *arg, sint8 err)
{
   //error occured , tcp connection broke.
   
    printf("reconnect callback, error code %d !!! \r\n",err);
}

LOCAL void tcp_server_multi_send(void* arg)
{
   struct espconn *pesp_conn = &esp_conn;
    // struct espconn *pesp_conn = arg;

   remot_info *premot = NULL;
   uint8 count = 0;
   sint8 value = ESPCONN_OK;
   if (espconn_get_connection_info(pesp_conn,&premot,0) == ESPCONN_OK){
      char *pbuf = "tcp_server_multi_send\n";
      for (count = 0; count < pesp_conn->link_cnt; count ++){
         pesp_conn->proto.tcp->remote_port = premot[count].remote_port;
         
         pesp_conn->proto.tcp->remote_ip[0] = premot[count].remote_ip[0];
         pesp_conn->proto.tcp->remote_ip[1] = premot[count].remote_ip[1];
         pesp_conn->proto.tcp->remote_ip[2] = premot[count].remote_ip[2];
         pesp_conn->proto.tcp->remote_ip[3] = premot[count].remote_ip[3];

         printf("%d", espconn_send(pesp_conn, pbuf, strlen(pbuf)));
         printf("multi %d\n", count);
      }
   }
}


/******************************************************************************
 * FunctionName : tcp_server_listen
 * Description  : TCP server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
tcp_server_listen(void *arg)
{
    struct espconn *pesp_conn = arg;
    printf("tcp_server_listen !!! \r\n");

    printf("%d\n",espconn_set_opt(arg, ESPCONN_NODELAY));

    espconn_regist_recvcb(pesp_conn, tcp_server_recv_cb);
    espconn_regist_reconcb(pesp_conn, tcp_server_recon_cb);
    espconn_regist_disconcb(pesp_conn, tcp_server_discon_cb);

   
    printf("%d\n", espconn_regist_sentcb(pesp_conn, tcp_server_sent_cb));
   tcp_server_multi_send(arg);
}

/******************************************************************************
 * FunctionName : user_tcpserver_init
 * Description  : parameter initialize as a TCP server
 * Parameters   : port -- server port
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_tcpserver_init(uint32 port)
{
    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    espconn_regist_connectcb(&esp_conn, tcp_server_listen);

    sint8 ret = espconn_accept(&esp_conn);
    // espconn_regist_time(&esp_conn, 20, 0);
   
    printf("espconn_accept [%d] !!! \r\n", ret);

}
LOCAL os_timer_t test_timer;

/******************************************************************************
 * FunctionName : user_esp_platform_check_ip
 * Description  : check whether get ip addr or not
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_esp_platform_check_ip(void)
{
    struct ip_info ipconfig;

   //disarm timer first
    os_timer_disarm(&test_timer);

   //get ip info of ESP8266 station
    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {

      printf("got ip !!! \r\n");
      user_tcpserver_init(SERVER_LOCAL_PORT);
      GPIO_OUTPUT_SET(LED_GPIO, 1);
      

    } else {
       
        if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
                wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
                wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) {
               
         printf("connect fail !!! \r\n");
         
        } else {
       
           //re-arm timer to check ip
            os_timer_setfn(&test_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
            os_timer_arm(&test_timer, 100, 0);
        }
    }
}

/******************************************************************************
 * FunctionName : user_set_station_config
 * Description  : set the router info which ESP8266 station will connect to
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_set_station_config(void)
{
  int i;
   // Wifi configuration
   char ssid[32] = SSID;
   char password[64] = PASSWORD;
   struct station_config stationConf;

   //need not mac address
   stationConf.bssid_set = 0;
   
   //Set ap settings
   // memcpy(&stationConf.ssid, ssid, 32);
   // memcpy(&stationConf.password, password, 64);
   for (i = 0; i < strlen(ssid); i++) {
        stationConf.ssid[i] = ssid[i];
   }
   for (i = 0; i < strlen(password); i++) {
        stationConf.password[i] = password[i];
   }
   // stationConf.ssid = ssid;
   // stationConf.password = password;
   wifi_station_set_config(&stationConf);

   //set a timer to check whether got ip from router succeed or not.
   os_timer_disarm(&test_timer);
   os_timer_setfn(&test_timer, (os_timer_func_t *)user_esp_platform_check_ip, NULL);
   os_timer_arm(&test_timer, 100, 0);

}

/*
 * this task will print the message
 */
void helloworld(void *pvParameters)
{
  PIN_FUNC_SELECT(LED_GPIO_MUX, LED_GPIO_FUNC);
  const portTickType xDelay = 1000 / portTICK_RATE_MS;
  for(;;)
  {
    GPIO_OUTPUT_SET(LED_GPIO, 0);
    printf("Hello World!\n");
    vTaskDelay( xDelay);
  }
}

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

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
    bool ret;
    PIN_FUNC_SELECT(LED_GPIO_MUX, LED_GPIO_FUNC);
    xUARTQueue = xQueueCreate(128, sizeof(char));
    uart_init_new();
    UART_intr_handler_register(&uart0_rx_intr_handler, &xUARTQueue);
    ETS_UART_INTR_ENABLE();
    printf("SDK version:%s\n", system_get_sdk_version());
    GPIO_OUTPUT_SET(LED_GPIO, 0);

   
   //Set  station mode
   wifi_set_opmode(STATIONAP_MODE);

   // ESP8266 connect to router.
   // user_set_station_config();

   // Setup TCP server
   


   wifi_station_set_auto_connect(0);
    // wifi_station_set_reconnect_policy(0);

    printf("Wifi Button example program. \r\n");
    if (!read_user_config(&user_config))
    {
        ret = wifi_set_opmode(STATIONAP_MODE);
        DBG("wifi_set_opmode returns %d op_mode now %d\r\n", ret, wifi_get_opmode());
        // user_set_station_config();
        user_tcpserver_init(SERVER_LOCAL_PORT);
        // user_tcpserver_init(SERVER_LOCAL_PORT);
        wifi_station_set_auto_connect(1);
    }
    else
    {
        printf ("No valid config\r\n");
    }

   // xTaskCreate(check_input, "input", 256, &xUARTQueue, 3, NULL);

   // xTaskCreate(helloworld, "hw", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
   // xTaskCreate(blinky, "bl", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
}
