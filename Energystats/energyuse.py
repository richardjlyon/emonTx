#! /usr/bin/env python
# -*- coding: UTF-8 -*-

import datetime

from emoncms.emoncms import EmonCMS
from config import servers, tarrif
from thinkstats import thinkstats

def daterange(start_date, end_date):
    for n in range(int ((end_date - start_date).days)):
        yield start_date + datetime.timedelta(n)

def printStats(feeds = ['C1', 'C3'], period = datetime.timedelta(days = 14), server=servers['emoncms'], tarrif=tarrif):
    """Print statistics for the given feeds in the given period

    Args:
        feeds: a list of feed name strings
        period: int the number of days to process, starting yesterday

    Returns:
        nothing
    """

    e = EmonCMS(server, tarrif)

    daystart, priceday = tarrif["day"]
    nightstart, pricenight = tarrif["night"]

    end_date = datetime.date.today()
    start_date = end_date - period

    daypowers = []
    nightpowers = []
    daycosts = []
    nightcosts = []

    heatingpowertotal = 0

    print "Power usage and cost\n======================================="
    print "{:>34}{:>5}".format("Day", "Htg")
    for date in daterange(start_date, end_date):
        kwhdaytotal = kwhnighttotal = kwheatingtotal = costday = costnight = 0
        for feed in feeds:
            kwhday, kwhnight = e.kWh(feed, date)
            kwhdaytotal += kwhday
            kwhnighttotal += kwhnight
            if feed == "C3":
                kwheatingtotal += (kwhday + kwhnight)
                heatingpowertotal += (kwhday + kwhnight)
        costday = kwhdaytotal * priceday / 100.0
        costnight = kwhnighttotal * pricenight / 100.0

        daypowers.append(kwhdaytotal)
        nightpowers.append(kwhnighttotal)
        daycosts.append(costday)
        nightcosts.append(costnight)

        formatter = '{:%d-%m-%Y} | {:>4.1f} kWh, £{:>5.2f}, {:>2.0f}%, {:>2.0f}%'

        print formatter.format(
                date,
                kwhdaytotal+kwhnighttotal,
                costday+costnight,
                100 * kwhdaytotal / float(kwhdaytotal + kwhnighttotal),
                100 * kwheatingtotal / float(kwhdaytotal + kwhnighttotal),
                )

    totalpowers = [day + night for day, night in zip(daypowers, nightpowers)]
    totalcosts = [day + night for day, night in zip(daycosts, nightcosts)]

    print "---------------------------------------"
    print "{:>17.1f} kWh, £{:.2f}".format(sum(totalpowers), sum(totalcosts))

    print "\nPower statistics\n======================================="
    print "{:<10} | {:>4.1f} kWh (var {:.1f} kWh)".format("Mean Power", thinkstats.Mean(totalpowers), thinkstats.Var(totalpowers))
    print "{:<10} | £{:>3.2f}    (var £{:.2f}   )".format("Mean Cost", thinkstats.Mean(totalcosts), thinkstats.Var(totalcosts))
    print "{:<10} | {:>2.0f}%".format("Day Tarrif", 100 * sum(daypowers) / float(sum(totalpowers)))
    print "{:<10} | {:>2.0f}%".format("Heating", 100 * heatingpowertotal / float(sum(totalpowers)))
    print "\n"

    return

if __name__ == "__main__":

    printStats()

