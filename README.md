# SensorTag project for sending and receiving morse code.

## Instructions:
- Connect the device to the computer with usb cable
  - if the red LED lights up wait for it turn off
- Pressing the button 0 will set the device into reading mode where it reads movements
  - Turning the device to left will send "." via UART and turning right will send "-".
  - Button 1 will send " " via UART
- Device will automatically read any data send via UART and beep the received morse code
### Device in reading mode:
![pics/Sensortag_interface.jpg](pics/Sensortag_reading.png)

### Device in reading mode:
![pics/Sensortag_interface.jpg](pics/Sensortag_receiving.png)

Computer Systems Course University of Oulu 2024