# -*- coding: utf-8 -*-
import time
from threading import Timer

timestamp = [0.0]

def stop_measurement():
    print(f"Stop after seconds: {time.time() - timestamp[0]}")


def do_measurement(time_sec=3):
    timer = Timer(time_sec, stop_measurement)
    timestamp[0] = time.time()
    print("Start!")
    timer.start()
