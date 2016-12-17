"""
TSE Interface
Daniel J. Gonzalez - dgonz@mit.edu
Fall 2015
"""

import subprocess as sp
import time
import struct

IN2MM = 25.4
binLocation = "../bin/PSM_main"

interface = sp.Popen(binLocation, stdout=sp.PIPE, stdin=sp.PIPE)
myit = 0
# while(myit<50):
#     q = [451.015, 452.015, 453.014, 454.01, 455.01, 456.01]
#     readData = interface.stdout.readline()[:-1]
#     print 'PyReceived: ', readData
#     if(readData=='RUN CHECK'):
#         interface.stdin.write('1')
#     elif(readData=='RUN OK'):
#         iter = 0
#     elif(readData=='GIMME'):
#         print struct.pack('d',q[iter])+'\n'
#         interface.stdin.write(struct.pack('d',q[iter]))
#         iter+=1
#     elif(readData=='BAD Q'):
#         print 'Bad Q!'
#     myit+=1
# print 'Done'

def sendCommand(q):
    #Send the TSE Program the desired sigmas in MILLIMETERS!
    iter = 0
    while(1):
        readData = interface.stdout.readline()[:-1]
        #print 'PyReceived: ', readData
        if(readData=='RUN CHECK'):
            interface.stdin.write('1')
        elif(readData=='GIMME'):
            interface.stdin.write(struct.pack('d',q[iter])) #not perfect, check.
            iter+=1
        elif(readData=='GOOD Q'):
            #print 'Good Q!'
            break
        elif(readData=='BAD Q'):
            print 'Bad Q!'
            break