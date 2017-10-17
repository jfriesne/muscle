## BeOS Generic Makefile v2.5 ##

## Fill in this file to specify the project being created, and the referenced
## makefile-engine will do all of the hard work for you.  This handles both
## Intel and PowerPC builds of the BeOS and Haiku.

## Application Specific Settings ---------------------------------------------

# specify the name of the binary
NAME = muscle_server

# specify the type of binary
#	APP:	Application
#	SHARED:	Shared library or add-on
#	STATIC:	Static library archive
#	DRIVER: Kernel Driver
TYPE = APP

# 	if you plan to use localization features 
# 	specify the application MIME siganture
APP_MIME_SIG = 

#	add support for new Pe and Eddie features
#	to fill in generic makefile

#%{
# @src->@ 

#	specify the source files to use
#	full paths or paths relative to the makefile can be included
# 	all files, regardless of directory, will have their object
#	files created in the common object directory.
#	Note that this means this makefile will not work correctly
#	if two source files with the same name (source.c or source.cpp)
#	are included from different directories.  Also note that spaces
#	in folder names do not work well with this makefile.
SRCS += MuscleDaemon.cpp
SRCS += ../ConvertMessages.cpp

SRCS += ../../iogateway/AbstractMessageIOGateway.cpp
SRCS += ../../iogateway/MessageIOGateway.cpp
#SRCS += ../../iogateway/PlainTextMessageIOGateway.cpp

SRCS += ../../message/Message.cpp

SRCS += ../../reflector/AbstractReflectSession.cpp
SRCS += ../../reflector/DataNode.cpp
SRCS += ../../reflector/DumbReflectSession.cpp
SRCS += ../../reflector/FilterSessionFactory.cpp
SRCS += ../../reflector/RateLimitSessionIOPolicy.cpp
SRCS += ../../reflector/ReflectServer.cpp
SRCS += ../../reflector/ServerComponent.cpp
SRCS += ../../reflector/SignalHandlerSession.cpp
SRCS += ../../reflector/StorageReflectSession.cpp

SRCS += ../../regex/PathMatcher.cpp
SRCS += ../../regex/QueryFilter.cpp
SRCS += ../../regex/StringMatcher.cpp

SRCS += ../../syslog/SysLog.cpp

#SRCS += ../../system/AcceptSocketsThread.cpp
SRCS += ../../system/GlobalMemoryAllocator.cpp
#SRCS += ../../system/MessageTransceiverThread.cpp
SRCS += ../../system/SetupSystem.cpp
SRCS += ../../system/SignalMultiplexer.cpp
#SRCS += ../../system/Thread.cpp

SRCS += ../../util/ByteBuffer.cpp
SRCS += ../../util/Directory.cpp
SRCS += ../../util/FilePathInfo.cpp
SRCS += ../../util/MiscUtilityFunctions.cpp
SRCS += ../../util/NetworkUtilityFunctions.cpp
SRCS += ../../util/MemoryAllocator.cpp
SRCS += ../../util/PulseNode.cpp
SRCS += ../../util/String.cpp
SRCS += ../../util/SocketMultiplexer.cpp
SRCS += ../../zlib/ZLibCodec.cpp
#SRCS += ../../zlib/ZLibUtilityFunctions.cpp

SRCS += ../../zlib/zlib/adler32.c
SRCS += ../../zlib/zlib/compress.c
SRCS += ../../zlib/zlib/crc32.c
SRCS += ../../zlib/zlib/deflate.c
SRCS += ../../zlib/zlib/gzlib.c
SRCS += ../../zlib/zlib/gzread.c
SRCS += ../../zlib/zlib/gzwrite.c
SRCS += ../../zlib/zlib/gzclose.c
SRCS += ../../zlib/zlib/trees.c
SRCS += ../../zlib/zlib/zutil.c
SRCS += ../../zlib/zlib/inflate.c
SRCS += ../../zlib/zlib/inftrees.c
SRCS += ../../zlib/zlib/inffast.c

#Aktivate ssl
#SSLSocketDataIO.o SSLSocketAdapterGateway.o
   
#	specify the resource definition files to use
#	full path or a relative path to the resource file can be used.
RDEFS = muscled.rdef
	
#	specify the resource files to use. 
#	full path or a relative path to the resource file can be used.
#	both RDEFS and RSRCS can be defined in the same makefile.
RSRCS = 

# @<-src@ 
#%}

#	end support for Pe and Eddie

