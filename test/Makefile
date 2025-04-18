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

#CXXFLAGS += -fsanitize=thread -g
#LFLAGS   += -fsanitize=thread

#CXXFLAGS += -fsanitize=address,undefined -g
#LFLAGS   += -fsanitize=address,undefined

EXECUTABLES = testhashtable testmini testfilepathinfo testmicro testmessage testclone testzip testtar testrefcount testqueue teststringtokenizer testtuple testgateway testudp testsocketmultiplexer testpackettunnel testpacketio teststatus teststring testbitchord testhashcodes testbytebuffer testmatchfiles testparsefile testtime testtimeunitconversions testendian testsysteminfo testregex testnagle testresponse testqueryfilter testtypedefs testserial testpulsenode testnetconfigdetect testnetutil testpool testatomicvalue testbatchguard testthread testserverthread testreaderwritermutex testthreadpool testobjectpool testchildprocess testsharedmem

REGEXOBJS =
ZLIBOBJS = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o crc32.o compress.o gzclose.o gzread.o gzwrite.o gzlib.o
ZIPOBJS = zip.o unzip.o ioapi.o
STDOBJS = $(ZLIBOBJS) FilePathInfo.o Directory.o StringMatcher.o FileDataIO.o TCPSocketDataIO.o UDPSocketDataIO.o NetworkUtilityFunctions.o SocketMultiplexer.o StringTokenizer.o

VPATH = ../hashtable ../message ../iogateway ../reflector ../regex ../util ../syslog ../system ../dataio ../zlib ../zlib/zlib ../zlib/zlib/contrib/minizip ../lang/c/minimessage ../lang/c/micromessage

# if the OS type variable is unset, try to set it using the uname shell command
ifeq ($(OSTYPE),)
  OSTYPE = $(strip $(shell uname))
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

SSLOBJS =
ifneq (,$(findstring MUSCLE_ENABLE_SSL,$(CXXFLAGS))) # Add OpenSSL-specific files only if OpenSSL is enabled
       SSLOBJS += SSLSocketDataIO.o SSLSocketAdapterGateway.o
       LIBS += -lssl -lcrypto
endif

HEXTERMOBJS = hexterm.o StackTrace.o SysLog.o SetupSystem.o String.o RS232DataIO.o ChildProcessDataIO.o ByteBuffer.o MiscUtilityFunctions.o Message.o StdinDataIO.o PlainTextMessageIOGateway.o AbstractMessageIOGateway.o FileDescriptorDataIO.o PulseNode.o SimulatedMulticastDataIO.o Thread.o ZLibDataIO.o

ifeq ($(MUSCLE_BUILD_CONTEXT),meyer)
   CXXFLAGS    += -DBUILD_MUSCLE_IN_MEYER_CONTEXT -I../../../libs/libmslicommon/include
   CXXFLAGS    += -DMEYER_RELEASE_IS_FULLY_QUALIFIED=$(MEYER_RELEASE_IS_FULLY_QUALIFIED)
   CXXFLAGS    += -DSVN_VERSION_STRING="$(SVN_REVISION)"
   VPATH       += ../../../libs/libmslicommon/src/version/
   HEXTERMOBJS += CoreVersionInfo.o
endif

# Makes all the programs that can be made using just cross-platform code
all : $(EXECUTABLES)

