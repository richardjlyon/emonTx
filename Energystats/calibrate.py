#! /usr/bin/env python
# -*- coding: UTF-8 -*-
"""
    Calibrate an instance of emoncms from the pulsepower input.
    PowerPulse is assumed to be definitive. The correlation between C1, C2 and PowerPulse is assumed to be a function of PowerPulse.
    Reads C1, C3 and PowerPulse over a defined period. Estimates the correlation factors to correct C1 and C3.

    ** NOTE: Assumes feeds have the same sample frequency **
"""
from emoncms.emoncms import EmonCMS
from thinkstats import thinkstats
from config import servers, tarrif
import datetime
from time import mktime

import matplotlib.pyplot as plt

# EmonTx calibration factors for converting CT readings to powers
FACTORS = {
    "C1": 111.1,
    "C2": 111.1,
    "C3": 111.1,
}

def totalPower(powers):
    """Returns the total power in tuple (dayPower, nightPower)"""

    day, night = powers
    return day + night

if __name__ == "__main__":

    e = EmonCMS(servers['raspberry'], tarrif=tarrif)

    # Select a period to calibrate over
    start_time = datetime.datetime(2013, 9, 21, 16, 50)
    end_time = datetime.datetime(2013, 9, 22, 04, 50)
    #end_time = datetime.datetime.now()

    # Get the data from the server
    C1 = e.getData('C1', start_time, end_time)
    C3 = e.getData('C3', start_time, end_time)
    PowerPulse = e.getData('PowerPulse', start_time, end_time)

    # filter to low power and unzip
    data = zip(C1, C3, PowerPulse)
    filtered = [d for d in data if d[0][1] < 1000]
    C1, C3, PowerPulse = zip(*filtered)

    # write filtered data to file
    f = open('data.txt', 'w')
    for d in filtered:
        C1L = d[0]
        PPL = d[2]
        f.write("%s, %s, %s\n" % (C1L[0], C1L[1], PPL[1]))
    f.close()

    # compute scale factor
    C1power = totalPower(e.kWh('C1', start_time, end_time))
    PowerPulsepower = totalPower(e.kWh('PowerPulse', start_time, end_time))

    # print results
    print "Calibration results\n==================="
    print "From:  %s" % start_time
    print "To:    %s" % end_time
    print "Pulse: %.3f kWh" % float(PowerPulsepower)
    print "C1:    %.3f kWh" % float(C1power)
    print "Corr:  %.4f" % float(PowerPulsepower / C1power)
    print "Prev:  %.1f" % float(FACTORS['C1'])
    print "New:   %.1f" % float(FACTORS['C1'] * PowerPulsepower / C1power)

    # print scatter plot
    times = [d for d, p in C1]
    C1powers = [p for d, p in C1]
    C3powers = [p for d, p in C3]
    PowerPulsePowers = [p for d, p in PowerPulse]
    TotalPowers = [C1 + C3 for C1, C3 in zip(C1powers, C3powers)]

    xs = PowerPulsePowers
    ys = TotalPowers

    plt.axis([0, max(xs), 0, max(ys)])
    plt.scatter(PowerPulsePowers, TotalPowers)
    plt.show()
