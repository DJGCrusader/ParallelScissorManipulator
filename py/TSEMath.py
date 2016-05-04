"""
Inverse Kinematic Solver for Triple Scissor Extender
by Daniel J. Gonzalez
dgonz@mit.edu
Spring 2016
"""

import numpy as np
from scipy.optimize import fsolve
import math 

pi = np.pi

def rotz(theta):
	return np.matrix([[np.cos(theta), -np.sin(theta), 0], [np.sin(theta),  np.cos(theta), 0], [0, 0, 1]])

def roty(theta):
	return np.matrix([[np.cos(theta), 0, np.sin(theta)],[0, 1, 0], [-np.sin(theta), 0, np.cos(theta)]])

def rotx(theta):
	return np.matrix([[1, 0, 0], [0, np.cos(theta), -np.sin(theta)], [0, np.sin(theta),  np.cos(theta)]])

def solveNDIK(p=[0, 0, 48, 0, 0, 0], L=68.0, k1=18.0/68.0, k2=(np.pi/6.0), k3=0.0186, k4=8.0/68.0, hT=2.125, hB=3.5231):
	"""
	Example: 
		SolveNDIK([0, 0, 48, 0, 0, 0], 68, 18/68, pi/6, 8/68, 0.0186, hT, hB)
	Parameters:
		p = Endpoint Pose
		L = Total Scissor Length
		k1 = l_0/L
		k2 = Actuator Angle eta
		k3 = rA/L
		k4 = rt/L
	Dimensions
		hT = 2.125   #2+5/8-1/2 Distance from top to ball joint  
		hB = 3.5231  #height from base top surface to actuator ball joint
	"""

	####################################Check if legal point, put within limits
	if(p[2]>63.75):
		p[2]=63.75
	if(p[2]<12.75):
		p[2]=12.75

	if(p[1]>6):
		p[1]=6
	if(p[1]<-6):
		p[1]=-6
	if(p[0]>6):
		p[0]=6
	if(p[0]<-6):
		p[0]=-6

	if(p[3]>pi/6):
		p[3]=pi/6
	if(p[3]<-pi/6):
		p[3]=-pi/6
	if(p[4]>pi/6):
		p[4]=pi/6
	if(p[4]<-pi/6):
		p[4]=-pi/6
	if(p[5]>pi/6):
		p[5]=pi/6
	if(p[5]<-pi/6):
		p[5]=-pi/6


	L = float(L)
	rT = k4*L #8 inches usually, Radius from center of top top ball joint
	
	eTp1 = np.matrix([rT, 0, -hT]).transpose()
	eTp2 = np.matrix([rT*np.cos(2*pi/3), rT*np.sin(2*pi/3), -hT]).transpose()
	eTp3 = np.matrix([rT*np.cos(4*pi/3), rT*np.sin(4*pi/3), -hT]).transpose()

	Rz = rotz(p[3])
  	Ry = roty(p[4])
  	Rx = rotx(p[5])

  	BTp1 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp1
  	BTp2 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp2
  	BTp3 = np.matrix(p[0:3]).transpose()+Rx*Ry*Rz*eTp3

  	#	Transform points from Origin to A, B, C
  	#Rotation Matrices for coordinate shift
	RzA = rotz(-pi/2)
	RzB = rotz(pi/6)
	RzC = rotz(5*pi/6)

	#get center points of A, B, C in O frame: 
	bA = RzA*np.matrix([0, k4*L, 0]).transpose()
	bB = RzB*np.matrix([0, k4*L, 0]).transpose()
	bC = RzC*np.matrix([0, k4*L, 0]).transpose()

	#to convert a point in O to A: 
	#Subtract by bA, multiply by inverse rotation matrix. 
	t1 = np.linalg.inv(RzA)*(BTp1 - bA)
	t2 = np.linalg.inv(RzB)*(BTp2 - bB)
	t3 = np.linalg.inv(RzC)*(BTp3 - bC)

	Tops = np.hstack((t1, t2, t3))

	##  Kinematic constraint equations

	q = np.transpose([0.0, 0.0, 0.0, 0.0, 0.0, 0.0])

	for ii in range(0,3):
		x_C = Tops[0,ii]/L
		y_C = Tops[1,ii]/L
		z_C = (Tops[2,ii]-hB)/L
		
		def simulEqns(inp):
			s_A, s_B = inp
			eq1 = s_A**2 - 2*x_C*s_A*np.cos(k2) + 2*(k3-y_C)*s_A*np.sin(k2) - (s_B**2 + 2*x_C*s_B*np.cos(k2) + 2*(k3-y_C)*s_B*np.sin(k2))
			eq2 = s_A**2 - 2*x_C*s_A*np.cos(k2) + 2*(k3-y_C)*s_A*np.sin(k2) - \
																( -(x_C**2+k3**2-2*y_C*k3+y_C**2+z_C**2) + 1 + 0.25*(1-(1/k1)**2)*(s_A**2+s_A**2+2*s_A*s_B*np.cos(2*k2)))
			return (eq1,eq2)

		(sigA, sigB) = fsolve(simulEqns,(5,5))

		q[ii*2] = L*sigA
		q[ii*2+1] = L*sigB

	pos= [12 if (20.6-xx)>12 else(0 if (20.6-xx)<0 else (20.6-xx)) for xx in q]
	q = [20.6-xx for xx in pos]
	print 'pos: ', pos
	return q

def test():
	q = solveNDIK(p=[0, 0, 48, 0, 0, 0], L=68.0, k1=18.0/68.0, k2=(pi/6.0), k3=0.0186, k4=8.0/68.0, hT=2.125, hB=3.5231)
	print q