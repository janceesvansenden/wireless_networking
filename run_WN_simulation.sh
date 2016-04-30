#!/bin/bash
../waf --run WN_NS3

gnuplot ../throughput-vs-backbone-datarate.plt
eog ./throughput-vs-backbone-datarate.png &
rm ../throughput-vs-backbone-datarate*

gnuplot ../throughput-vs-numOfStations.plt
eog ./throughput-vs-numOfStations.png &
rm ../throughput-vs-numOfStations*

gnuplot ../throughput-vs-streamer.plt
eog ./throughput-vs-streamer.png &
rm ../throughput-vs-streamer*