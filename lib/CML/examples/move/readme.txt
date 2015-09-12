
This directory contains a very simple example of moving a single axis using the 
CML libraries.  Here are the steps you will need to get it working:

- Use the CME program to setup the motor.  This example doesn't set any loop 
  gains or other setup parameters, it assumes that has already been done.
  
- Make sure the node ID of the amplifier matches the one set at the top of the
  move.cpp file.
  
- Compile move.cpp.  You will also need to compile in the various CPP files in the
  main CML directory.  The files you will need to compile will be:
  
  CML/examples/move/move.cpp
  CML/c/*.cpp
  CML/c/can/can_copley.cpp           <--- depends on which CAN card is being used.
  CML/c/threads/Threads_w32.cpp      <--- depends on the operating system
  
- You will need to set the include path fo the compiler to find header files located
  in the following directories:
  
  CML/inc/
  CML/inc/can

- If running under windows, you will need to adjust the compiler settings so it can
  find the header files that came with the CAN card.
  
Once everything compiles, you should be able to run the example and have it  
perform a bunch of random moves.
