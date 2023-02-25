DEFINES += -DMUSCLE_ENABLE_ZLIB_ENCODING
DEFINES += -DMUSCLE_AVOID_SIGNAL_HANDLING
#DEFINES += -DMUSCLE_DISABLE_KEEPALIVE_API
#DEFINES += -DMUSCLE_AVOID_NEWNOTHROW
#DEFINES += -DMUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS
#DEFINES += -DMUSCLE_ENABLE_DEADLOCK_FINDER
#DEFINES += -DMUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
#DEFINES += -DMUSCLE_AVOID_IPV6
#DEFINES += -DMUSCLE_AVOID_TAGGED_POINTERS
#DEFINES += -DMUSCLE_AVOID_MULTICAST_API
#DEFINES += -DMUSCLE_POOL_SLAB_SIZE=0
#DEFINES += -DMUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS
#DEFINES += -DMUSCLE_USE_POLL
#DEFINES += -DMUSCLE_USE_KQUEUE
#DEFINES += -DMUSCLE_USE_EPOLL
#DEFINES += -DMUSCLE_AVOID_MINIMIZED_HASHTABLES
#DEFINES += -DMUSCLE_ENABLE_SSL
#DEFINES += -DMUSCLE_AVOID_LINUX_DETECT_NETWORK_HARDWARE_TYPES
#DEFINES += -DMUSCLE_AVOID_CPLUSPLUS11
#DEFINES += -DMUSCLE_AVOID_CPLUSPLUS11_THREADS
#DEFINES += -DMUSCLE_AVOID_CPLUSPLUS11_THREAD_LOCAL_KEYWORD
#DEFINES += -DMUSCLE_AVOID_CPLUSPLUS11_BITCHORD
#DEFINES += -DMUSCLE_USE_PTHREADS
#DEFINES += -DMUSCLE_COUNT_STRING_COPY_OPERATIONS
#DEFINES += -DMUSCLE_NO_EXCEPTIONS
#DEFINES += -DMUSCLE_AVOID_STDINT
#DEFINES += -DMUSCLE_ENABLE_AUTHORIZATION_EXECUTE_WITH_PRIVILEGES
#DEFINES += -DMUSCLE_USE_TEMPLATING_MESSAGE_IO_GATEWAY_BY_DEFAULT
#DEFINES += -DMUSCLE_DISABLE_LOGGING
#DEFINES += -DMUSCLE_MINIMALIST_LOGGING
#DEFINES += -DMUSCLE_FAKE_SHARED_MEMORY
#DEFINES += -DMUSCLE_SHAREDMEMORY_SEMOP_UNAVAILABLE
#DEFINES += -DMUSCLE_NUM_RESERVED_HIGH_BITS_IN_POINTERS=16

CFLAGS = -I.. -I../zlib/zlib -g
#CFLAGS += -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX12.1.sdk  # useful when compiling with scan-build/ClangSA

GCCFLAGS = -fno-exceptions -DMUSCLE_NO_EXCEPTIONS
CXXFLAGS = $(CFLAGS) -O3 $(DEFINES)
CXXFLAGS += -std=c++11 -W -Wall

LFLAGS =
LIBS =  -lpthread
EXECUTABLES = microchatclient microreflectclient minireflectclient minichatclient calctypecode printtypecode singlethreadedreflectclient multithreadedreflectclient muscleproxy portscan striphextermoutput deadlock portableplaintextclient uploadstress bandwidthtester readmessage daemonsitter hexterm udpproxy serialproxy printsourcelocations findsourcelocations chatclient snoopsharedmem erasesharedmem

REGEXOBJS =
ZLIBOBJS = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o crc32.o compress.o gzclose.o gzread.o gzwrite.o gzlib.o
ZIPOBJS = zip.o unzip.o ioapi.o
STDOBJS = $(ZLIBOBJS) FilePathInfo.o Directory.o StringMatcher.o FileDataIO.o TCPSocketDataIO.o UDPSocketDataIO.o NetworkUtilityFunctions.o SocketMultiplexer.o StringTokenizer.o

