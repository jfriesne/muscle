cmake_minimum_required(VERSION 2.8.12)

include(CTest)
# Project's name
project(muscle_tests)
if(NOT DEFINED CMAKE_CXX_STANDARD)
   set(CMAKE_CXX_STANDARD 11)
endif()

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
   add_executable(testatomicvalue testatomicvalue.cpp)
   target_link_libraries(testatomicvalue muscle)
   add_test(testatomicvalue testatomicvalue fromscript)

   add_executable(testbatchguard testbatchguard.cpp)
   target_link_libraries(testbatchguard muscle)
   add_test(testbatchguard testbatchguard fromscript)

   add_executable(testbitchord testbitchord.cpp)
   target_link_libraries(testbitchord muscle)
   add_test(testbitchord testbitchord fromscript)

   add_executable(testbytebuffer testbytebuffer.cpp)
   target_link_libraries(testbytebuffer muscle)
   add_test(testbytebuffer testbytebuffer fromscript)

   add_executable(testchildprocess testchildprocess.cpp)
   target_link_libraries(testchildprocess muscle)
   add_test(testchildprocess testchildprocess fromscript)

   add_executable(testclone testclone.cpp)
   target_link_libraries(testclone muscle)
   add_test(testclone testclone fromscript)

   add_executable(testendian testendian.cpp)
   target_link_libraries(testendian muscle)
   add_test(testendian testendian fromscript)

   add_executable(testfilepathinfo testfilepathinfo.cpp)
   target_link_libraries(testfilepathinfo muscle)
   add_test(testfilepathinfo testfilepathinfo fromscript)

   add_executable(testgateway testgateway.cpp)
   target_link_libraries(testgateway muscle)
   add_test(testgateway testgateway fromscript)

   add_executable(testhashcodes testhashcodes.cpp)
   target_link_libraries(testhashcodes muscle)
   add_test(testhashcodes testhashcodes fromscript)

   add_executable(testhashtable testhashtable.cpp)
   target_link_libraries(testhashtable muscle)
   add_test(testhashtable testhashtable fromscript)

   add_executable(testmatchfiles testmatchfiles.cpp)
   target_link_libraries(testmatchfiles muscle)
   add_test(testmatchfiles testmatchfiles fromscript)

   add_executable(testmessage testmessage.cpp)
   target_link_libraries(testmessage muscle)
   add_test(testmessage testmessage fromscript)

   add_executable(testmicro testmicro.cpp ../lang/c/micromessage/MicroMessage.c ../lang/c/micromessage/MicroMessageGateway.c)
   target_link_libraries(testmicro muscle)
   add_test(testmicro testmicro fromscript)

   add_executable(testmini testmini.cpp ../lang/c/minimessage/MiniMessage.c ../lang/c/minimessage/MiniMessageGateway.c)
   target_link_libraries(testmini muscle)
   add_test(testmini testmini fromscript)

   add_executable(testnagle testnagle.cpp)
   target_link_libraries(testnagle muscle)
   add_test(testnagle testnagle fromscript)

   add_executable(testnetconfigdetect testnetconfigdetect.cpp)
   target_link_libraries(testnetconfigdetect muscle)
   add_test(testnetconfigdetect testnetconfigdetect fromscript)

   add_executable(testnetutil testnetutil.cpp)
   target_link_libraries(testnetutil muscle)
   add_test(testnetutil testnetutil fromscript)

   add_executable(testobjectpool testobjectpool.cpp)
   target_link_libraries(testobjectpool muscle)
   add_test(testobjectpool testobjectpool fromscript)

   add_executable(testpacketio testpacketio.cpp)
   target_link_libraries(testpacketio muscle)
   add_test(testpacketio testpacketio fromscript)

   add_executable(testpackettunnel testpackettunnel.cpp)
   target_link_libraries(testpackettunnel muscle)
   add_test(testpackettunnel testpackettunnel fromscript)

   add_executable(testparsefile testparsefile.cpp)
   target_link_libraries(testparsefile muscle)
   add_test(testparsefile testparsefile fromscript)

   add_executable(testpool testpool.cpp)
   target_link_libraries(testpool muscle)
   add_test(testpool testpool fromscript)

   add_executable(testpulsenode testpulsenode.cpp)
   target_link_libraries(testpulsenode muscle)
   add_test(testpulsenode testpulsenode fromscript)

   add_executable(testqueryfilter testqueryfilter.cpp)
   target_link_libraries(testqueryfilter muscle)
   add_test(testqueryfilter testqueryfilter fromscript)

   add_executable(testqueue testqueue.cpp)
   target_link_libraries(testqueue muscle)
   add_test(testqueue testqueue fromscript)

   add_executable(testrefcount testrefcount.cpp)
   target_link_libraries(testrefcount muscle)
   add_test(testrefcount testrefcount fromscript)

   add_executable(testregex testregex.cpp)
   target_link_libraries(testregex muscle)
   add_test(testregex testregex fromscript)

   add_executable(testresponse testresponse.cpp)
   target_link_libraries(testresponse muscle)
   add_test(testresponse testresponse fromscript)

   add_executable(testserial testserial.cpp)
   target_link_libraries(testserial muscle)
   add_test(testserial testserial fromscript)

   add_executable(testserverthread testserverthread.cpp)
   target_link_libraries(testserverthread muscle)
   add_test(testserverthread testserverthread fromscript)

   add_executable(testsharedmem testsharedmem.cpp)
   target_link_libraries(testsharedmem muscle)
   add_test(testsharedmem testsharedmem fromscript)

   add_executable(testsocketmultiplexer testsocketmultiplexer.cpp)
   target_link_libraries(testsocketmultiplexer muscle)
   add_test(testsocketmultiplexer testsocketmultiplexer fromscript)

   add_executable(teststatus teststatus.cpp)
   target_link_libraries(teststatus muscle)
   add_test(teststatus teststatus fromscript)

   add_executable(teststring teststring.cpp)
   target_link_libraries(teststring muscle)
   add_test(teststring teststring fromscript)

   add_executable(teststringtokenizer teststringtokenizer.cpp)
   target_link_libraries(teststringtokenizer muscle)
   add_test(teststringtokenizer teststringtokenizer fromscript)

   add_executable(testsysteminfo testsysteminfo.cpp)
   target_link_libraries(testsysteminfo muscle)
   add_test(testsysteminfo testsysteminfo fromscript)

   add_executable(testtar testtar.cpp)
   target_link_libraries(testtar muscle)
   add_test(testtar testtar fromscript)

   add_executable(testthread testthread.cpp)
   target_link_libraries(testthread muscle)
   add_test(testthread testthread fromscript)

   add_executable(testthreadpool testthreadpool.cpp)
   target_link_libraries(testthreadpool muscle)
   add_test(testthreadpool testthreadpool fromscript)

   add_executable(testtime testtime.cpp)
   target_link_libraries(testtime muscle)
   add_test(testtime testtime fromscript)

   add_executable(testtimeunitconversions testtimeunitconversions.cpp)
   target_link_libraries(testtimeunitconversions muscle)
   add_test(testtimeunitconversions testtimeunitconversions fromscript)

   add_executable(testtuple testtuple.cpp)
   target_link_libraries(testtuple muscle)
   add_test(testtuple testtuple fromscript)

   add_executable(testtypedefs testtypedefs.cpp)
   target_link_libraries(testtypedefs muscle)
   add_test(testtypedefs testtypedefs fromscript)

   add_executable(testudp testudp.cpp)
   target_link_libraries(testudp muscle)
   add_test(testudp testudp fromscript)

   add_executable(testzip testzip.cpp)
   target_link_libraries(testzip muscle)
   add_test(testzip testzip fromscript)
endif ()
