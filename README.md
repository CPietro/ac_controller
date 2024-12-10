# ESP32 based IR AC Controller 

This project is coursework for the Embedded Systems undergraduate course @ UNIMI.  
It is designed to control AC units unidirectionally using an IR LED. Its functionality includes turning the unit on and off, setting the target room temperature, adjusting the fan speed, and selecting the operating mode. These features are accessed by navigating a menu with an encoder on a 1602 LCD.  
The controller also includes an ambient temperature sensor, which enables protective heating or cooling when the room temperature drops below 10°C or rises above 30°C.  
Additionally, it is equipped with an mmWave presence sensor (LD2410C) for automatic air conditioning based on whether people are present in the room.


## Libraries

This project makes use of the following libraries:
- [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) by David Conran, under [LGPL-2.1](LGPL-2.1) license.
- [NewEncoder](https://github.com/gfvalvo/NewEncoder) by _gfvalvo_, under [LGPL-2.1](LGPL-2.1) license.
- [DHT-sensor-library](https://github.com/adafruit/DHT-sensor-library) by Adafruit Industries, under [MIT](MIT) license.
- [Unified Sensor Driver](https://github.com/adafruit/Adafruit_Sensor) by Adafruit Industries, under [Apache 2.0](APACHE-2.0) license.
- [LiquidCrystal_I2C](https://github.com/johnrickman/LiquidCrystal_I2C) by John Rickman.