
This directory contains a very simple example of moving multiple axes using the 
CML libraries.  Here are the steps you will need to get it working:

- Use the CME program to setup the amps.  This example doesn't set any loop 
  gains or other setup parameters, it assumes that has already been done.
  
- Make sure the node ID of the amplifiers match the ones set at the top of the
  twoaxis.cpp file.
  
- Compile twoaxis.cpp.  You will also need to compile in the various CPP files in the
  main CML directory.  The files you will need to compile will be:
  
  CML/examples/twoaxis/twoaxis.cpp
  CML/c/*.cpp
  CML/c/can/can_copley.cpp           <--- depends on which CAN card is being used.
  CML/c/threads/Threads_w32.cpp      <--- depends on the operating system
  
- You will need to set the include path for the compiler to find header files located
  in the following directories:
  
  CML/inc/
  CML/inc/can
  

- If running under windows, you will need to adjust the compiler settings so it can
  find the header files that came with the CAN card.

If everything compiles, then you should be able to run the example and have it  
perform a bunch of multi-axis moves.