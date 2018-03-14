/* DHT11 temperature sensor library
   Usage:
   		Set DHT PIN using  setDHTPin(pin) command
   		getFtemp(); this returns temperature in F
   Sam Johnston
   October 2016
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/*modified
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "dht11.h"



gpio_num_t dht_pin = GPIO_NUM_4; //default pin


void dht11_setPin(gpio_num_t pin)
{
	dht_pin = pin;
}

void dht11_errorHandle(int response)
{
	switch(response){
		case DHT11_TIMEOUT_ERROR :
			printf("DHT11 Sensor Timeout!\n");
			break;
		case DHT11_CHECKSUM_ERROR:
			printf("CheckSum error!\n");
			break;
		case DHT11_OKAY:
			break;
		default :
			printf("Dont know how you got here!\n");
	}
}

static void sendStart()
{
	//Send start signal from ESP32 to DHT device
	gpio_set_direction(dht_pin, GPIO_MODE_OUTPUT);
	gpio_set_level(dht_pin,0);
	ets_delay_us(22000);
	gpio_set_level(dht_pin,1);
	ets_delay_us(43);
	gpio_set_direction(dht_pin, GPIO_MODE_INPUT);
}

static int getData(dht11_data_t *data)
{
	//Variables used in this function
	int counter = 0; //count us
	uint8_t bits[5]; //40 bits data
	uint8_t byteCounter = 0;
	uint8_t bitcnt = 7; //7 to 0

	//set all values to 0
	for(int i=0; i<5; i++)
	{
		bits[i] = 0;
	}

	//Start transfer
	sendStart();
	//Wait for a response from the DHT11 device
	//This requires waiting for 20-40 us
	counter = 0;

	while(gpio_get_level(dht_pin)==1){
		if(counter > 40){
			return DHT11_TIMEOUT_ERROR;
		}
		counter++;
		ets_delay_us(1);
	}
	//Now that the DHT has pulled the line low,
	//it will keep the line low for 80 us and then high for 80us
	//check to see if it keeps low
	counter = 0;
	while(gpio_get_level(dht_pin)==0){
		if(counter > 80){
			return DHT11_TIMEOUT_ERROR;
		}
		counter++;
		ets_delay_us(1);
	}
	counter = 0;
	while(gpio_get_level(dht_pin)==1){
		if(counter > 80){
			return DHT11_TIMEOUT_ERROR;
		}
		counter++;
		ets_delay_us(1);
	}
	// If no errors have occurred, it is time to read data
	//output data from the DHT11 is 40 bits.
	//Loop here until 40 bits have been read or a timeout occurs

	for(int i = 0; i < 40; i++){
		//int currentBit = 0;
		//starts new data transmission with 50us low signal
		counter = 0;
		while(gpio_get_level(dht_pin)==0){
			if(counter > 55){
				return DHT11_TIMEOUT_ERROR;
			}
			counter++;
			ets_delay_us(1);
		}

		//Now check to see if new data is a 0 or a 1
		counter = 0;
		while(gpio_get_level(dht_pin)==1){
			if(counter > 75){
				return DHT11_TIMEOUT_ERROR;
			}
			counter++;
			ets_delay_us(1);
		}
		//add the current reading to the output data
		//since all bits where set to 0 at the start of the loop, only looking for 1s
		//look for when count is greater than 40 - this allows for some margin of error
		if(counter > 40){
			bits[byteCounter] |= (1 << bitcnt);
		}
		//here are conditionals that work with the bit counters
		if(bitcnt == 0){
			bitcnt = 7;
			byteCounter++;
		}
		else{
			bitcnt--;
		}
	}
	//END transfer

	uint8_t sum = bits[0] + bits[1] + bits[2] + bits[3];
	if(bits[4] != sum){
		return DHT11_CHECKSUM_ERROR;
	}

	data->humidity = bits[0];
	data->temperature = bits[2];

	return DHT11_OKAY;
}

int dht11_getData(dht11_data_t *data)
{
	int ret;
	portMUX_TYPE mutex;
	mutex.owner = portMUX_FREE_VAL; //unlocked, lock owner
	mutex.count = 0; //unlocked, lock count

	//Start critical section
	//This disable interupts and task scheduler for this crtitical section,
	//lock to 1 of 2 cores, not resolve shared variables
	//To keep time precision of transfer
	vTaskEnterCritical(&mutex);
	ret = getData(data);
	vTaskExitCritical(&mutex);

	return ret;
}
