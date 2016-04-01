# vital-ppi-arduino-env

* Author: Lorenzo Bracco
* Summary: This is the VITAL PPI for Arduino Mega 2560 and DHT11 sensor (using WiFi Shield)
* Target Project: VITAL (<http://vital-iot.eu>)
* Source: <http://gitlab.atosresearch.eu/vital-iot/vital-ppi-arduino-env.git>

## System requirements

For this project you need:

* Git (<https://git-scm.com>)
* Arduino (<https://www.arduino.cc/en/Main/Software>)

Follow installation instructions of Git and Arduino software.

## Configure, Build and Run the Arduino PPI

1. Checkout the code from the repository:

        git clone http://gitlab.atosresearch.eu/vital-iot/vital-ppi-arduino-env.git

3. Open the sketch in Arduino and add possibly missing libraries (WiFi, DHT sensor library, Time).

4. Modify the configuration in the sketch: WiFi network parameters, listening port number and DDNS parameters.
5. Compile the sketch and upload the program to Arduino.

## Access the module

The Arduino PPI is available at the hostname given by the DDNS service and chosen port.

