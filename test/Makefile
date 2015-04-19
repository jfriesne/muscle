DEFINES += -DMUSCLE_ENABLE_ZLIB_ENCODING
DEFINES += -DMUSCLE_USE_PTHREADS
DEFINES += -DMUSCLE_AVOID_SIGNAL_HANDLING
#DEFINES += -DMUSCLE_AVOID_NEWNOTHROW
#DEFINES += -DMUSCLE_USE_MUTEXES_FOR_ATOMIC_OPERATIONS
#DEFINES += -DMUSCLE_ENABLE_DEADLOCK_FINDER
#DEFINES += -DMUSCLE_AVOID_THREAD_SAFE_HASHTABLE_ITERATORS
#DEFINES += -DMUSCLE_AVOID_IPV6
#DEFINES += -DMUSCLE_AVOID_BITSTUFFING
#DEFINES += -DMUSCLE_AVOID_MULTICAST_API
#DEFINES += -DMUSCLE_POOL_SLAB_SIZE=0
#DEFINES += -DMUSCLE_WARN_ABOUT_LOUSY_HASH_FUNCTIONS
#DEFINES += -DMUSCLE_USE_POLL
#DEFINES += -DMUSCLE_USE_KQUEUE
#DEFINES += -DMUSCLE_USE_EPOLL
#DEFINES += -DMUSCLE_AVOID_MINIMIZED_HASHTABLES
#DEFINES += -DMUSCLE_ENABLE_SSL

CFLAGS = -I.. -I../zlib/zlib -g
GCCFLAGS = -fno-exceptions -DMUSCLE_NO_EXCEPTIONS -W -Wall -Wno-multichar
CXXFLAGS = $(CFLAGS) -O3 $(DEFINES)

# Uncomment this to enable C++11 support
#CXXFLAGS += -std=c++11 -stdlib=libc++ -DMUSCLE_USE_CPLUSPLUS11 -DMUSCLE_COUNT_STRING_COPY_OPERATIONS

# Uncomment these to enable clang++'s static analyzer (for more thorough error checking)
#CXXFLAGS += --analyze -Xanalyzer -analyzer-output=text
#CFLAGS   += --analyze -Xanalyzer -analyzer-output=text

LFLAGS =
LIBS =  -lpthread
EXECUTABLES = testhashtable microchatclient testmini testfilepathinfo testmicro microreflectclient minireflectclient minichatclient testmessage testzip testrefcount testqueue testtuple testgateway calctypecode printtypecode portablereflectclient portscan testudp testsocketmultiplexer testpackettunnel testpacketio teststring testbytebuffer testmatchfiles testparsefile testtime deadlockfinder deadlock testendian testsysteminfo portableplaintextclient uploadstress bandwidthtester readmessage testregex testnagle testresponse testqueryfilter testtypedefs hexterm udpproxy serialproxy printsourcelocations findsourcelocations svncopy testserial chatclient testpulsenode testnetconfigdetect testnetutil testpool testbatchguard testthread testthreadpool testobjectpool
REGEXOBJS =
ZLIBOBJS = adler32.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o crc32.o compress.o gzclose.o gzread.o gzwrite.o gzlib.o
ZIPOBJS = zip.o unzip.o ioapi.o
STDOBJS = $(ZLIBOBJS) FilePathInfo.o Directory.o StringMatcher.o

VPATH = ../hashtable ../message ../iogateway ../reflector ../regex ../util ../syslog ../system ../dataio ../zlib ../zlib/zlib ../zlib/zlib/contrib/minizip ../minimessage ../micromessage

# if the OS type variable is unset, try to set it using the uname shell command
ifeq ($(OSTYPE),)
  OSTYPE = $(strip $(shell uname))
endif

# IRIX may report itself as IRIX or as IRIX64.  They are both the same to us.
ifeq ($(OSTYPE),IRIX64)
  OSTYPE = IRIX
endif

