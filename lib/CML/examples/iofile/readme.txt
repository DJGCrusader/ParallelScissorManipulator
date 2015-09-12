
This directory contains a simple example of reading and writing a .cci file
using the CML libraries.  Here are the steps you will need to get it working:
  
- Make sure the node ID of the I/O Module matches the one set at the top of the
  iofile.cpp file.

- Make sure the CAN ID of the network card matches the one set at the top of the
  iofile.cpp file.

- Make sure network objects match the network card vendor. For instance, for a
  Copley CAN card, use the CopleyCAN object.
  
- Compile iofile.cpp.  You will also need to compile in the various CPP files in the
  main CML directory.  The files you will need to compile will be:
  
  CML/examples/iofile/iofile.cpp
  CML/c/*.cpp
  CML/c/can/can_copley.cpp            <--- depends on which CAN card is being used
  CML/c/threads/Threads_w32.cpp       <--- depends on the operating system
  
- You will need to set the include path for the compiler to find header files located
  in the following directories:
  
  CML/inc/
  CML/inc/can
  
Once everything compiles, you should be able to run the example and have it  
read and restore I/O parameters from a .cci File.

To adapt this example to load your own .cci file simply modify the folowing line
to reflect the path of your own .cci file.

  err = io.LoadFromFile( "IOFileExample.cci", line );
