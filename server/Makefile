# compilation flags used under any OS or compiler (may be appended to, below)
CXXFLAGS  += -I..
CXXFLAGS  += -DMUSCLE_ENABLE_ZLIB_ENCODING
CXXFLAGS  += -std=c++11 $(CFLAGS) $(DEFINES)
CXXFLAGS  += -W -Wall -Wunused-result
#CXXFLAGS += -DMUSCLE_SINGLE_THREAD_ONLY
#CXXFLAGS += -DMUSCLE_AVOID_IPV6
#CXXFLAGS += -DMUSCLE_AVOID_BITSTUFFING
#CXXFLAGS += -DMUSCLE_INCLUDE_SOURCE_LOCATION_IN_LOGTIME
#CXXFLAGS += -DMUSCLE_USE_POLL
#CXXFLAGS += -DMUSCLE_USE_KQUEUE
#CXXFLAGS += -DMUSCLE_USE_EPOLL
#CXXFLAGS += -DMUSCLE_ENABLE_SSL
#CXXFLAGS += -DMUSCLE_AVOID_CPLUSPLUS11
#CXXFLAGS += -DMUSCLE_ENABLE_MEMORY_TRACKING
#CXXFLAGS += -DMUSCLE_AVOID_STDINT
#CXXFLAGS += -DMUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT

# run "ENABLECATALYST=1 make -j4 libmuscle.a" if you want to build libmuscle.a for Catalyst
ifneq ($(ENABLECATALYST),)
   CATALYSTFLAGS = -target x86_64-apple-ios13.0-macabi -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk -isystem /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk/System/iOSSupport/usr/include -iframework /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.15.sdk/System/iOSSupport/System/Library/Frameworks
   CFLAGS   += $(CATALYSTFLAGS)
   CXXFLAGS += $(CATALYSTFLAGS)
endif

# compilation flags that are specific to the gcc compiler (hard-coded)
GCCFLAGS    = -fno-exceptions -DMUSCLE_NO_EXCEPTIONS -Wno-multichar

# flags to include when compiling the optimized version (with 'make')
CCOPTFLAGS  = -O3

# flags to include when linking (set per operating system, below)
LFLAGS      =

# libraries to include when linking (set per operating system, below)
LIBS        =

# names of the executables to compile
EXECUTABLES = muscled admin

# object files to include in all executables
OBJFILES = Message.o AbstractMessageIOGateway.o TemplatingMessageIOGateway.o MessageIOGateway.o String.o StringTokenizer.o AbstractReflectSession.o SignalMultiplexer.o SignalHandlerSession.o DumbReflectSession.o StorageReflectSession.o DataNode.o ReflectServer.o SocketMultiplexer.o SegmentedStringMatcher.o StringMatcher.o MiscUtilityFunctions.o NetworkUtilityFunctions.o StackTrace.o SysLog.o PulseNode.o PathMatcher.o FilterSessionFactory.o RateLimitSessionIOPolicy.o MemoryAllocator.o GlobalMemoryAllocator.o SetupSystem.o ServerComponent.o ZLibCodec.o ByteBuffer.o QueryFilter.o Directory.o FilePathInfo.o ChildProcessDataIO.o FileDataIO.o TCPSocketDataIO.o UDPSocketDataIO.o StdinDataIO.o PlainTextMessageIOGateway.o RawDataMessageIOGateway.o FileDescriptorDataIO.o SystemInfo.o regcomp.o regerror.o regexec.o regfree.o ReaderWriterMutex.o SharedMemory.o ZipFileUtilityFunctions.o ZLibUtilityFunctions.o CPULoadMeter.o TarFileWriter.o
ZLIBOBJS = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o crc32.o compress.o gzclose.o gzread.o gzwrite.o gzlib.o zip.o unzip.o ioapi.o

# These files aren't used by muscled, but some of the muscle-by-example programs need them to be in libmuscle.a
ifeq (,$(findstring MUSCLE_SINGLE_THREAD_ONLY,$(CXXFLAGS)))
   EXTRAFILES = MessageTransceiverThread.o DetectNetworkConfigChangesSession.o Thread.o
endif

# Where to find .cpp files
VPATH = ../message ../besupport ../dataio ../iogateway ../reflector ../regex ../util ../syslog ../system ../zlib ../zlib/zlib ../zlib/zlib/contrib/minizip ../regex/regex

# if the OS type variable is unset, try to set it using the uname shell command
ifeq ($(OSTYPE),)
  OSTYPE = $(strip $(shell uname))
endif

ifeq ($(OSTYPE),Darwin)
   CXXFLAGS += -stdlib=libc++
   LIBS += -framework SystemConfiguration -framework CoreFoundation -framework IOKit
   ifdef BUILD_UNIVERSAL_BINARIES
      ARCH_FLAGS =-arch x86_64 -arch arm64
      CFLAGS    += $(ARCH_FLAGS)
      CXXFLAGS  += $(ARCH_FLAGS)
      LFLAGS    += $(ARCH_FLAGS)
   endif
endif

ifeq ($(OSTYPE),Haiku)
   LIBS = -lbe -lnetwork -lroot
   CXXFLAGS  += -DMUSCLE_AVOID_IPV6
endif

ifeq ($(OSTYPE),Linux)
   CFLAGS   += -I../zlib/zlib -pthread
   LIBS     += -lutil -pthread
endif

ifeq ($(OSTYPE),linux)
   CFLAGS   += -I../zlib/zlib -pthread
   LIBS     += -lutil -pthread
endif

ifneq (,$(findstring g++,$(CXX)))
   CXXFLAGS += $(GCCFLAGS) $(CCOPTFLAGS)
else
   CXXFLAGS += $(CCOPTFLAGS)
endif

ifeq ($(OSTYPE),freebsd4.0)
   CXXFLAGS += -I/usr/include/machine
endif

ifeq ($(OSTYPE),DragonFly)
   # void
endif

ifneq (,$(findstring MUSCLE_ENABLE_SSL,$(CXXFLAGS))) # Add OpenSSL-specific files only if OpenSSL is enabled
	OBJFILES += SSLSocketDataIO.o SSLSocketAdapterGateway.o
	LIBS += -lssl -lcrypto
endif

all : $(EXECUTABLES) libmuscle.a

debug : CCOPTFLAGS = -g
debug : all

muscled : muscled.o libmuscle.a
	$(CXX) -o $@ $^ $(LIBS) $(LFLAGS)

admin : admin.o libmuscle.a
	$(CXX) -o $@ $^ $(LIBS) $(LFLAGS)

libmuscle.a : $(OBJFILES) $(ZLIBOBJS) $(EXTRAFILES)
	$(AR) rcs $@ $^

clean :
	rm -f *.o *.xSYM $(EXECUTABLES) libmuscle.a