#	specify additional libraries to link against
#	there are two acceptable forms of library specifications
#	-	if your library follows the naming pattern of:
#SRCS += libXXX.so or libXXX.a you can simply specify XXX
#SRCS += library: libbe.so entry: be
#
#	-	for version-independent linking of standard C++ libraries please add
#SRCS += $(STDCPPLIBS) instead of raw "stdc++[.r4] [supc++]" library names
#
#	-	for localization support add following libs:
#SRCS += locale localestub
#SRCS += 
#	- 	if your library does not follow the standard library
#SRCS += naming scheme you need to specify the path to the library
#SRCS += and it's name
#SRCS += library: my_lib.a entry: my_lib.a or path/my_lib.a
LIBS = be root network stdc++
#LIBS += ssl crypto

#	specify additional paths to directories following the standard
#	libXXX.so or libXXX.a naming scheme.  You can specify full paths
#	or paths relative to the makefile.  The paths included may not
#	be recursive, so include all of the paths where libraries can
#	be found.  Directories where source files are found are
#	automatically included.
LIBPATHS = 

#	additional paths to look for system headers
#	thes use the form: #include <header>
#	source file directories are NOT auto-included here
SYSTEM_INCLUDE_PATHS = /boot/system/develop/headers/private/interface

#	additional paths to look for local headers
#	thes use the form: #include "header"
#	source file directories are automatically included
LOCAL_INCLUDE_PATHS += ../../../muscle
LOCAL_INCLUDE_PATHS += ../../zlib/zlib ../../message ../../besupport
LOCAL_INCLUDE_PATHS += ../../dataio ../../iogateway ../../reflector
LOCAL_INCLUDE_PATHS += ../../regex ../../util ../../syslog 
LOCAL_INCLUDE_PATHS += ../../system ../../zlib ../../zlib/zlib 

#	specify the level of optimization that you desire
#	NONE, SOME, FULL
OPTIMIZE = SOME

# 	specify here the codes for languages you are going
# 	to support in this application. The default "en"
# 	one must be provided too. "make catkeys" will recreate only
# 	locales/en.catkeys file. Use it as template for creating other
# 	languages catkeys. All localization files must be placed
# 	in "locales" sub-directory.
LOCALES =

#	specify any preprocessor symbols to be defined.  The symbols will not
#	have their values set automatically; you must supply the value (if any)
#	to use.  For example, setting DEFINES to "DEBUG=1" will cause the
#	compiler option "-DDEBUG=1" to be used.  Setting DEFINES to "DEBUG"
#	would pass "-DDEBUG" on the compiler's command line.
DEFINES	+= MUSCLE_SINGLE_THREAD_ONLY
DEFINES	+= MUSCLE_ENABLE_ZLIB_ENCODING
DEFINES	+= MUSCLE_AVOID_IPV6
DEFINES	+= MUSCLE_AVOID_CPLUSPLUS11
#DEFINES  += MUSCLE_ENABLE_MEMORY_TRACKING
#DEFINES  += MUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
#DEFINES  += MUSCLE_USE_POLL
#DEFINES  += MUSCLE_USE_KQUEUE
#DEFINES  += MUSCLE_USE_EPOLL
#DEFINES  += MUSCLE_ENABLE_SSL


#	specify special warning levels
#	if unspecified default warnings will be used
#	NONE = supress all warnings
#	ALL = enable all warnings
WARNINGS = ALL

#	specify whether image symbols will be created
#	so that stack crawls in the debugger are meaningful
#	if TRUE symbols will be created
SYMBOLS = 

#	specify debug settings
#	if TRUE will allow application to be run from a source-level
#	debugger.  Note that this will disable all optimzation.
DEBUGGER =

#	specify additional compiler flags for all files
COMPILER_FLAGS = -fno-exceptions -DMUSCLE_NO_EXCEPTIONS -W -Wall -Wno-multichar
#COMPILER_FLAGS += -O3

#	specify additional linker flags
LINKER_FLAGS =

#	specify the version of this particular item
#	(for example, -app 3 4 0 d 0 -short 340 -long "340 "`echo -n -e '\302\251'`"1999 GNU GPL") 
#	This may also be specified in a resource.
APP_VERSION = 

#	(for TYPE == DRIVER only) Specify desired location of driver in the /dev
#	hierarchy. Used by the driverinstall rule. E.g., DRIVER_PATH = video/usb will
#	instruct the driverinstall rule to place a symlink to your driver's binary in
#	~/add-ons/kernel/drivers/dev/video/usb, so that your driver will appear at
#	/dev/video/usb when loaded. Default is "misc".
DRIVER_PATH = 

## include the makefile-engine
DEVEL_DIRECTORY := \
	$(shell findpaths -r "makefile_engine" B_FIND_PATH_DEVELOP_DIRECTORY)
include $(DEVEL_DIRECTORY)/etc/makefile-engine