VPATH = ../hashtable ../message ../iogateway ../reflector ../regex ../util ../syslog ../system ../dataio ../zlib ../zlib/zlib ../zlib/zlib/contrib/minizip ../lang/c/minimessage ../lang/c/micromessage

# if the OS type variable is unset, try to set it using the uname shell command
ifeq ($(OSTYPE),)
  OSTYPE = $(strip $(shell uname))
endif

# IRIX may report itself as IRIX or as IRIX64.  They are both the same to us.
ifeq ($(OSTYPE),IRIX64)
  OSTYPE = IRIX
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

ifeq ($(OSTYPE),Darwin)
   LIBS += -framework IOKit -framework Carbon -framework SystemConfiguration -framework Security
   CXXFLAGS += -stdlib=libc++
   ifdef BUILD_UNIVERSAL_BINARIES
      ARCH_FLAGS = -arch x86_64 -arch arm64
      CFLAGS    += $(ARCH_FLAGS)
      CXXFLAGS  += $(ARCH_FLAGS)
      LFLAGS    += $(ARCH_FLAGS)
   endif
endif

ifeq ($(OSTYPE),linux)
   LIBS += -lutil
endif

ifeq ($(OSTYPE),Linux)
   LIBS += -lutil
endif

ifeq ($(OSTYPE),IRIX)
   CXXFLAGS += -DSGI -DMIPS
   ifneq (,$(findstring g++,$(CXX))) # if we are using SGI's CC compiler, we gotta change a few things
      CXX = CC
      CCFLAGS = -g2 -n32 -LANG:std -woff 1110,1174,1552,1460,3303
      LFLAGS  = -g2 -n32
      CXXFLAGS += -DNEW_H_NOT_AVAILABLE
      MEMORY_TRACKING_SUPPORTED = no # SGI CC can't handle it correctly :^(
   endif
endif

ifeq ($(MEMORY_TRACKING_SUPPORTED),yes)
   CXXFLAGS += -DMUSCLE_ENABLE_MEMORY_TRACKING
endif

SSLOBJS =
ifneq (,$(findstring MUSCLE_ENABLE_SSL,$(CXXFLAGS))) # Add OpenSSL-specific files only if OpenSSL is enabled
       SSLOBJS += SSLSocketDataIO.o SSLSocketAdapterGateway.o
       LIBS += -lssl -lcrypto
endif

HEXTERMOBJS = hexterm.o SysLog.o SetupSystem.o String.o RS232DataIO.o ChildProcessDataIO.o ByteBuffer.o MiscUtilityFunctions.o Message.o StdinDataIO.o PlainTextMessageIOGateway.o AbstractMessageIOGateway.o FileDescriptorDataIO.o PulseNode.o SimulatedMulticastDataIO.o Thread.o ZLibDataIO.o

ifeq ($(MUSCLE_BUILD_CONTEXT),meyer)
   CXXFLAGS    += -DBUILD_MUSCLE_IN_MEYER_CONTEXT -I../../../libs/libmslicommon/include
   CXXFLAGS    += -DMEYER_RELEASE_IS_FULLY_QUALIFIED=$(MEYER_RELEASE_IS_FULLY_QUALIFIED)
   CXXFLAGS    += -DSVN_VERSION_STRING="$(SVN_REVISION)"
   VPATH       += ../../../libs/libmslicommon/src/version/
   HEXTERMOBJS += core_version.o
endif

# Makes all the programs that can be made using just cross-platform code
all : $(EXECUTABLES)

calctypecode : $(STDOBJS) calctypecode.o SysLog.o SetupSystem.o String.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

printtypecode : $(STDOBJS) printtypecode.o SysLog.o SetupSystem.o String.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

microchatclient : microchatclient.o MicroMessage.o MicroMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

microreflectclient : microreflectclient.o MicroMessage.o MicroMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

