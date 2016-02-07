#include "httpd.h"
#include "fs.h"
#include "fsdata.h"
#include "esp_common.h"
#include "lwip/tcp.h"


void user_init(void) {
    lwip_init();
    httpd_init();
    
    wifi_set_opmode(STATIONAP_MODE);

    wifi_station_set_auto_connect(1);
}