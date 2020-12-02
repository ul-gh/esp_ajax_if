# -*- coding: utf-8 -*-
import time
from threading import Timer

import requests

timestamp = [0.0]

url = "http://isocal.lan/cmd"

cmd1 = {"set_power_pwm_active": "true"}
cmd2 = {"set_power_pwm_active": "false"}

def stop_measurement():
    print(f"Stop after seconds: {time.time() - timestamp[0]}")
    requests.get(url, cmd2)


def do_measurement(time_sec=3):
    timer = Timer(time_sec, stop_measurement)
    timestamp[0] = time.time()
    print("Start!")
    requests.get(url, cmd1)
    timer.start()
