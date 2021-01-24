greaterThan(QT_MAJOR_VERSION, 4) {
   QT += widgets
}
win32:LIBS     += shlwapi.lib ws2_32.lib winmm.lib User32.lib Advapi32.lib shell32.lib iphlpapi.lib version.lib
unix:!mac:LIBS += -lutil -lrt
mac:LIBS       += -framework IOKit -framework SystemConfiguration -framework Carbon
OBJECTS_DIR     = objects
MOC_DIR         = moc
MUSCLE_DIR      = ../..
win32:DEFINES  += _WIN32_WINNT=0x0501
INCLUDEPATH    += $$MUSCLE_DIR

# Uncomment the line below to test/demonstrate Qt Event loop integration.
# With integration enabled, InternalThreadSession will use Qt's
# event loop and a QTimer object to send back its once-per-second
# status reports, rather than its traditional WaitForNextMessage()
# based event loop.
#DEFINES        += MUSCLE_ENABLE_QTHREAD_EVENT_LOOP_INTEGRATION MUSCLE_PREFER_QT_OVER_WIN32

win32:INCLUDEPATH += $$MUSCLE_DIR/regex/regex 

MUSCLE_SOURCES = $$MUSCLE_DIR/dataio/FileDataIO.cpp                   \
                 $$MUSCLE_DIR/dataio/TCPSocketDataIO.cpp              \
                 $$MUSCLE_DIR/iogateway/AbstractMessageIOGateway.cpp  \
                 $$MUSCLE_DIR/iogateway/MessageIOGateway.cpp          \
                 $$MUSCLE_DIR/iogateway/RawDataMessageIOGateway.cpp   \
                 $$MUSCLE_DIR/message/Message.cpp                     \
                 $$MUSCLE_DIR/qtsupport/QMessageTransceiverThread.cpp \
                 $$MUSCLE_DIR/reflector/AbstractReflectSession.cpp    \
                 $$MUSCLE_DIR/reflector/DataNode.cpp                  \
                 $$MUSCLE_DIR/reflector/DumbReflectSession.cpp        \
                 $$MUSCLE_DIR/reflector/SignalHandlerSession.cpp      \
                 $$MUSCLE_DIR/reflector/StorageReflectSession.cpp     \
                 $$MUSCLE_DIR/reflector/FilterSessionFactory.cpp      \
                 $$MUSCLE_DIR/reflector/ReflectServer.cpp             \
                 $$MUSCLE_DIR/reflector/ServerComponent.cpp           \
                 $$MUSCLE_DIR/regex/SegmentedStringMatcher.cpp        \
                 $$MUSCLE_DIR/regex/StringMatcher.cpp                 \
                 $$MUSCLE_DIR/regex/PathMatcher.cpp                   \
                 $$MUSCLE_DIR/regex/QueryFilter.cpp                   \
                 $$MUSCLE_DIR/syslog/SysLog.cpp                       \
                 $$MUSCLE_DIR/system/MessageTransceiverThread.cpp     \
                 $$MUSCLE_DIR/system/SetupSystem.cpp                  \
                 $$MUSCLE_DIR/system/SignalMultiplexer.cpp            \
                 $$MUSCLE_DIR/system/Thread.cpp                       \
                 $$MUSCLE_DIR/util/ByteBuffer.cpp                     \
                 $$MUSCLE_DIR/util/Directory.cpp                      \
                 $$MUSCLE_DIR/util/FilePathInfo.cpp                   \
                 $$MUSCLE_DIR/util/MiscUtilityFunctions.cpp           \
                 $$MUSCLE_DIR/util/NetworkUtilityFunctions.cpp        \
                 $$MUSCLE_DIR/util/SocketMultiplexer.cpp              \
                 $$MUSCLE_DIR/util/String.cpp                         \
                 $$MUSCLE_DIR/util/StringTokenizer.cpp                \
                 $$MUSCLE_DIR/util/PulseNode.cpp

win32:MUSCLE_SOURCES += $$MUSCLE_DIR/regex/regex/regcomp.c            \
                        $$MUSCLE_DIR/regex/regex/regerror.c           \
                        $$MUSCLE_DIR/regex/regex/regexec.c            \
                        $$MUSCLE_DIR/regex/regex/regfree.c

MUSCLE_INCLUDES = $$MUSCLE_DIR/qtsupport/QMessageTransceiverThread.h

SOURCES = qt_advanced_example.cpp AdvancedQMessageTransceiverThread.cpp ThreadedInternalSession.cpp $$MUSCLE_SOURCES
HEADERS = qt_advanced_example.h $$MUSCLE_INCLUDES
