# Microsoft Developer Studio Project File - Name="pdotest" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=pdotest - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "pdotest.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "pdotest.mak" CFG="pdotest - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "pdotest - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "pdotest - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "pdotest - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "pdotest - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CRT_SECURE_NO_WARNINGS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "..\..\inc" /I "..\..\inc\can" /I "..\..\inc\ecat" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "_CRT_SECURE_NO_WARNINGS" /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "pdotest - Win32 Release"
# Name "pdotest - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=pdotest.cpp
# End Source File
# End Group
# Begin Group "CML"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\c\Amp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpFile.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpFW.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpParam.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpPDO.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpPVT.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpStruct.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpUnits.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\AmpVersion.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Can.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\can\can_copley.cpp
# End Source File
# Begin Source File

SOURCE=..\..\inc\can\can_copley.h
# End Source File
# Begin Source File

SOURCE=..\..\c\CanOpen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\CML.cpp
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Amp.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_AmpDef.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_AmpStruct.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Can.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_CanOpen.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Error.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_ErrorCodes.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_EventMap.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Filter.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Firmware.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Geometry.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_IO.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Linkage.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Node.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Path.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_PDO.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_SDO.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Settings.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Threads.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Trajectory.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_TrjScurve.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\CML_Utils.h
# End Source File
# Begin Source File

SOURCE=..\..\inc\can\copley_internal.h
# End Source File
# Begin Source File

SOURCE=..\..\c\CopleyIO.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\CopleyIOFile.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\CopleyNode.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\ecat\ecat_winudp.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\ecatdc.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Error.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\EtherCAT.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\EventMap.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\File.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Filter.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Firmware.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Geometry.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\InputShaper.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\IOmodule.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Linkage.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\LSS.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Network.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Node.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Path.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\PDO.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Reference.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\SDO.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Threads.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\threads\Threads_w32.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\TrjScurve.cpp
# End Source File
# Begin Source File

SOURCE=..\..\c\Utils.cpp
# End Source File
# End Group
# End Target
# End Project
