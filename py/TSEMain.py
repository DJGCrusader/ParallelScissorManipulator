"""
TSE Main
Daniel J. Gonzalez - dgonz@mit.edu
"""

# import TSEInterface as tse
import TSEVis as tsv
import TSEMath as tsm
import numpy as np
import TSEInterface as tsi
import time
import random
import math 

pi = np.pi
in2mm = 25.4

def main():
	running = True

	p=[0,0,48.0,0,0,0]
	L=68.0
	k1=18.0/68.0
	k2=(np.pi/6.0)
	k3=0.03710 #0.0186
	k4=8.0/68.0
	hT=2.125
	hB=3.5231

	#Create an instance of the TSE_Plot() Object
	myTSEPlot = tsv.TSE_Plot()
	#Solve IK
	q = tsm.solveNDIK(p, L, k1, k2, k3, k4, hT, hB)
	#Create Initial Plot
	myTSEPlot.plotTSE(p, q)

	# zz = 12.75
	while(running):
		#New Endpoint Pose
		xx = 0
		yy = 0
		zz = 48
		phiphi = np.radians(0)*np.sin(2*pi*time.time()/5)
		thttht = np.radians(30)*np.sin(2*pi*time.time()/5)
		psipsi = np.radians(30)*np.cos(2*pi*time.time()/5)
		## For testing: ##
		# xx = 0#12*random.random()-6
		# yy = 0#12*random.random()-6
		# zz = 12.75 + random.random()*51.0
		# #zz = zz+0.25
		# phiphi = 0#random.random()-.5
		# thttht = 0#random.random()-.5
		# psipsi = 0#random.random()-.5

		p=[xx,yy,zz, phiphi, thttht,psipsi]

		#print 'Going to new point: ', p

		#Solve IK for new Endpoint Pose
		q = tsm.solveNDIK(p, L, k1, k2, k3, k4, hT, hB)

		q = [a*in2mm for a in q]
		tsi.sendCommand(q)

		myTSEPlot.updatePlot(p, q)
		# tse.sendCommand()

def test(p=[0,0,48.0,0,0,0]):
	L=68.0
	k1=18.0/68.0
	k2=(np.pi/6.0)
	k3=0.03710 #0.0186
	k4=8.0/68.0
	hT=2.125
	hB=3.5231

	#Create an instance of the TSE_Plot() Object
	myTSEPlot = tsv.TSE_Plot()
	#Solve IK
	q = tsm.solveNDIK(p, L, k1, k2, k3, k4, hT, hB)
	#Create Initial Plot
	myTSEPlot.plotTSE(p, q)

	#New Endpoint Pose
	p=[5,3,30,.1,.2,.12]
	#Solve IK for new Endpoint Pose
	q = tsm.solveNDIK(p, L, k1, k2, k3, k4, hT, hB)
	#
	myTSEPlot.updatePlot(p, q)

	p=[5,3,31,.1,.2,.12]
	q = tsm.solveNDIK(p, L, k1, k2, k3, k4, hT, hB)
	myTSEPlot.updatePlot(p, q)

	p=[5,3,32,.1,.2,.12]
	q = tsm.solveNDIK(p, L, k1, k2, k3, k4, hT, hB)
	myTSEPlot.updatePlot(p, q)

	p=[5,3,33,.1,.2,.12]
	q = tsm.solveNDIK(p, L, k1, k2, k3, k4, hT, hB)
	myTSEPlot.updatePlot(p, q)
	time.sleep(1)

main()