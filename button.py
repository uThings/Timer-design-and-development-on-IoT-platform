import time
from pyA20.gpio import gpio
from pyA20.gpio import port
import keyboard

gpio.init()

gpio.setcfg(port.PA6, gpio.INPUT)

gpio.pullup(port.PA6, gpio.PULLDOWN)

while True:
	if gpio.input(port.PA6) == 1:
		keyboard.press_and_release('a')
		time.sleep(0.1)
	time.sleep(0.1)
