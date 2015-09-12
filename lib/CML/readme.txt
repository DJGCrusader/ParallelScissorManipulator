Sept 19, 2006

This directory contains the source code for the Copley Motion Libraries (CML).
The libraries are a collection of C++ classes which provide a high level 
interface to the CANopen devices produced by Copley Controls Corp.
In addition, the libraries provide a generic CANopen network layer that may 
be used to communicate with other CANopen devices on the network.

To get started with the library, the following steps should be taken:

- Use the CME (Copley Motion Explorer) program to configure your amplifier(s).
  This is the best way to ensure that the motors are wired correctly, set good
  loop gains for the position, velocity & current loops, and generally ensure 
  that things are working before trying to control them over the CANopen network.
  
- Take a quick look at the reference manual.  The manual is provided in two 
  formats; PDF and HTML.  The top level of the html version is the file 
  CML/html/index.html.  If you're going to be doing much CML coding then this
  is a handy bookmark to have in your browser.

  This manual isn't really intended to be read through, but it is a useful
  reference and it's very well hyperlinked.  It's important to become somewhat
  familiar with it's layout.
  
- Take a look at CML/inc/CML_Settings.h.  This file contains a couple of settings that 
  can be used to customize the libraries.  The default settings will work for 
  most systems.
  
- Make sure you have a CAN card configured properly in your computer with any
  necessary drivers loaded.  The libraries support CAN cards from a number of
  different manufacturers, and they are designed to make it easy to add
  support for new cards.

  The examples shipped with the libraries are setup to use the Copley PCI CAN card
  by default.  If you are using another manufacturers card then you will need
  to download a development library from that card's manufacturer to link in
  with CML when you compile it.
  
- Look at the simple move example program.  This is a trivial example of moving 
  a single axis using the CML libraries.  Before you compile it, take a look at
  the file examples\move\readme.txt.

The following should be included in this directory structure:

ROOT
 |
 +---- c ............... This directory contains the *.cpp files which implement the core
 |     |                 of the CML libraries.
 |     |
 |     +--- can ........ This directory gives examples of CAN network interface classes.
 |     |                 These classes provide a generic interface for various CAN cards.
 |     |                 As of this writing there is one interface that will work for
 |     |                 both windows and Linux (can_copley.cpp - the Copley CAN card 
 |     |                 interface), and a number of other interfaces that work under
 |     |                 Windows only.
 |     |
 |     +--- threads .... Implementations of the generic multi-threading utility classes.
 |                       Presently three implementations are available, one for WIN32,
 |                       one for Posix compliant operating systems (including Linux), and 
 |                       one that supports an embedded OS produced by Texas Instruments.
 |
 +---- inc ............. Contains the header files used by the core classes
 |     |
 |     +--- can ........ Header files related to the CAN drivers
 |
 +---- examples
 |     |
 |     +--- move ....... A very simple example of initializing one amplifier, homing it, and
 |     |                 doing a bunch of random moves.
 |     |
 |     +--- twoaxis .... A very simple example of using the Linkage class to control multiple
 |     |                 amplifiers as a single unit.
 |     |
 |     +--- gantry ..... An example of homing an moving two axes connected together in a gantry 
 |     |                 configuration.
 |     |
 |     +--- flash ...... A utility program which can be used to update the firmware on all the
 |                       Copley amplifiers on the CANopen network.  This will only be necessary
 |                       when updated firmware becomes available.
 |
 +----- html ........... Library documentation in html format.  Start with the file index.html.
                         The same documentation is also available in PDF format in the ROOT 
                         directory. 

Windows specific notes:

The libraries and examples have been tested under Windows using Microsoft VC++ 6.0 compiler.
Project files are included with the examples, but if you need to create your own project file 
for some reason then the following notes may be helpful:

- Include all the cpp files in the directory CML/c
- Include one CAN driver from CML/c/can
- Include the windows multi-threading support from CML/c/threads
- Add the directories CML/inc and CML/inc/can to your include file path
- If you are using a CAN card other then the Copley PCI CAN card then add the directories 
  containing the CAN card headers to your include file path.  The location of these headers 
  is dependent on which CAN card you are using, and where you installed it's driver files.  
  This is not necessary for the Copley CAN card as all headers are included in the CML source.
- The libraries are multi-threaded, so you should select the multi-threaded run-time libraries
  from the VC++ menu: Project->Settings->C++->Code Generation menu.


Linux specific notes:

The libraries should compile under any recent version of Linux.  They are tested in house 
using Ubuntu Linux.  At the moment, only the Copley PCI CAN card is supported for use under Linux.

Other OS support:

Most real time operating systems follow the Posix standard API.  For such operating systems, the
multi-threaded classes developed for use under Linux (Threads_posix.cpp) should work with minimal 
change.  The libraries are being actively used by some customers under QNX, and should work with
other Posix based operating systems.  Please contact Copley Controls for assistance in porting the 
libraries to other platforms.

