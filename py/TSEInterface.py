"""
TSE Interface
Daniel J. Gonzalez - dgonz@mit.edu
Fall 2015
"""

import subprocess as sp
import time

binLocation = "../bin/PSM_main"

interface = sp.Popen(binLocation, stdout=sp.PIPE, stdin=sp.PIPE)
while(1):
	readData = interface.stdout.readline()
	if(readData==''):
		break
	else:
		print readData
print 'Done'

def sendCommand(q):
	#Send the TSE Program the desired sigmas

	interface.stdin.write(str(q))