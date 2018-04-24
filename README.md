

# ESP32 multisensor

## Used libs

 - compilators and manulals: http://esp-idf.readthedocs.io/en/latest/get-started/index.html#setup-toolchain
 - framework: https://github.com/espressif/esp-idf
 - MQTT client: https://github.com/tuanpmt/espmqtt
 - DHT11 (modified): https://github.com/lemonhall/esp32_dht11


## Setup

Tested on Ubuntu 16.04

Install dependencies
```
sudo apt-get install gcc git wget make libncurses-dev flex bison gperf python python-serial
```

Clone this repository to your $HOME directory
```
git clone https://github.com/hary777/esp32-projekt.git
```

Run script `./nastaveni_prostredi.sh`
This setup $PATH variable for compiler and IDF framework.
This is not permanent, setup only actual terminal.

Go to `cd projekt_multisensor` and run `./run.sh`
This compile app, flash to ESP32 device and launch serial monitor.

Wifi setup is in macros in file `main.c` and IP or DNS of MQTT broker.


##Wire connection

ESP32 