testhashtable : $(STDOBJS) String.o testhashtable.o StackTrace.o SysLog.o SetupSystem.o Message.o SetupSystem.o MiscUtilityFunctions.o ByteBuffer.o Thread.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testclone : $(STDOBJS) String.o testclone.o StackTrace.o SysLog.o SetupSystem.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmessage : $(STDOBJS) Message.o String.o testmessage.o StackTrace.o SysLog.o ByteBuffer.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testfilepathinfo : $(STDOBJS) testfilepathinfo.o FilePathInfo.o SetupSystem.o String.o StackTrace.o SysLog.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testthread : $(STDOBJS) testthread.o SetupSystem.o Message.o String.o ByteBuffer.o StackTrace.o SysLog.o Thread.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testserverthread : $(STDOBJS) testserverthread.o SetupSystem.o Message.o String.o ByteBuffer.o StackTrace.o SysLog.o Thread.o AbstractReflectSession.o AbstractMessageIOGateway.o MessageIOGateway.o TemplatingMessageIOGateway.o PulseNode.o ServerComponent.o ReflectServer.o ZLibCodec.o MiscUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testreaderwritermutex : $(STDOBJS) testreaderwritermutex.o SetupSystem.o Message.o MiscUtilityFunctions.o String.o ByteBuffer.o StackTrace.o SysLog.o Thread.o AbstractReflectSession.o AbstractMessageIOGateway.o MessageIOGateway.o ReaderWriterMutex.o TemplatingMessageIOGateway.o PulseNode.o ServerComponent.o ReflectServer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testthreadpool : $(STDOBJS) testthreadpool.o SetupSystem.o Message.o String.o ByteBuffer.o StackTrace.o SysLog.o Thread.o ThreadPool.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpool : $(STDOBJS) Message.o String.o testpool.o StackTrace.o SysLog.o ByteBuffer.o SetupSystem.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testatomicvalue : $(STDOBJS) testatomicvalue.o SetupSystem.o Message.o String.o ByteBuffer.o StackTrace.o SysLog.o Thread.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testbatchguard : $(STDOBJS) Message.o String.o testbatchguard.o StackTrace.o SysLog.o ByteBuffer.o SetupSystem.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testzip : $(STDOBJS) Message.o String.o testzip.o StackTrace.o SysLog.o ByteBuffer.o SetupSystem.o ZipFileUtilityFunctions.o MiscUtilityFunctions.o $(ZIPOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testtar : $(STDOBJS) Message.o String.o testtar.o StackTrace.o SysLog.o ByteBuffer.o SetupSystem.o ZipFileUtilityFunctions.o MiscUtilityFunctions.o TarFileWriter.o ZLibDataIO.o $(ZIPOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmicro : $(STDOBJS) Message.o String.o testmicro.o StackTrace.o SysLog.o ByteBuffer.o SetupSystem.o MicroMessage.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmini : $(STDOBJS) Message.o String.o testmini.o StackTrace.o SysLog.o ByteBuffer.o SetupSystem.o MiniMessage.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testresponse : $(STDOBJS) testresponse.o MessageIOGateway.o AbstractMessageIOGateway.o StackTrace.o SysLog.o SetupSystem.o PulseNode.o Message.o String.o ByteBuffer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testnagle : $(STDOBJS) testnagle.o StackTrace.o SysLog.o SetupSystem.o String.o ByteBuffer.o Thread.o Message.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testrefcount : $(STDOBJS) testrefcount.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Thread.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

teststatus : teststatus.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o FileDataIO.o StringTokenizer.o FilePathInfo.o StringMatcher.o Directory.o NetworkUtilityFunctions.o SocketMultiplexer.o $(ZLIBOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testtimeunitconversions : testtimeunitconversions.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o FileDataIO.o StringTokenizer.o FilePathInfo.o StringMatcher.o Directory.o NetworkUtilityFunctions.o SocketMultiplexer.o $(ZLIBOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testtypedefs : testtypedefs.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o FileDataIO.o StringTokenizer.o FilePathInfo.o StringMatcher.o Directory.o NetworkUtilityFunctions.o SocketMultiplexer.o $(ZLIBOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

teststring : $(STDOBJS) teststring.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

teststringtokenizer : $(STDOBJS) teststringtokenizer.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testbitchord : $(STDOBJS) testbitchord.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testhashcodes : $(STDOBJS) testhashcodes.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testbytebuffer : $(STDOBJS) testbytebuffer.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmatchfiles : $(STDOBJS) testmatchfiles.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o StringMatcher.o FilePathExpander.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testparsefile : $(STDOBJS) testparsefile.o StackTrace.o SysLog.o String.o MiscUtilityFunctions.o SetupSystem.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testtime : $(STDOBJS) testtime.o StackTrace.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o ByteBuffer.o Message.o MiscUtilityFunctions.o

	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testendian : $(STDOBJS) testendian.o MiscUtilityFunctions.o ByteBuffer.o String.o StackTrace.o SysLog.o SetupSystem.o Message.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testsysteminfo : $(STDOBJS) testsysteminfo.o String.o SetupSystem.o SystemInfo.o StackTrace.o SysLog.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testqueue : $(STDOBJS) testqueue.o StackTrace.o SysLog.o String.o SetupSystem.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testtuple : $(STDOBJS) testtuple.o String.o StackTrace.o SysLog.o SetupSystem.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testserial : $(STDOBJS) testserial.o StackTrace.o SysLog.o RS232DataIO.o String.o SetupSystem.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testgateway : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o testgateway.o StackTrace.o SysLog.o PulseNode.o SetupSystem.o ZLibDataIO.o ZLibCodec.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testregex : $(STDOBJS) testregex.o String.o StackTrace.o SysLog.o SetupSystem.o ByteBuffer.o $(REGEXOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testudp : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o PlainTextMessageIOGateway.o RawDataMessageIOGateway.o String.o testudp.o MiscUtilityFunctions.o StackTrace.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o StdinDataIO.o FileDescriptorDataIO.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testsocketmultiplexer : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o testsocketmultiplexer.o StackTrace.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpackettunnel : $(STDOBJS) testpackettunnel.o Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o StackTrace.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o ProxyIOGateway.o PacketTunnelIOGateway.o MiniPacketTunnelIOGateway.o PacketizedProxyDataIO.o MiscUtilityFunctions.o ByteBufferPacketDataIO.o ByteBufferDataIO.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpacketio : $(STDOBJS) testpacketio.o Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o StackTrace.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o PacketizedProxyDataIO.o MiscUtilityFunctions.o ByteBufferPacketDataIO.o ByteBufferDataIO.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testchildprocess : $(STDOBJS) Message.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o String.o MiscUtilityFunctions.o ChildProcessDataIO.o testchildprocess.o StackTrace.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o StdinDataIO.o FileDescriptorDataIO.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testsharedmem: $(STDOBJS) StackTrace.o SysLog.o SharedMemory.o testsharedmem.o String.o MiscUtilityFunctions.o SetupSystem.o ByteBuffer.o Message.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testobjectpool: $(STDOBJS) StackTrace.o SysLog.o SharedMemory.o testobjectpool.o String.o SetupSystem.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testqueryfilter: $(STDOBJS) StackTrace.o SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o testqueryfilter.o SetupSystem.o MiscUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpulsenode:  $(STDOBJS) $(SSLOBJS) StackTrace.o SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o SetupSystem.o MiscUtilityFunctions.o AbstractReflectSession.o PulseNode.o ReflectServer.o AbstractMessageIOGateway.o ServerComponent.o MessageIOGateway.o TemplatingMessageIOGateway.o ZLibCodec.o testpulsenode.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testnetconfigdetect:  $(STDOBJS) $(SSLOBJS) StackTrace.o SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o SetupSystem.o MiscUtilityFunctions.o AbstractReflectSession.o PulseNode.o ReflectServer.o AbstractMessageIOGateway.o DetectNetworkConfigChangesSession.o ServerComponent.o MessageIOGateway.o TemplatingMessageIOGateway.o ZLibCodec.o Thread.o testnetconfigdetect.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testnetutil:  $(STDOBJS) $(SSLOBJS) StackTrace.o SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o SetupSystem.o MiscUtilityFunctions.o AbstractReflectSession.o PulseNode.o ReflectServer.o AbstractMessageIOGateway.o DetectNetworkConfigChangesSession.o ServerComponent.o MessageIOGateway.o TemplatingMessageIOGateway.o ZLibCodec.o Thread.o testnetutil.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

clean :
	rm -f *.o *.xSYM $(EXECUTABLES)
