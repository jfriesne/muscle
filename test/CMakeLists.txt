cmake_minimum_required(VERSION 2.8)

# Project's name
project(muscle_tests)
set(CMAKE_CXX_STANDARD 11)

#if (WIN32)
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2 -DMUSCLE_ENABLE_ZLIB_ENCODING -DMUSCLE_CATCH_SIGNALS_BY_DEFAULT -DMUSCLE_NO_EXCEPTIONS /W3 /D_CRT_SECURE_NO_WARNINGS")
#else (!WIN32)
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DMUSCLE_ENABLE_ZLIB_ENCODING -DMUSCLE_CATCH_SIGNALS_BY_DEFAULT -DMUSCLE_NO_EXCEPTIONS -fno-exceptions -Wall")
#endif (WIN32)
#
#if (WIN32)
#   # cool
#else (!WIN32)
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DMUSCLE_USE_PTHREADS -Wno-multichar")
#   if (APPLE)
#      # cool
#   else (!APPLE)
#      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
#   endif (APPLE)
#endif (WIN32)

target_link_libraries(muscle)

option(WITH_TESTS "Enable building of muscle tests" ON)
if (WITH_TESTS)
   add_executable(bandwidthtester bandwidthtester.cpp)
   target_link_libraries(bandwidthtester muscle)

   add_executable(calctypecode calctypecode.cpp)
   target_link_libraries(calctypecode muscle)

   add_executable(chatclient chatclient.cpp)
   target_link_libraries(chatclient muscle)

   add_executable(daemonsitter daemonsitter.cpp)
   target_link_libraries(daemonsitter muscle)

   add_executable(deadlock deadlock.cpp)
   target_link_libraries(deadlock muscle)

   add_executable(deadlockfinder deadlockfinder.cpp)
   target_link_libraries(deadlockfinder muscle)

   add_executable(findsourcelocations findsourcelocations.cpp)
   target_link_libraries(findsourcelocations muscle)

   add_executable(hexterm hexterm.cpp)
   target_link_libraries(hexterm muscle)

   add_executable(muscleproxy muscleproxy.cpp)
   target_link_libraries(muscleproxy muscle)

   add_executable(portableplaintextclient portableplaintextclient.cpp)
   target_link_libraries(portableplaintextclient muscle)

   add_executable(portablereflectclient portablereflectclient.cpp)
   target_link_libraries(portablereflectclient muscle)

   add_executable(portscan portscan.cpp)
   target_link_libraries(portscan muscle)

   add_executable(printsourcelocations printsourcelocations.cpp)
   target_link_libraries(printsourcelocations muscle)

   add_executable(printtypecode printtypecode.cpp)
   target_link_libraries(printtypecode muscle)

   add_executable(readmessage readmessage.cpp)
   target_link_libraries(readmessage muscle)

   add_executable(serialproxy serialproxy.cpp)
   target_link_libraries(serialproxy muscle)

   add_executable(striphextermoutput striphextermoutput.cpp)
   target_link_libraries(striphextermoutput muscle)

   add_executable(testbatchguard testbatchguard.cpp)
   target_link_libraries(testbatchguard muscle)

   add_executable(testbitchord testbitchord.cpp)
   target_link_libraries(testbitchord muscle)

   add_executable(testbytebuffer testbytebuffer.cpp)
   target_link_libraries(testbytebuffer muscle)

   add_executable(testchildprocess testchildprocess.cpp)
   target_link_libraries(testchildprocess muscle)

   add_executable(testclone testclone.cpp)
   target_link_libraries(testclone muscle)

   add_executable(testendian testendian.cpp)
   target_link_libraries(testendian muscle)

   add_executable(testfilepathinfo testfilepathinfo.cpp)
   target_link_libraries(testfilepathinfo muscle)

   add_executable(testgateway testgateway.cpp)
   target_link_libraries(testgateway muscle)

   add_executable(testhashcodes testhashcodes.cpp)
   target_link_libraries(testhashcodes muscle)

   add_executable(testhashtable testhashtable.cpp)
   target_link_libraries(testhashtable muscle)

   add_executable(testmatchfiles testmatchfiles.cpp)
   target_link_libraries(testmatchfiles muscle)

   add_executable(testmessage testmessage.cpp)
   target_link_libraries(testmessage muscle)

   add_executable(testnagle testnagle.cpp)
   target_link_libraries(testnagle muscle)

   add_executable(testnetconfigdetect testnetconfigdetect.cpp)
   target_link_libraries(testnetconfigdetect muscle)

   add_executable(testnetutil testnetutil.cpp)
   target_link_libraries(testnetutil muscle)

   add_executable(testobjectpool testobjectpool.cpp)
   target_link_libraries(testobjectpool muscle)

   add_executable(testpacketio testpacketio.cpp)
   target_link_libraries(testpacketio muscle)

   add_executable(testpackettunnel testpackettunnel.cpp)
   target_link_libraries(testpackettunnel muscle)

   add_executable(testparsefile testparsefile.cpp)
   target_link_libraries(testparsefile muscle)

   add_executable(testpool testpool.cpp)
   target_link_libraries(testpool muscle)

   add_executable(testpulsenode testpulsenode.cpp)
   target_link_libraries(testpulsenode muscle)

   add_executable(testqueryfilter testqueryfilter.cpp)
   target_link_libraries(testqueryfilter muscle)

   add_executable(testqueue testqueue.cpp)
   target_link_libraries(testqueue muscle)

   add_executable(testrefcount testrefcount.cpp)
   target_link_libraries(testrefcount muscle)

   add_executable(testregex testregex.cpp)
   target_link_libraries(testregex muscle)

   add_executable(testresponse testresponse.cpp)
   target_link_libraries(testresponse muscle)

   add_executable(testserial testserial.cpp)
   target_link_libraries(testserial muscle)

   add_executable(testserverthread testserverthread.cpp)
   target_link_libraries(testserverthread muscle)

   add_executable(testsharedmem testsharedmem.cpp)
   target_link_libraries(testsharedmem muscle)

   add_executable(testsocketmultiplexer testsocketmultiplexer.cpp)
   target_link_libraries(testsocketmultiplexer muscle)

   add_executable(teststatus teststatus.cpp)
   target_link_libraries(teststatus muscle)

   add_executable(teststring teststring.cpp)
   target_link_libraries(teststring muscle)

   add_executable(teststringtokenizer teststringtokenizer.cpp)
   target_link_libraries(teststringtokenizer muscle)

   add_executable(testsysteminfo testsysteminfo.cpp)
   target_link_libraries(testsysteminfo muscle)

   add_executable(testthread testthread.cpp)
   target_link_libraries(testthread muscle)

   add_executable(testthreadpool testthreadpool.cpp)
   target_link_libraries(testthreadpool muscle)

   add_executable(testtime testtime.cpp)
   target_link_libraries(testtime muscle)

   add_executable(testtimeunitconversions testtimeunitconversions.cpp)
   target_link_libraries(testtimeunitconversions muscle)

   add_executable(testtuple testtuple.cpp)
   target_link_libraries(testtuple muscle)

   add_executable(testtypedefs testtypedefs.cpp)
   target_link_libraries(testtypedefs muscle)

   add_executable(testudp testudp.cpp)
   target_link_libraries(testudp muscle)

   add_executable(testzip testzip.cpp)
   target_link_libraries(testzip muscle)

   add_executable(udpproxy udpproxy.cpp)
   target_link_libraries(udpproxy muscle)

   add_executable(uploadstress uploadstress.cpp)
   target_link_libraries(uploadstress muscle)

   if (WIN32)
      add_executable(win32client win32client.cpp)
      target_link_libraries(win32client muscle)
   endif (WIN32)
endif ()
