#!/usr/bin/env python3

import subprocess
import re
import time
import sys

outfile = open('outfile2.data', 'w+')

template = './waf --run "scratch/report2 --useKernel=1 --tracing=1 --tcp_stack_nodeA={} --tcp_stack_nodeB={} --delayA={} --simTime=200 --err_rate={}"'
tshark = 'tshark -r report2-{}.pcap -q -z endpoints,tcp > tsharktemp.tmp'

#tcp_versions = [('reno', 'reno'), ('cubic', 'cubic')]
tcp_versions = [('cubic', 'cubic')]
rtts = ['5ms', '20ms', '50ms']
errors = ['0.000001', '0.000002', '0.000003', '0.000004', '0.000005']

for tcp_version in tcp_versions:
    for rtt in rtts:
        for i, error in enumerate(errors):
            cmd = template.format(tcp_version[0], tcp_version[1], rtt, error)
            ident = '{}_{}_{}'.format('{}-{}'.format(*tcp_version), rtt, i)
            print('[{}] {}'.format(i, cmd))
            output = subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=True).decode()

            if not 'Done.' in output:
                print(output)
                sys.exit('Error occured. No Done. in output.')
            else:
                for device in ['0-1', '1-1', '3-1']:
                    tshark_cmd = tshark.format(device)
                    print('\t {}'.format(tshark_cmd))
                    out = subprocess.check_output(tshark_cmd, stderr=subprocess.STDOUT, shell=True).decode()
                    time.sleep(0.1)
                    for line in open('tsharktemp.tmp', 'r'):
                        if line.startswith('10.'):
                            towrite = '\t\t{} {} {}'.format(ident, device, line)
                            print(towrite)
                            outfile.write(towrite)
                            outfile.flush()
outfile.close()
