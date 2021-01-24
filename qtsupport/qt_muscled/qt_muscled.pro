greaterThan(QT_MAJOR_VERSION, 4) {
   QT += widgets
}
win32:LIBS	+= shlwapi.lib ws2_32.lib winmm.lib User32.lib Advapi32.lib shell32.lib iphlpapi.lib version.lib
unix:!mac:LIBS	+= -lutil -lrt -lz
mac:LIBS        += -lz -framework Carbon -framework SystemConfiguration

win32:DEFINES += _WIN32_WINNT=0x0501 WINAPI_FAMILY=100

OBJECTS_DIR	= objects
MUSCLE_DIR	= ../../

DEFINES  += UNIFIED_DAEMON   # so we can have our own main() function, without clashing with the one inside muscled.cpp
DEFINES  += MUSCLE_ENABLE_ZLIB_ENCODING
DEFINES  += MUSCLE_SINGLE_THREAD_ONLY
#DEFINES += MUSCLE_AVOID_IPV6
unix:mac:QMAKE_CXXFLAGS += -std=c++11 -stdlib=libc++

INCLUDEPATH	+= $$MUSCLE_DIR
win32:INCLUDEPATH	+= $$MUSCLE_DIR/regex/regex $$MUSCLE_DIR/zlib/zlib/win32

SOURCES	+= $$MUSCLE_DIR/message/Message.cpp \
        $$MUSCLE_DIR/iogateway/MessageIOGateway.cpp \
        $$MUSCLE_DIR/iogateway/AbstractMessageIOGateway.cpp \
        $$MUSCLE_DIR/iogateway/PlainTextMessageIOGateway.cpp \
        $$MUSCLE_DIR/dataio/ChildProcessDataIO.cpp \
        $$MUSCLE_DIR/dataio/FileDataIO.cpp \
        $$MUSCLE_DIR/dataio/TCPSocketDataIO.cpp \
        $$MUSCLE_DIR/syslog/SysLog.cpp \
        $$MUSCLE_DIR/system/SetupSystem.cpp \
        $$MUSCLE_DIR/system/SignalMultiplexer.cpp \
        $$MUSCLE_DIR/system/GlobalMemoryAllocator.cpp \
        $$MUSCLE_DIR/zlib/ZLibCodec.cpp \
        $$MUSCLE_DIR/regex/StringMatcher.cpp \
        $$MUSCLE_DIR/regex/QueryFilter.cpp \
        $$MUSCLE_DIR/regex/PathMatcher.cpp \
        $$MUSCLE_DIR/regex/SegmentedStringMatcher.cpp \
        $$MUSCLE_DIR/reflector/AbstractReflectSession.cpp \
        $$MUSCLE_DIR/reflector/SignalHandlerSession.cpp \
        $$MUSCLE_DIR/reflector/StorageReflectSession.cpp \
        $$MUSCLE_DIR/reflector/DumbReflectSession.cpp \
        $$MUSCLE_DIR/reflector/DataNode.cpp \
        $$MUSCLE_DIR/reflector/ReflectServer.cpp \
        $$MUSCLE_DIR/reflector/FilterSessionFactory.cpp \
        $$MUSCLE_DIR/reflector/RateLimitSessionIOPolicy.cpp \
        $$MUSCLE_DIR/reflector/ServerComponent.cpp \
        $$MUSCLE_DIR/util/MemoryAllocator.cpp \
        $$MUSCLE_DIR/util/Directory.cpp \
        $$MUSCLE_DIR/util/FilePathInfo.cpp \
        $$MUSCLE_DIR/util/MiscUtilityFunctions.cpp \
        $$MUSCLE_DIR/util/ByteBuffer.cpp \
        $$MUSCLE_DIR/util/String.cpp \
        $$MUSCLE_DIR/util/StringTokenizer.cpp \
        $$MUSCLE_DIR/util/NetworkUtilityFunctions.cpp \
        $$MUSCLE_DIR/util/PulseNode.cpp \
        $$MUSCLE_DIR/util/SocketMultiplexer.cpp \
        $$MUSCLE_DIR/server/muscled.cpp \
        qt_muscled.cpp 

win32:SOURCES	+= $$MUSCLE_DIR/regex/regex/regcomp.c \
        $$MUSCLE_DIR/regex/regex/regerror.c \
        $$MUSCLE_DIR/regex/regex/regexec.c \
        $$MUSCLE_DIR/regex/regex/regfree.c \
        $$MUSCLE_DIR/zlib/zlib/adler32.c \
        $$MUSCLE_DIR/zlib/zlib/crc32.c \
        $$MUSCLE_DIR/zlib/zlib/deflate.c \
        $$MUSCLE_DIR/zlib/zlib/gzclose.c \
        $$MUSCLE_DIR/zlib/zlib/gzlib.c \
        $$MUSCLE_DIR/zlib/zlib/gzread.c \
        $$MUSCLE_DIR/zlib/zlib/gzwrite.c \
        $$MUSCLE_DIR/zlib/zlib/inffast.c \
        $$MUSCLE_DIR/zlib/zlib/inflate.c \
        $$MUSCLE_DIR/zlib/zlib/inftrees.c \
        $$MUSCLE_DIR/zlib/zlib/trees.c \
        $$MUSCLE_DIR/zlib/zlib/zutil.c

HEADERS += qt_muscled.h
