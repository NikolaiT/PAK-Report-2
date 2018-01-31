#!/usr/bin/env python3

"""
TCP Endpoints
Filter:<No Filter>
                       |  Port  ||  Packets  | |  Bytes  | | Tx Packets | | Tx Bytes | | Rx Packets | | Rx Bytes |
10.1.2.1                  57831      11241       9990070       6463         9695258        4778          294812
10.1.3.2                  60000      11241       9990070       4778          294812        6463         9695258
10.1.1.1                  49323       6438       5613704       3631         5440194        2807          173510
10.1.3.2                  50000       6438       5613704       2807          173510        3631         5440194
"""

import matplotlib.pyplot as plt
import math
import re
import numpy as np
import sys

if not sys.argv[1]:
    tcp_version = 'reno-reno'
else:
    tcp_version = sys.argv[1]

font = {'family' : 'normal',
        'weight' : 'normal',
        'size'   : 16}

plt.rc('font', **font)

mses = ['5ms', '10ms', '20ms', '50ms']
errs = ['0.000001', '0.000002', '0.000003', '0.000004', '0.000005']

def rel_rate(x, y):
    return [a/(b/1024.0/1024.0/200.0) for a,b in zip(x,y)]


datafile = open('outfile2.data', 'r')


def get_data(file, tcp='cubic-cubic'):
    X = dict()
    Y = dict()
    CorrA  = dict()
    CorrB = dict()
    var = [None, None, None, None]
    for line in datafile:
        line = line.strip()
        if '0-1 10.1.1.1' in line and tcp in line:
            matches = re.findall(r'\s([0-9]{3,15})', line)
            var[0] = matches[3] # tx_a

        if '1-1 10.1.2.1' in line and tcp in line:
            tx_bytes_b = None
            matches = re.findall(r'\s([0-9]{3,15})', line)
            var[1] = matches[3] # tx_b

        if '3-1 10.1.3.2' in line and tcp in line:
            res = re.search(r'_(\d+ms)_(\d+)', line)
            ms, it = res.group(1), res.group(2)
            matches = re.findall(r'\s([0-9]{3,15})', line)
            port = matches[0]
            rx_bytes = matches[6]
            tx_bytes = matches[4]
            rx_bytes = int(rx_bytes)/1024.0/200.0
            if int(port) == 50000:
                var[2] = matches[5] # rx_a
                if ms not in X:
                    X[ms] = []
                X[ms].append(rx_bytes)
            elif int(port) == 60000:
                var[3] = matches[5] # rx_b
                if ms not in Y:
                    Y[ms] = []
                Y[ms].append(rx_bytes)

        if not None in var:
            var = [int(a) for a in var]
            print(ms, errs[int(it)], 'Corrupt packages A', var[0] - var[2], 'Corrupt packages B', var[1] - var[3])
            if ms not in CorrA:
                CorrA[ms] = []
            CorrA[ms].append(var[0] - var[2])
            if ms not in CorrB:
                CorrB[ms] = []
            CorrB[ms].append(var[1] - var[3])
            X[ms].append(rx_bytes)
            var = [None, None, None, None]


    return X, Y, CorrA, CorrB


X, Y, CorrA, CorrB = get_data(datafile, tcp_version)

# calculate standard error
# error = sample standard deviation / sqrt(n)
def std_err(X):
    return np.std(X) / math.sqrt(len(X))

XX = []
XXerr = []
YY = []
YYerr = []
for a in ['5ms', '10ms', '20ms', '50ms']:
    avgA = sum(X[a])/len(X[a])
    avgB = sum(Y[a])/len(Y[a])
    avgCorrA = sum(CorrA[a])/len(CorrA[a])
    avgCorrB = sum(CorrB[a])/len(CorrB[a])
    print('Avg Rx A', round(avgA, 2), a)
    print('Avg Rx B', round(avgB, 2), a)
    print('Avg Corrupt packages A', round(avgCorrA, 2), a)
    print('Avg Corrupt packages B', round(avgCorrB, 2), a)
    XX.append(avgA)
    XXerr.append(std_err(X[a]))
    YY.append(avgB)
    YYerr.append(std_err(Y[a]))

x = np.arange(0, 4, 1)

tcp_name = tcp_version.split('-')[0].upper()
plt.errorbar(x, XX, yerr=XXerr, linestyle='-', marker='o', color='r', markersize=6, markeredgewidth=2, capsize=15, label='{}-TCP Stream with different RTTs'.format(tcp_name))
plt.errorbar(x, YY, yerr=YYerr, linestyle='-', marker='o', color='g', markersize=6, markeredgewidth=2, capsize=15, label='{}-TCP Stream with static RTT=10ms'.format(tcp_name))
plt.xlim([-0.5, 4])
plt.title('Throughput of two {}-TCP connections, Samplesize(N) = 5, Errorbar: Standard Error'.format(tcp_name))
plt.ylabel('KB/s')
plt.xticks(x, ['5ms', '10ms', '20ms', '50ms'], rotation='vertical')
plt.legend(loc='upper center', shadow=True)
plt.show()
plt.savefig('img/{}.png'.format(tcp_version))
