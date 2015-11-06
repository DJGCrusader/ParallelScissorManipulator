"""
TSE Interface
Daniel J. Gonzalez - dgonz@mit.edu
Fall 2015
"""

import subprocess as sp

binLocation = "../bin/PSM_main"

interface = sp.Popen(binLocation, stdout=sp.PIPE, stdin=sp.PIPE)
print interface.communicate()

def sendCommand(q):
	#Send the TSE Program the desired sigmas
	(respMsg, errMsg) = interface.communicate(str(q))