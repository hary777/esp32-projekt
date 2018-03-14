
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "driver/gpio.h"

#include "mqtt_client.h"

#include "dht11.h"



/* The examples use simple WiFi configuration that you can set via
#define WIFI_SSID "mywifissid"
*/
#define WIFI_SSID "badwifi"
#define WIFI_PASS "hesloheslo999"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;


static const char *TAG = "projekt_multisensor";



static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch(event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
			auto-reassociate. */
		esp_wifi_connect();
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		break;
	default:
		break;
	}
	return ESP_OK;
}

static void initialise_wifi(void)
{
	tcpip_adapter_init();
	wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
	ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
	wifi_config_t wifi_config = {
		.sta = {
			.ssid = WIFI_SSID,
			.password = WIFI_PASS,
		},
	};
	ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
	ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK( esp_wifi_start() );

	ESP_LOGI(TAG, "Enable modem sleep");
	esp_wifi_set_ps( WIFI_PS_MODEM );
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        //.uri = "mqtt://iot.eclipse.org",
        .host = "192.168.88.7",
        .event_handle = mqtt_event_handler,
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);



    dht11_setPin(GPIO_NUM_17);
	printf("Starting DHT measurement!\n");
	dht11_data_t sensor_data;



    gpio_config_t gpio_cfg = {
		.pin_bit_mask = GPIO_SEL_16,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK( gpio_config(&gpio_cfg) );

	gpio_config_t gpio_cfg2 = {
		.pin_bit_mask = GPIO_SEL_21,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK( gpio_config(&gpio_cfg2) );





	int cnt=0;
    while(1){
    	vTaskDelay(5000 / portTICK_PERIOD_MS);

		printf("%s\n","posilam teplotu");
    	esp_mqtt_client_publish(client, "home/room/test", "20", 2, 0, 0);

    	char tmp[10];
    	int len;
		dht11_errorHandle( dht11_getData(&sensor_data) );

		len = sprintf(tmp, "%d", sensor_data.temperature);
		esp_mqtt_client_publish(client, "home/room/temp", tmp, len, 0, 0);

		len = sprintf(tmp, "%d", sensor_data.humidity);
		esp_mqtt_client_publish(client, "home/room/hum", tmp, len, 0, 0);

		printf("Temp: %d°C\n", sensor_data.temperature );
		printf("Hum:  %d%%\n", sensor_data.humidity );


		printf("Motion: %d\n",gpio_get_level(GPIO_NUM_21) );


    	if(cnt){
			ESP_ERROR_CHECK( gpio_set_level(GPIO_NUM_16,0) );
			//ESP_ERROR_CHECK( gpio_set_level(GPIO_NUM_17,1) );
			cnt = 0;
    	}
		else{
			ESP_ERROR_CHECK( gpio_set_level(GPIO_NUM_16,1) );
			//ESP_ERROR_CHECK( gpio_set_level(GPIO_NUM_17,0) );
			cnt = 1;
		}
	}





}

void DHT_task(void *pvParameter)
{
	dht11_setPin(GPIO_NUM_17);
	printf("Starting DHT measurement!\n");

	dht11_data_t sensor_data;

	while(1){
		dht11_errorHandle( dht11_getData(&sensor_data) );

		printf("Temp: %d°C\n", sensor_data.temperature );
		printf("Hum:  %d%%\n", sensor_data.humidity );

		vTaskDelay(3000 / portTICK_RATE_MS);
	}
}

void app_main()
{
	nvs_flash_init();
	initialise_wifi();
	//xTaskCreate(&http_get_task, "http_get_task", 2048, NULL, 5, NULL);
	ESP_LOGI(TAG, "Starting again!");

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);





	vTaskDelay( 5000 / portTICK_RATE_MS );
	//xTaskCreate( &DHT_task, "DHT_task", 2048, NULL, 5, NULL );

	mqtt_app_start();

}
