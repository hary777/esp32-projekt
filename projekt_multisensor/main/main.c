
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

#define MQTT_BROOKER_IP		"10.42.0.1"//"192.168.88.7"
#define DHT11_IN_NUM		GPIO_NUM_17
#define MOTION_IN_SEL		GPIO_SEL_12
#define MOTION_IN_NUM		GPIO_NUM_12

#define WAKEUP_TIMER_US		60000000


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

//message TAG
static const char *TAG = "projekt_multisensor";

//MQTT client handle global var
static esp_mqtt_client_handle_t mqtt_client_handle = NULL;


//declarations
static void init_mqtt_client(void);
static void multisensor_app(void* pvParameter);



static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch(event->event_id) {
	case SYSTEM_EVENT_STA_START:
		esp_wifi_connect();
		break;
	case SYSTEM_EVENT_STA_GOT_IP:
		xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		ESP_LOGI(TAG, "WIFI_GOT_IP_EVENT, Start MQTT client");
		init_mqtt_client();
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		/* This is a workaround as ESP32 WiFi libs don't currently
			auto-reassociate. */

		//not reasociate in this small time
		//esp_wifi_connect();
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
	switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            //start task after connect
            xTaskCreate( &multisensor_app, "MULTISENSOR_APP", 65535, NULL, 5, NULL );
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
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

static void init_mqtt_client(void)
{
	if(mqtt_client_handle == NULL){
		const esp_mqtt_client_config_t mqtt_cfg = {
			//.uri = "mqtt://iot.eclipse.org",
			.host = MQTT_BROOKER_IP,
			.event_handle = mqtt_event_handler,
		};
		mqtt_client_handle = esp_mqtt_client_init(&mqtt_cfg);
	}
	else{
		esp_mqtt_client_start(mqtt_client_handle);
	}
}

static void multisensor_app_sendtemp(void)
{
	dht11_data_t sensor_data;
	char tmp[10];
	int len;
	dht11_errorHandle( dht11_getData(&sensor_data) );

	len = sprintf(tmp, "%d", sensor_data.temperature);
	esp_mqtt_client_publish(mqtt_client_handle, "home/room/temp", tmp, len, 0, 0);

	len = sprintf(tmp, "%d", sensor_data.humidity);
	esp_mqtt_client_publish(mqtt_client_handle, "home/room/hum", tmp, len, 0, 0);

	printf("Temp: %d°C\n", sensor_data.temperature );
	printf("Hum:  %d%%\n", sensor_data.humidity );
}

static void multisensor_app(void* pvParameter)
{
	ESP_LOGI(TAG, "MULTISENSOR_APP_TASK_START");

	char tmp[3];
	int len;
	int motion_sensor;
	esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();




	if(reason == ESP_SLEEP_WAKEUP_EXT1){
		do{
			//send temp and humidity
			multisensor_app_sendtemp();

			//send motion 1 for 5 sec
			esp_mqtt_client_publish(mqtt_client_handle, "home/room/motion", "1", 1, 0, 0);
			vTaskDelay(5000 / portTICK_PERIOD_MS);

			motion_sensor = gpio_get_level(MOTION_IN_NUM);
		}
		while(motion_sensor);
		//motion 0
		esp_mqtt_client_publish(mqtt_client_handle, "home/room/motion", "0", 1, 0, 0);
	}

	else{
		//timer wake up or other reason, on start and any not usual state

		//send temp and humidity
		multisensor_app_sendtemp();

		//check motion, might be 0
		motion_sensor = gpio_get_level(MOTION_IN_NUM);
		len = sprintf(tmp, "%d", motion_sensor);
		esp_mqtt_client_publish(mqtt_client_handle, "home/room/motion", tmp, len, 0, 0);
	}



	//stop mqtt
	esp_mqtt_client_destroy(mqtt_client_handle)

	//wait for TCP FIN-ACK
	vTaskDelay(100 / portTICK_PERIOD_MS);

	//stop wifi
	esp_wifi_disconnect();
	esp_wifi_stop();

	//set timer
	esp_sleep_enable_timer_wakeup(WAKEUP_TIMER_US);

	//set wake interupt
	esp_sleep_enable_ext1_wakeup(MOTION_IN_SEL, ESP_EXT1_WAKEUP_ANY_HIGH);

	//go sleep
	esp_deep_sleep_start();

}


void app_main()
{
	ESP_LOGI(TAG, "Starting again!");

	esp_log_level_set("*", ESP_LOG_INFO);
	esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
	esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
	esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

	//set blue led pin
	gpio_config_t gpio_cfg = {
		.pin_bit_mask = GPIO_SEL_16,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK( gpio_config(&gpio_cfg) );
	ESP_ERROR_CHECK( gpio_set_level(GPIO_NUM_16,0) ); //on

	//set motion sensor pin
	gpio_config_t gpio_cfg2 = {
		.pin_bit_mask = MOTION_IN_SEL,
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	ESP_ERROR_CHECK( gpio_config(&gpio_cfg2) );

	//set DHT11 pin
    dht11_setPin(DHT11_IN_NUM);


    //init nvs memory for wifi
	nvs_flash_init();

	//init MQTT client
	init_mqtt_client();

	//init and start wifi
	initialise_wifi();

}
