# PimaEspHome

This program implements an interface for negotiation with PIMA Hunter Pro alarms.
It was built based on PIMA™'s General Specification for Home Automation & Building Management protocol Ver. 1.15.
PIMA™ is a trademark of PIMA Electronic Systems Ltd, http://www.pima-alarms.com.
This program was built with no affiliation of PIMA Electronic Systems Ltd.

Meant for PIMA Hunter Pro alarm™, with 32, 96 - Will not work with 144 zones.
Run on ESP8266 boards - Tested on diymore ESP8266 ESP-07 board
(https://www.aliexpress.com/item/32995506222.html?spm=a2g0s.12269583.0.0.18c66a88PQJJho)

Need to update the code itself (h and yaml files) to correspond the specific alarm implementation.
Compile directly in Home Assistant + ESPHome
1. File "esphome_Pima.h" , function send_login_message - update the technician code (length and the code itself)
2. File "Pima.yaml" 
   a. update all the standart EspHome stuff like wifi, mqtt server ...
   b. update the actual sensors you have in you alarm system - there is a remark and look at the example in the current yaml file
   
No real documentation for now 

This is a rough implementation, stability is not guaranteed
A lot of stuff was hardcoded and should be fixed for next version

Based on projects :
https://github.com/deiger/Alarm by deiger
https://github.com/Margriko/Paradox-ESPHome by Margriko