ifeq ($(OSTYPE),beos)
   VPATH += ../besupport
   EXECUTABLES += testbesupport
   ifeq ($(BE_HOST_CPU),ppc)
      CXX = mwcc
      OBJFILES += regcomp.o regerror.o regexec.o regfree.o
      VPATH += ../regex/regex
      CFLAGS += -I../regex/regex -I../zlib/zlib
      MEMORY_TRACKING_SUPPORTED = no  # mwcc can't handle it correctly :^(
   else # not ppc
      CXXFLAGS += $(GCCFLAGS) $(CCOPTFLAGS)
      LIBS = -lbe -lnet -lroot
      ifeq ($(shell ls 2>/dev/null -1 /boot/develop/headers/be/bone/bone_api.h), /boot/develop/headers/be/bone/bone_api.h)
         CFLAGS += -I/boot/develop/headers/be/bone -DBONE
         LIBS = -nodefaultlibs -lbind -lsocket -lbe -lroot -L/boot/beos/system/lib
      endif
   endif
else # not beos
    ifneq (,$(findstring g++,$(CXX)))
        CXXFLAGS += $(GCCFLAGS) $(CCOPTFLAGS)
    else
        CXXFLAGS += $(CCOPTFLAGS)
    endif
endif

ifeq ($(OSTYPE),Haiku)
   VPATH       += ../besupport
   CXXFLAGS    += -DMUSCLE_AVOID_IPV6
   EXECUTABLES += testbesupport
   LIBS         = -lbe -lnetwork -lroot
else
   EXECUTABLES += testchildprocess
endif

ifeq ($(SYSTEM),AtheOS)
   VPATH += ../atheossupport
   EXECUTABLES += testatheossupport
   LIBS = -latheos
endif

ifeq ($(OSTYPE),freebsd4.0)
   CXXFLAGS += -I/usr/include/machine
endif

ifeq ($(OSTYPE),DragonFly)
   # void
endif

ifeq ($(OSTYPE),Darwin)
   LIBS += -framework IOKit -framework Carbon -framework SystemConfiguration
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

HEXTERMOBJS = hexterm.o NetworkUtilityFunctions.o SocketMultiplexer.o SysLog.o SetupSystem.o String.o RS232DataIO.o ChildProcessDataIO.o ByteBuffer.o MiscUtilityFunctions.o Message.o StdinDataIO.o PlainTextMessageIOGateway.o AbstractMessageIOGateway.o FileDescriptorDataIO.o PulseNode.o

ifeq ($(MUSCLE_BUILD_CONTEXT),meyer)
   CXXFLAGS    += -DBUILD_MUSCLE_IN_MEYER_CONTEXT -I../../..
   CXXFLAGS    += -DLCS_RELEASE_IS_FULLY_QUALIFIED=$(LCS_RELEASE_IS_FULLY_QUALIFIED)
   CXXFLAGS    += -DSVN_VERSION_STRING="$(SVN_REVISION)"
   VPATH       += ../../../version/
   HEXTERMOBJS += dmitri_version.o
endif

# Makes all the programs that can be made using just cross-platform code
all : $(EXECUTABLES)

testatheossupport : $(STDOBJS) ConvertMessages.o Message.o String.o testatheossupport.o SysLog.o SetupSystem.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testbesupport : $(STDOBJS) ConvertMessages.o Message.o String.o testbesupport.o SysLog.o SetupSystem.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

calctypecode : $(STDOBJS) calctypecode.o SysLog.o SetupSystem.o String.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

