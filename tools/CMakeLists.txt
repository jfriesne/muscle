cmake_minimum_required(VERSION 2.8.12)

# Project's name
project(muscle_tools)
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

option(WITH_TOOLS "Enable building of muscle tools" ON)
if (WITH_TOOLS)
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

   add_executable(rwdeadlock rwdeadlock.cpp)
   target_link_libraries(rwdeadlock muscle)

   add_executable(erasesharedmem erasesharedmem.cpp)
   target_link_libraries(erasesharedmem muscle)

   add_executable(findsourcelocations findsourcelocations.cpp)
   target_link_libraries(findsourcelocations muscle)

   add_executable(hexterm hexterm.cpp)
   target_link_libraries(hexterm muscle)

   add_executable(muscleproxy muscleproxy.cpp)
   target_link_libraries(muscleproxy muscle)

   add_executable(multithreadedreflectclient multithreadedreflectclient.cpp)
   target_link_libraries(multithreadedreflectclient muscle)

   add_executable(portableplaintextclient portableplaintextclient.cpp)
   target_link_libraries(portableplaintextclient muscle)

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

   add_executable(singlethreadedreflectclient singlethreadedreflectclient.cpp)
   target_link_libraries(singlethreadedreflectclient muscle)

   add_executable(snoopsharedmem snoopsharedmem.cpp)
   target_link_libraries(snoopsharedmem muscle)

   add_executable(striphextermoutput striphextermoutput.cpp)
   target_link_libraries(striphextermoutput muscle)

   add_executable(udpproxy udpproxy.cpp)
   target_link_libraries(udpproxy muscle)

   add_executable(uploadstress uploadstress.cpp)
   target_link_libraries(uploadstress muscle)
endif ()