minireflectclient : minireflectclient.o MiniMessage.o MiniMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

minichatclient : minichatclient.o MiniMessage.o MiniMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

readmessage : $(STDOBJS) Message.o String.o readmessage.o SysLog.o ByteBuffer.o SetupSystem.o ZLibUtilityFunctions.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

striphextermoutput : $(STDOBJS) striphextermoutput.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

singlethreadedreflectclient : $(STDOBJS) $(SSLOBJS) Message.o AbstractMessageIOGateway.o TemplatingMessageIOGateway.o MessageIOGateway.o String.o singlethreadedreflectclient.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o StdinDataIO.o FileDescriptorDataIO.o MiscUtilityFunctions.o PlainTextMessageIOGateway.o QueryFilter.o $(REGEXOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

multithreadedreflectclient : $(STDOBJS) $(SSLOBJS) Message.o Thread.o MessageTransceiverThread.o CallbackMessageTransceiverThread.o AbstractMessageIOGateway.o TemplatingMessageIOGateway.o MessageIOGateway.o String.o multithreadedreflectclient.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o StdinDataIO.o FileDescriptorDataIO.o MiscUtilityFunctions.o PlainTextMessageIOGateway.o DataNode.o PathMatcher.o QueryFilter.o ReflectServer.o ServerComponent.o AbstractReflectSession.o DumbReflectSession.o StorageReflectSession.o $(REGEXOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

muscleproxy : $(STDOBJS) $(SSLOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o TemplatingMessageIOGateway.o String.o muscleproxy.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o SetupSystem.o MiscUtilityFunctions.o PlainTextMessageIOGateway.o ReflectServer.o ServerComponent.o AbstractReflectSession.o ZLibCodec.o $(REGEXOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

portscan : $(STDOBJS) portscan.o SysLog.o SetupSystem.o String.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

chatclient : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o chatclient.o MiscUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o PathMatcher.o QueryFilter.o StdinDataIO.o FileDescriptorDataIO.o PlainTextMessageIOGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

bandwidthtester : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o bandwidthtester.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

uploadstress : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o uploadstress.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

portableplaintextclient : $(STDOBJS) Message.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o String.o portableplaintextclient.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o StdinDataIO.o FileDescriptorDataIO.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

daemonsitter : $(STDOBJS) Message.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o String.o MiscUtilityFunctions.o ChildProcessDataIO.o daemonsitter.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o StdinDataIO.o ReflectServer.o ServerComponent.o AbstractReflectSession.o MessageIOGateway.o TemplatingMessageIOGateway.o StdinDataIO.o FileDescriptorDataIO.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

hexterm : $(STDOBJS) $(HEXTERMOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

serialproxy : $(STDOBJS) serialproxy.o SysLog.o SetupSystem.o String.o RS232DataIO.o ByteBuffer.o MiscUtilityFunctions.o Message.o AbstractMessageIOGateway.o PulseNode.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

udpproxy : $(STDOBJS) udpproxy.o SysLog.o SetupSystem.o String.o ByteBuffer.o MiscUtilityFunctions.o Message.o AbstractMessageIOGateway.o PulseNode.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

findsourcelocations : $(STDOBJS) findsourcelocations.o SysLog.o SetupSystem.o String.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o PulseNode.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

printsourcelocations : $(STDOBJS) printsourcelocations.o SysLog.o SetupSystem.o String.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o PulseNode.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

snoopsharedmem: $(STDOBJS) SysLog.o SharedMemory.o snoopsharedmem.o String.o MiscUtilityFunctions.o SetupSystem.o ByteBuffer.o Message.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

erasesharedmem: $(STDOBJS) SysLog.o SharedMemory.o erasesharedmem.o String.o MiscUtilityFunctions.o SetupSystem.o ByteBuffer.o Message.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

deadlock: $(STDOBJS) deadlock.o SysLog.o String.o SetupSystem.o Thread.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

clean :
	rm -f *.o *.xSYM $(EXECUTABLES)