printtypecode : $(STDOBJS) printtypecode.o SysLog.o SetupSystem.o String.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testhashtable : $(STDOBJS) String.o testhashtable.o SysLog.o SocketMultiplexer.o NetworkUtilityFunctions.o SetupSystem.o Message.o SetupSystem.o MiscUtilityFunctions.o ByteBuffer.o Thread.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmessage : $(STDOBJS) Message.o String.o testmessage.o SysLog.o ByteBuffer.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testfilepathinfo : $(STDOBJS) testfilepathinfo.o FilePathInfo.o SetupSystem.o String.o SysLog.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testthread : $(STDOBJS) testthread.o SetupSystem.o Message.o String.o ByteBuffer.o SysLog.o Thread.o SocketMultiplexer.o NetworkUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testthreadpool : $(STDOBJS) testthreadpool.o SetupSystem.o Message.o String.o ByteBuffer.o SysLog.o Thread.o ThreadPool.o SocketMultiplexer.o NetworkUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpool : $(STDOBJS) Message.o String.o testpool.o SysLog.o ByteBuffer.o SetupSystem.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testbatchguard : $(STDOBJS) Message.o String.o testbatchguard.o SysLog.o ByteBuffer.o SetupSystem.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testzip : $(STDOBJS) Message.o String.o testzip.o SysLog.o ByteBuffer.o SetupSystem.o ZipFileUtilityFunctions.o MiscUtilityFunctions.o SocketMultiplexer.o NetworkUtilityFunctions.o $(ZIPOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmicro : $(STDOBJS) Message.o String.o testmicro.o SysLog.o ByteBuffer.o SetupSystem.o MicroMessage.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

microchatclient : microchatclient.o MicroMessage.o MicroMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

microreflectclient : microreflectclient.o MicroMessage.o MicroMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmini : $(STDOBJS) Message.o String.o testmini.o SysLog.o ByteBuffer.o SetupSystem.o MiniMessage.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

minireflectclient : minireflectclient.o MiniMessage.o MiniMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

minichatclient : minichatclient.o MiniMessage.o MiniMessageGateway.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

readmessage : $(STDOBJS) Message.o String.o readmessage.o SysLog.o ByteBuffer.o SetupSystem.o ZLibUtilityFunctions.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testresponse : $(STDOBJS) testresponse.o MessageIOGateway.o AbstractMessageIOGateway.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o SetupSystem.o PulseNode.o Message.o String.o ByteBuffer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testnagle : $(STDOBJS) testnagle.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o SetupSystem.o String.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testrefcount : $(STDOBJS) testrefcount.o SysLog.o String.o SetupSystem.o MiscUtilityFunctions.o Thread.o Message.o ByteBuffer.o SocketMultiplexer.o NetworkUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

teststring : $(STDOBJS) teststring.o SysLog.o String.o SocketMultiplexer.o NetworkUtilityFunctions.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testbytebuffer : $(STDOBJS) testbytebuffer.o SysLog.o String.o SocketMultiplexer.o NetworkUtilityFunctions.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testmatchfiles : $(STDOBJS) testmatchfiles.o SysLog.o String.o SocketMultiplexer.o NetworkUtilityFunctions.o SetupSystem.o MiscUtilityFunctions.o Message.o ByteBuffer.o StringMatcher.o FilePathExpander.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testparsefile : $(STDOBJS) testparsefile.o SysLog.o String.o MiscUtilityFunctions.o SetupSystem.o Message.o ByteBuffer.o SocketMultiplexer.o NetworkUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testtime : $(STDOBJS) testtime.o SysLog.o String.o SocketMultiplexer.o NetworkUtilityFunctions.o SetupSystem.o MiscUtilityFunctions.o ByteBuffer.o Message.o MiscUtilityFunctions.o

	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testendian : $(STDOBJS) testendian.o MiscUtilityFunctions.o ByteBuffer.o String.o SysLog.o SocketMultiplexer.o NetworkUtilityFunctions.o SetupSystem.o Message.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testsysteminfo : $(STDOBJS) testsysteminfo.o String.o SetupSystem.o SystemInfo.o SysLog.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testqueue : $(STDOBJS) testqueue.o SysLog.o String.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testtuple : $(STDOBJS) testtuple.o String.o SysLog.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testserial : $(STDOBJS) testserial.o SysLog.o RS232DataIO.o String.o SetupSystem.o SocketMultiplexer.o NetworkUtilityFunctions.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testgateway : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o testgateway.o SysLog.o PulseNode.o SetupSystem.o ZLibDataIO.o ZLibCodec.o ByteBuffer.o SocketMultiplexer.o NetworkUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testregex : $(STDOBJS) testregex.o String.o SysLog.o SetupSystem.o $(REGEXOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

portablereflectclient : $(STDOBJS) $(SSLOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o portablereflectclient.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o StdinDataIO.o FileDescriptorDataIO.o MiscUtilityFunctions.o PlainTextMessageIOGateway.o QueryFilter.o $(REGEXOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

portscan : $(STDOBJS) portscan.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o SetupSystem.o String.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

chatclient : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o chatclient.o MiscUtilityFunctions.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o PathMatcher.o QueryFilter.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testudp : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o RawDataMessageIOGateway.o String.o testudp.o SocketMultiplexer.o MiscUtilityFunctions.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testsocketmultiplexer : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o testsocketmultiplexer.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpackettunnel : $(STDOBJS) testpackettunnel.o Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o PacketTunnelIOGateway.o PacketizedDataIO.o MiscUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpacketio : $(STDOBJS) testpacketio.o Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o SetupSystem.o PacketizedDataIO.o MiscUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

bandwidthtester : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o bandwidthtester.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

uploadstress : $(STDOBJS) Message.o AbstractMessageIOGateway.o MessageIOGateway.o String.o uploadstress.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

portableplaintextclient : $(STDOBJS) Message.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o String.o portableplaintextclient.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testchildprocess : $(STDOBJS) Message.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o String.o MiscUtilityFunctions.o ChildProcessDataIO.o testchildprocess.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o PulseNode.o SetupSystem.o ByteBuffer.o ZLibCodec.o StdinDataIO.o FileDescriptorDataIO.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

hexterm : $(STDOBJS) $(HEXTERMOBJS)
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

serialproxy : $(STDOBJS) serialproxy.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o SetupSystem.o String.o RS232DataIO.o ByteBuffer.o MiscUtilityFunctions.o Message.o AbstractMessageIOGateway.o PulseNode.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

udpproxy : $(STDOBJS) udpproxy.o SocketMultiplexer.o NetworkUtilityFunctions.o SysLog.o SetupSystem.o String.o ByteBuffer.o MiscUtilityFunctions.o Message.o AbstractMessageIOGateway.o PulseNode.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

findsourcelocations : $(STDOBJS) findsourcelocations.o SysLog.o SetupSystem.o String.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o PulseNode.o Message.o ByteBuffer.o SocketMultiplexer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

printsourcelocations : $(STDOBJS) printsourcelocations.o SysLog.o SetupSystem.o String.o AbstractMessageIOGateway.o PlainTextMessageIOGateway.o PulseNode.o Message.o ByteBuffer.o SocketMultiplexer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testsharedmem: $(STDOBJS) SysLog.o SharedMemory.o SocketMultiplexer.o NetworkUtilityFunctions.o testsharedmem.o String.o MiscUtilityFunctions.o SetupSystem.o ByteBuffer.o Message.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testobjectpool: $(STDOBJS) SysLog.o SharedMemory.o SocketMultiplexer.o NetworkUtilityFunctions.o testobjectpool.o String.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testqueryfilter: $(STDOBJS) SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o testqueryfilter.o SetupSystem.o MiscUtilityFunctions.o SocketMultiplexer.o NetworkUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testpulsenode:  $(STDOBJS) $(SSLOBJS) SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o SetupSystem.o MiscUtilityFunctions.o AbstractReflectSession.o PulseNode.o ReflectServer.o SocketMultiplexer.o AbstractMessageIOGateway.o ServerComponent.o SocketMultiplexer.o NetworkUtilityFunctions.o MessageIOGateway.o ZLibCodec.o testpulsenode.o ByteBuffer.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testnetconfigdetect:  $(STDOBJS) $(SSLOBJS) SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o SetupSystem.o MiscUtilityFunctions.o SocketMultiplexer.o NetworkUtilityFunctions.o AbstractReflectSession.o PulseNode.o ReflectServer.o SocketMultiplexer.o AbstractMessageIOGateway.o DetectNetworkConfigChangesSession.o ServerComponent.o MessageIOGateway.o ZLibCodec.o Thread.o testnetconfigdetect.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

testnetutil:  $(STDOBJS) $(SSLOBJS) SysLog.o ByteBuffer.o Message.o QueryFilter.o String.o SetupSystem.o MiscUtilityFunctions.o SocketMultiplexer.o NetworkUtilityFunctions.o AbstractReflectSession.o PulseNode.o ReflectServer.o SocketMultiplexer.o AbstractMessageIOGateway.o DetectNetworkConfigChangesSession.o ServerComponent.o MessageIOGateway.o ZLibCodec.o Thread.o testnetutil.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

deadlockfinder : $(STDOBJS) deadlockfinder.o SysLog.o String.o SetupSystem.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

deadlock: $(STDOBJS) deadlock.o SysLog.o String.o SetupSystem.o Thread.o SocketMultiplexer.o NetworkUtilityFunctions.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

svncopy : $(STDOBJS) svncopy.o String.o SetupSystem.o SysLog.o
	$(CXX) $(LFLAGS) -o $@ $^ $(LIBS)

clean :
	rm -f *.o *.xSYM $(EXECUTABLES)
