"""
Triple Scissor Extender Visualizer
by Daniel J. Gonzalez
dgonz@mit.edu
Spring 2016
"""

from mpl_toolkits.mplot3d import Axes3D
import numpy as np
import matplotlib.pyplot as plt
import TSEMath as tsm
import time

pi = np.pi

class TSE_Plot():

	def __init__(self, p=[0,0,48.0,0,0,0], q=[15.53072495,15.53072495,15.53072495,15.53072495,15.53072495,15.53072495],\
								 L=68.0, k1=18.0/68.0, k2=(np.pi/6.0), k3=0.0186, k4=8.0/68.0, hT=2.125, hB=3.5231):
		self.L = float(L)
		self.k1 = k1
		self.k2 = k2
		self.k3 = k3
		self.k4 = k4
		self.hT = hT
		self.hB = hB
		rT = k4*L #8 inches usually, Radius from center of top top ball joint

		self.eTp1 = np.matrix([rT, 0, -hT]).transpose()
		self.eTp2 = np.matrix([rT*np.cos(2*pi/3), rT*np.sin(2*pi/3), -hT]).transpose()
		self.eTp3 = np.matrix([rT*np.cos(4*pi/3), rT*np.sin(4*pi/3), -hT]).transpose()

		self.fig = plt.figure()

	def plotTSE(self, p=[0,0,48.0,0,0,0], q=[15.53072495,15.53072495,15.53072495,15.53072495,15.53072495,15.53072495]):
		L = self.L
		k1 = self.k1 
		k2 = self.k2
		k3 = self.k3 
		k4 = self.k4 
		hT = self.hT 
		hB = self.hB
		rT = k4*L #8 inches usually, Radius from center of top top ball joint
		eTp1 = self.eTp1
		eTp2 = self.eTp2
		eTp3 = self.eTp3

		ax = self.fig.add_subplot(111, projection='3d')
		ax.set_xlim3d(-30, 30)
		ax.set_ylim3d(-30, 30)
		ax.set_zlim3d(0, 72)
		plt.ion()
		plt.show()

		ax.set_xlabel('X [inches]')
		ax.set_ylabel('Y [inches]')
		ax.set_zlabel('Z [inches]')

		Rz = tsm.rotz(p[3])
		Ry = tsm.roty(p[4])
		Rx = tsm.rotx(p[5])


		#Draw Origin
		ax.scatter(0,0,0, marker = '+', c = 'k')

		#Draw base
		baseA = np.matrix([-29,5,0]).transpose()
		baseB = np.matrix([[-29],[-5],[0]])

		basePts = np.hstack((baseA, baseB, tsm.rotz(2*np.pi/3)*baseA, tsm.rotz(2*np.pi/3)*baseB, tsm.rotz(-2*np.pi/3)*baseA, tsm.rotz(-2*np.pi/3)*baseB, baseA))
		basePts = np.array(basePts)

		ax.plot(basePts[0,:] ,basePts[1,:], basePts[2,:],c='dimgrey')

		# Plot Endpoint
		a1 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*np.matrix([[10],[0],[0]])
		a2 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*np.matrix([[0],[10],[0]])
		a3 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*np.matrix([[0],[0],[10]])

		self.xEnd = ax.plot([p[0], a1[0]], [p[1], a1[1]], [p[2], a1[2]], c='r', marker = '<')
		self.yEnd = ax.plot([p[0], a2[0]], [p[1], a2[1]], [p[2], a2[2]], c='g', marker = '<')
		self.zEnd = ax.plot([p[0], a3[0]], [p[1], a3[1]], [p[2], a3[2]], c='b', marker = '<')

		#Get top points
		
		BTp1 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp1
		BTp2 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp2
		BTp3 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp3

		BTp = np.array(np.hstack((BTp1, BTp2, BTp3, BTp1)))

		#plot top points
		self.myPts = ax.plot(BTp[0,:], BTp[1,:], BTp[2,:],c='darkviolet')

		#Update the Figure
		self.fig.canvas.draw_idle()
		plt.pause(0.001)

	def update_lines(self, num, dataLines, lines) :
		for line, data in zip(lines, dataLines) :
			# NOTE: there is no .set_data() for 3 dim data...
			line.set_data(data[0:2, num:num+2])
			line.set_3d_properties(data[2,num:num+2])
		return lines

	def updateThings(self, linesObj, xPts, yPts, zPts):
		linesObj[0].set_data(xPts, yPts)
		linesObj[0].set_3d_properties(zPts)

	def updatePlot(self, p=[0,0,48.0,0,0,0], q=[15.53072495,15.53072495,15.53072495,15.53072495,15.53072495,15.53072495]):

		Rz = tsm.rotz(p[3])
		Ry = tsm.roty(p[4])
		Rx = tsm.rotx(p[5])
		eTp1 = self.eTp1
		eTp2 = self.eTp2
		eTp3 = self.eTp3

		# Plot Endpoint
		a1 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*np.matrix([[10],[0],[0]])
		a2 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*np.matrix([[0],[10],[0]])
		a3 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*np.matrix([[0],[0],[10]])
		self.updateThings(self.xEnd,[p[0], a1[0]], [p[1], a1[1]],[p[2], a1[2]])
		self.updateThings(self.yEnd,[p[0], a2[0]], [p[1], a2[1]],[p[2], a2[2]])
		self.updateThings(self.zEnd,[p[0], a3[0]], [p[1], a3[1]],[p[2], a3[2]])

		#plot top points
		BTp1 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp1
		BTp2 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp2
		BTp3 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp3
		BTp = np.array(np.hstack((BTp1, BTp2, BTp3, BTp1)))
		self.updateThings(self.myPts, BTp[0,:], BTp[1,:], BTp[2,:])

		#Update the Figure
		self.fig.canvas.draw_idle()
		plt.pause(0.001)
		return	

def test(p=[0,0,48.0,0,0,0]):
	L=68.0
	k1=18.0/68.0
	k2=(np.pi/6.0)
	k3=0.0186
	k4=8.0/68.0
	hT=2.125
	hB=3.5231

	#Create an instance of the TSE_Plot() Object
	myTSEPlot = TSE_Plot()
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