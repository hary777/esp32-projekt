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

#ifndef DHT11_H_
#define DHT11_H_


#define DHT11_TIMEOUT_ERROR -2
#define DHT11_CHECKSUM_ERROR -1
#define DHT11_OKAY  0


typedef struct{
	uint8_t humidity;
	uint8_t temperature;
} dht11_data_t;



// function prototypes

//Set GPIO pin, default GPIO_NUM_4
void dht11_setPin(gpio_num_t pin);

//To get all measurements
int dht11_getData(dht11_data_t *data);

//Debug, print errors
void dht11_errorHandle(int response);




#endif
