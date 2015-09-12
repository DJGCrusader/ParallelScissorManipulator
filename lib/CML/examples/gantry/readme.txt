
This directory contains a very simple example of using two amplifiers/motors
in a coordinated way to drive a single gantry.  (A gantry is a pair of linear
stages that move in unison with some structure held between them.)

Here are the steps you will need to get it working:

- Use the CME program to setup the amps.  This example doesn't set any loop 
  gains or other setup parameters, it assumes that has already been done.
  
- Make sure the node ID of the amplifiers match the ones set at the top of the
  gantry.cpp file.
  
- Compile gantry.cpp.  You will also need to compile in the various CPP files in the
  main CML directory.  The files you will need to compile will be:
  
  CML/examples/gantry/gantry.cpp
  CML/c/*.cpp
  CML/c/can/can_copley.cpp           <--- depends on which CAN card is being used.
  CML/c/threads/Threads_w32.cpp      <--- depends on the operating system
  
- If running under windows, you can use the supplied project file (gantry.dsp)
  to configure the compiler.

