#
# TODO: Move `libmongoclient.a` to /usr/local/lib so this can work on production servers
#

#Library Compiler:
#dgonz@dgonz-W530:~/projects/ParallelScissorManipulator/lib/CML/c$ g++ -I ../inc -c *.cpp
#dgonz@dgonz-W530:~/projects/ParallelScissorManipulator/lib/CML/c/can$ g++ -I ../../inc/can -I ../../inc -c can_kvaser.cpp
#dgonz@dgonz-W530:~/projects/ParallelScissorManipulator/lib/CML/c/threads$ g++ -I ../../inc/can -I ../../inc -c Threads_posix.cpp
 
CC := g++ # This is the main compiler
# CC := clang --analyze # and comment out the linker last line for sanity
SRCDIR := src
BUILDDIR := build
TARGET := bin/PSM_main 

SRCEXT := cpp
SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT)) 
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o)) lib/CML/c/CML.o lib/CML/c/Linkage.o lib/CML/c/Amp.o lib/CML/c/can/can_kvaser.o lib/CML/c/CanOpen.o lib/CML/c/Utils.o lib/CML/c/Threads.o lib/CML/c/threads/Threads_posix.o lib/CML/c/Can.o lib/CML/c/CopleyIOFile.o lib/CML/c/CopleyIO.o lib/CML/c/CopleyNode.o  lib/CML/c/AmpFile.o lib/CML/c/AmpFW.o lib/CML/c/AmpPVT.o lib/CML/c/AmpUnits.o lib/CML/c/AmpVersion.o lib/CML/c/AmpStruct.o lib/CML/c/AmpPDO.o lib/CML/c/AmpParam.o lib/CML/c/ecatdc.o lib/CML/c/Error.o lib/CML/c/EtherCAT.o lib/CML/c/EventMap.o lib/CML/c/File.o lib/CML/c/Filter.o lib/CML/c/Firmware.o lib/CML/c/Geometry.o lib/CML/c/InputShaper.o lib/CML/c/IOmodule.o  lib/CML/c/LSS.o lib/CML/c/Network.o lib/CML/c/Node.o lib/CML/c/Path.o lib/CML/c/PDO.o lib/CML/c/Reference.o lib/CML/c/SDO.o  lib/CML/c/TrjScurve.o 

#

CFLAGS := -g # -Wall
LIB := -L lib -L lib -pthread -lpthread -lrt
INC := -I include -I lib/CML/inc -I lib/CML/inc/can -I lib/CML/c -I lib/linuxcan/canlib

$(TARGET): $(OBJECTS)
	@echo " Linking..."
	@echo " $(CC) $^ -o $(TARGET) $(LIB)"; $(CC) $^ -o $(TARGET) $(LIB) -Wl,--no-as-needed -ldl

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@echo " Building..."
	@echo " $(CC) $(CFLAGS) $(INC) -c -o $@ $<"; $(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	@echo " Cleaning..."; 
	@echo " $(RM) -r $(BUILDDIR) $(TARGET)"; $(RM) -r $(BUILDDIR) $(TARGET)

# Tests
tester:
	$(CC) $(CFLAGS) test/tester.cpp $(INC) $(LIB) -o bin/tester

# Spikes
ticket:
	$(CC) $(CFLAGS) spikes/ticket.cpp $(INC) $(LIB) -o bin/ticket

.PHONY: clean