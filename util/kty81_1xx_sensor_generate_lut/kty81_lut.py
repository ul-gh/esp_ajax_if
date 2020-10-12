"""
Generate a look-up-table for temperature values of a KTY81-type silicon
sensor circuit based on a number of equidistant voltage steps.

The sensor circuit is a single pull-up-resistor connecting the sensor to VDD.
The other sensor lead is connected to GND. Middle node connected to ADC input.

There are two versions, the KTY81-110 and -120 types have equal nominal
resistance values. The KTY81-121 has non-symmetric tolerances and slightly
different nominal values.

 +-----------------------+
 |                       |
+++                      |VREF
| |r_pullup              |(3V3)
| |(2k2)             +----------+
+++                  |          |
 |                   |          |
 +-------------+-----+ AIN      |
 |             |     |          |
+++            |     +---+------+
| |KTY81-    +---+       | AGND
| | 1xx      +---+       |
+++            |100nF    |
 |             |         |
 +-------------+---------+
"""
import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import interp1d


#################### Circuit configuration
r_circuit_pullup_ohms = 2200
vdd_circuit_millivolts = 3300
# Temperatures in °C for which the datasheet lists nominal resistance values
temps_datasheet = [-55, -50, -40, -30, -20,
                   -10, 0, 10, 20, 25,
                   30, 40, 50, 60, 70,
                   80, 90, 100, 110, 120,
                   125, 130, 140, 150]


###################### Calculation for KTY81_110 and KTY81_120
# Nominal resistance values in ohms
r_x_kty81_110_120 = [490, 515, 567, 624, 684,
                     747, 815, 886, 961, 1000,
                     1040, 1122, 1209, 1299, 1392,
                     1490, 1591, 1696, 1805, 1915,
                     1970, 2023, 2124, 2211]
v_x = [vdd_circuit_millivolts * r_x / (r_circuit_pullup_ohms + r_x)
       for r_x in r_x_kty81_110_120]
f_interp_kty81_110_120 = interp1d(v_x, temps_datasheet, "cubic")
v_x_lin = np.linspace(v_x[0], v_x[-1], num=32)
################### RESULT: temperature look-up table for 31 equal voltage steps
lut_temps_kty81_110_120 = f_interp_kty81_110_120(v_x_lin)


####################### Calculation for KTY81_121
# Nominal resistance values in ohms
r_x_kty81_121 = [485, 510, 562, 617, 677,
                 740, 807, 877, 951, 990,
                 1029, 1111, 1196, 1286, 1378,
                 1475, 1575, 1679, 1786, 1896,
                 1950, 2003, 2103, 2189]
v_x = [vdd_circuit_millivolts * r_x / (r_circuit_pullup_ohms + r_x)
       for r_x in r_x_kty81_121]
f_interp_kty81_121 = interp1d(v_x, temps_datasheet, "cubic")
v_x_lin = np.linspace(v_x[0], v_x[-1], num=32)
################### RESULT: temperature look-up table for 31 equal voltage steps
lut_temps_kty81_121 = f_interp_kty81_121(v_x_lin)