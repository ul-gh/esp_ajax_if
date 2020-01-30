# -*- coding: utf-8 -*-

import requests
import time

url = "http://192.168.4.1/cmd"

cmd1 = {"set_frequency": 500}
cmd2 = {"set_frequency": 100}

def sequence(delay=2):
    response = requests.get(url, cmd1)
    print(response)
    time.sleep(delay)
    response = requests.get(url, cmd2)
    print(response)
    time.sleep(delay)

def loop(delay=2):
    while True:
        sequence(delay)

