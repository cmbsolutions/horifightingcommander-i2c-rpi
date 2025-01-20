# Hori I2C Wireless Fighting Commander for Raspberry Pi
Allows connecting the 12 button wireless Hori Fighting Commander directly via the I2C bus on the GPIO header of the Raspberry Pi.

## Connecting the controller
Plug side view:
```
/---------\
|         |
|  1-2-3  |
|  4-5-6  |
|  _____  |
\_/     \_/
```
| Controller Pin | Function      | Pi Pin    |
|---------------:|---------------|-----------|
| 1              | I2C data      | 3 (SDA)   |
| 2              | device detect | -         |
| 3              | +3.3V         | 1 (+3.3V) |
| 4              | GND           | 6 (GND)   |
| 5              | not connected | -         |
| 6              | I2C clock     | 5 (SCL)   |


## Enabling i2c on Raspberry Pi
This can be done with the bundled configuration tool `raspi-config` under "Interfacing Options" > "I2C".
If you want to do it manually, it comes down to:
* Adding `dtparam=i2c_arm=on` line to `/boot/config.txt`
* Loading `i2c_dev` module (`sudo modprobe i2c_dev` for the current session and adding `i2c_dev` line to `/etc/modules` to load on boot)

## Installing required packages

```
sudo apt-get install build-essential cmake libi2c-dev i2c-tools
```

## Testing the connection

When properly connected the controller should be now visible at address 0x52 and all other addresses should
be empty.
```
$ sudo i2cdetect -y 1
     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
00:          -- -- -- -- -- -- -- -- -- -- -- -- -- 
10: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
20: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
30: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
40: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
50: -- -- 52 -- -- -- -- -- -- -- -- -- -- -- -- -- 
60: -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- 
70: -- -- -- -- -- -- -- -- 
```

## Compiling the driver
```
cmake .
make
```
## Testing the driver

```
sudo ./i2c-hori-controller -d
```
If the controller is correctly connected it should display the button events:
```
Detected device: 01 00 A4 20 01 01
buttons:    A           
```
## Driver options
* `-y <n>` by default the driver is using `/dev/i2c-1` for the i2c bus connection, which corresponds to the bus
exposed on Pi pins 3/5. This option allows using other buses.
* `-d` print controller events to the standard output

## Installing

```
sudo make install
sudo systemctl enable hori-i2c-controller
```
This will install a systemd service which will run the driver on startup. Additionally a udev rule will be set
to remove `ID_INPUT_KEY` property from the uinput node which prevents some software (specifically Kodi) to detect
the controller as a keyboard.

## Connecting more than one controller

All Hori fighting commander wireless i2c controllers use the same static i2c address which means only one can be connected to an i2c bus at a time.
I have not yet found a way to add a second controller to the same bus. If you have any ideas, please let me know.