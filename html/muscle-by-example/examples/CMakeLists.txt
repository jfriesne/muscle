cmake_minimum_required(VERSION 2.8)

# Project's name
project(muscle_examples)
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

option(WITH_TESTS "Enable building of muscle examples" ON)
if (WITH_TESTS)
   add_executable(atomiccounter_example_1_basic_usage atomiccounter/example_1_basic_usage.cpp)
   target_link_libraries(atomiccounter_example_1_basic_usage muscle)

   add_executable(atomiccounter_example_2_stress_test atomiccounter/example_2_stress_test.cpp)
   target_link_libraries(atomiccounter_example_2_stress_test muscle)

   add_executable(bytebuffer_example_1_basic_usage bytebuffer/example_1_basic_usage.cpp)
   target_link_libraries(bytebuffer_example_1_basic_usage muscle)

   add_executable(bytebuffer_example_2_bb_pool bytebuffer/example_2_bb_pool.cpp)
   target_link_libraries(bytebuffer_example_2_bb_pool muscle)

   add_executable(bytebuffer_example_3_endian bytebuffer/example_3_endian.cpp)
   target_link_libraries(bytebuffer_example_3_endian muscle)

   add_executable(cpuloadmeter_example_1_basic_usage cpuloadmeter/example_1_basic_usage.cpp)
   target_link_libraries(cpuloadmeter_example_1_basic_usage muscle)

   add_executable(dataio_example_1_basic_usage dataio/example_1_basic_usage.cpp)
   target_link_libraries(dataio_example_1_basic_usage muscle)

   add_executable(dataio_example_2_tcp_server dataio/example_2_tcp_server.cpp)
   target_link_libraries(dataio_example_2_tcp_server muscle)

   add_executable(dataio_example_3_seekable_dataio dataio/example_3_seekable_dataio.cpp)
   target_link_libraries(dataio_example_3_seekable_dataio muscle)

   add_executable(dataio_example_4_idioms dataio/example_4_idioms.cpp)
   target_link_libraries(dataio_example_4_idioms muscle)

   add_executable(dataio_example_5_packet_dataio dataio/example_5_packet_dataio.cpp)
   target_link_libraries(dataio_example_5_packet_dataio muscle)

   add_executable(dataio_example_6_child_process dataio/example_6_child_process.cpp)
   target_link_libraries(dataio_example_6_child_process muscle)

   add_executable(detectnetworkconfigchangessession_example_1_basic_usage detectnetworkconfigchangessession/example_1_basic_usage.cpp)
   target_link_libraries(detectnetworkconfigchangessession_example_1_basic_usage muscle)

   add_executable(detectnetworkconfigchangessession_example_2_without_subclassing detectnetworkconfigchangessession/example_2_without_subclassing.cpp)
   target_link_libraries(detectnetworkconfigchangessession_example_2_without_subclassing muscle)

   add_executable(directory_example_1_basic_usage directory/example_1_basic_usage.cpp)
   target_link_libraries(directory_example_1_basic_usage muscle)

   add_executable(filepathinfo_example_1_basic_usage filepathinfo/example_1_basic_usage.cpp)
   target_link_libraries(filepathinfo_example_1_basic_usage muscle)

   add_executable(flattenable_example_1_basic_usage flattenable/example_1_basic_usage.cpp)
   target_link_libraries(flattenable_example_1_basic_usage muscle)

   add_executable(hashtable_example_1_basic_usage hashtable/example_1_basic_usage.cpp)
   target_link_libraries(hashtable_example_1_basic_usage muscle)

   add_executable(hashtable_example_2_sorting hashtable/example_2_sorting.cpp)
   target_link_libraries(hashtable_example_2_sorting muscle)

   add_executable(hashtable_example_3_iterating hashtable/example_3_iterating.cpp)
   target_link_libraries(hashtable_example_3_iterating muscle)

   add_executable(hashtable_example_4_idioms hashtable/example_4_idioms.cpp)
   target_link_libraries(hashtable_example_4_idioms muscle)

   add_executable(hashtable_example_5_ordered_tables hashtable/example_5_ordered_tables.cpp)
   target_link_libraries(hashtable_example_5_ordered_tables muscle)

   add_executable(hashtable_example_6_key_types hashtable/example_6_key_types.cpp)
   target_link_libraries(hashtable_example_6_key_types muscle)

   add_executable(iogateway_example_1_message_to_file iogateway/example_1_message_to_file.cpp)
   target_link_libraries(iogateway_example_1_message_to_file muscle)

   add_executable(iogateway_example_2_message_to_tcp iogateway/example_2_message_to_tcp.cpp)
   target_link_libraries(iogateway_example_2_message_to_tcp muscle)

   add_executable(iogateway_example_3_text_to_file iogateway/example_3_text_to_file.cpp)
   target_link_libraries(iogateway_example_3_text_to_file muscle)

   add_executable(iogateway_example_4_text_to_tcp iogateway/example_4_text_to_tcp.cpp)
   target_link_libraries(iogateway_example_4_text_to_tcp muscle)

   add_executable(ipaddress_example_1_basic_usage ipaddress/example_1_basic_usage.cpp)
   target_link_libraries(ipaddress_example_1_basic_usage muscle)

   add_executable(ipaddress_example_2_interactive_ipaddress ipaddress/example_2_interactive_ipaddress.cpp)
   target_link_libraries(ipaddress_example_2_interactive_ipaddress muscle)

   add_executable(ipaddress_example_3_interactive_ipaddress_and_port ipaddress/example_3_interactive_ipaddress_and_port.cpp)
   target_link_libraries(ipaddress_example_3_interactive_ipaddress_and_port muscle)

   add_executable(logtime_example_1_basic_usage logtime/example_1_basic_usage.cpp)
   target_link_libraries(logtime_example_1_basic_usage muscle)

   add_executable(logtime_example_2_log_files logtime/example_2_log_files.cpp)
   target_link_libraries(logtime_example_2_log_files muscle)

   add_executable(logtime_example_3_advanced_log_files logtime/example_3_advanced_log_files.cpp)
   target_link_libraries(logtime_example_3_advanced_log_files muscle)

   add_executable(logtime_example_4_log_callbacks logtime/example_4_log_callbacks.cpp)
   target_link_libraries(logtime_example_4_log_callbacks muscle)

   add_executable(message_example_1_basic_usage message/example_1_basic_usage.cpp)
   target_link_libraries(message_example_1_basic_usage muscle)

   add_executable(message_example_2_nested_messages message/example_2_nested_messages.cpp)
   target_link_libraries(message_example_2_nested_messages muscle)

   add_executable(message_example_3_add_archive message/example_3_add_archive.cpp)
   target_link_libraries(message_example_3_add_archive muscle)

   add_executable(message_example_4_add_flat message/example_4_add_flat.cpp)
   target_link_libraries(message_example_4_add_flat muscle)

   add_executable(message_example_5_field_iteration message/example_5_field_iteration.cpp)
   target_link_libraries(message_example_5_field_iteration muscle)

   add_executable(messagetransceiverthread_example_1_threaded_smart_client messagetransceiverthread/example_1_threaded_smart_client.cpp)
   target_link_libraries(messagetransceiverthread_example_1_threaded_smart_client muscle)

   add_executable(miscutilityfunctions_example_1_command_line_args miscutilityfunctions/example_1_command_line_args.cpp)
   target_link_libraries(miscutilityfunctions_example_1_command_line_args muscle)

   add_executable(miscutilityfunctions_example_2_config_file miscutilityfunctions/example_2_config_file.cpp)
   target_link_libraries(miscutilityfunctions_example_2_config_file muscle)

   add_executable(miscutilityfunctions_example_3_hex_bytes miscutilityfunctions/example_3_hex_bytes.cpp)
   target_link_libraries(miscutilityfunctions_example_3_hex_bytes muscle)

   add_executable(mutex_example_1_basic_usage mutex/example_1_basic_usage.cpp)
   target_link_libraries(mutex_example_1_basic_usage muscle)

   add_executable(mutex_example_2_mutex_guard mutex/example_2_mutex_guard.cpp)
   target_link_libraries(mutex_example_2_mutex_guard muscle)

   add_executable(nestcount_example_1_basic_usage nestcount/example_1_basic_usage.cpp)
   target_link_libraries(nestcount_example_1_basic_usage muscle)

   add_executable(nestcount_example_2_recursion_batch nestcount/example_2_recursion_batch.cpp)
   target_link_libraries(nestcount_example_2_recursion_batch muscle)

   add_executable(nestcount_example_3_without_guard nestcount/example_3_without_guard.cpp)
   target_link_libraries(nestcount_example_3_without_guard muscle)

   add_executable(networkinterfaceinfo_example_1_basic_usage networkinterfaceinfo/example_1_basic_usage.cpp)
   target_link_libraries(networkinterfaceinfo_example_1_basic_usage muscle)

   add_executable(networkutilityfunctions_example_1_tcp_client networkutilityfunctions/example_1_tcp_client.cpp)
   target_link_libraries(networkutilityfunctions_example_1_tcp_client muscle)

   add_executable(networkutilityfunctions_example_2_udp_pingpong networkutilityfunctions/example_2_udp_pingpong.cpp)
   target_link_libraries(networkutilityfunctions_example_2_udp_pingpong muscle)

   add_executable(networkutilityfunctions_example_3_udp_multicast networkutilityfunctions/example_3_udp_multicast.cpp)
   target_link_libraries(networkutilityfunctions_example_3_udp_multicast muscle)

   add_executable(queryfilter_example_1_basic_usage queryfilter/example_1_basic_usage.cpp)
   target_link_libraries(queryfilter_example_1_basic_usage muscle)

   add_executable(queryfilter_example_2_smart_client_with_queryfilter queryfilter/example_2_smart_client_with_queryfilter.cpp)
   target_link_libraries(queryfilter_example_2_smart_client_with_queryfilter muscle)

   add_executable(queue_example_1_basic_usage queue/example_1_basic_usage.cpp)
   target_link_libraries(queue_example_1_basic_usage muscle)

   add_executable(queue_example_2_sorting queue/example_2_sorting.cpp)
   target_link_libraries(queue_example_2_sorting muscle)

   add_executable(queue_example_3_idioms queue/example_3_idioms.cpp)
   target_link_libraries(queue_example_3_idioms muscle)

   add_executable(refcount_example_1_basic_usage refcount/example_1_basic_usage.cpp)
   target_link_libraries(refcount_example_1_basic_usage muscle)

   add_executable(refcount_example_2_with_object_pool refcount/example_2_with_object_pool.cpp)
   target_link_libraries(refcount_example_2_with_object_pool muscle)

   add_executable(refcount_example_3_conversions refcount/example_3_conversions.cpp)
   target_link_libraries(refcount_example_3_conversions muscle)

   add_executable(refcount_example_4_dummy_refs refcount/example_4_dummy_refs.cpp)
   target_link_libraries(refcount_example_4_dummy_refs muscle)

   add_executable(reflector_example_1_dumb_server reflector/example_1_dumb_server.cpp)
   target_link_libraries(reflector_example_1_dumb_server muscle)

   add_executable(reflector_example_2_dumb_client reflector/example_2_dumb_client.cpp)
   target_link_libraries(reflector_example_2_dumb_client muscle)

   add_executable(reflector_example_3_annotated_dumb_server reflector/example_3_annotated_dumb_server.cpp)
   target_link_libraries(reflector_example_3_annotated_dumb_server muscle)

   add_executable(reflector_example_4_smart_server reflector/example_4_smart_server.cpp)
   target_link_libraries(reflector_example_4_smart_server muscle)

   add_executable(reflector_example_5_smart_client reflector/example_5_smart_client.cpp)
   target_link_libraries(reflector_example_5_smart_client muscle)

   add_executable(reflector_example_6_smart_server_with_pulsenode reflector/example_6_smart_server_with_pulsenode.cpp)
   target_link_libraries(reflector_example_6_smart_server_with_pulsenode muscle)

   add_executable(reflector_example_7_smart_server_with_udp_pingpong reflector/example_7_smart_server_with_udp_pingpong.cpp)
   target_link_libraries(reflector_example_7_smart_server_with_udp_pingpong muscle)

   add_executable(sharedmemory_example_1_basic_usage sharedmemory/example_1_basic_usage.cpp)
   target_link_libraries(sharedmemory_example_1_basic_usage muscle)

   add_executable(signalhandlersession_example_1_basic_usage signalhandlersession/example_1_basic_usage.cpp)
   target_link_libraries(signalhandlersession_example_1_basic_usage muscle)

   add_executable(signalhandlersession_example_2_custom_handling signalhandlersession/example_2_custom_handling.cpp)
   target_link_libraries(signalhandlersession_example_2_custom_handling muscle)

   add_executable(socket_example_1_basic_usage socket/example_1_basic_usage.cpp)
   target_link_libraries(socket_example_1_basic_usage muscle)

   add_executable(socketmultiplexer_example_1_tcp_echo_server socketmultiplexer/example_1_tcp_echo_server.cpp)
   target_link_libraries(socketmultiplexer_example_1_tcp_echo_server muscle)

   add_executable(socketmultiplexer_example_2_tcp_echo_server_with_timed_counter socketmultiplexer/example_2_tcp_echo_server_with_timed_counter.cpp)
   target_link_libraries(socketmultiplexer_example_2_tcp_echo_server_with_timed_counter muscle)

   add_executable(string_example_1_basic_usage string/example_1_basic_usage.cpp)
   target_link_libraries(string_example_1_basic_usage muscle)

   add_executable(string_example_2_substrings string/example_2_substrings.cpp)
   target_link_libraries(string_example_2_substrings muscle)

   add_executable(string_example_3_interactive string/example_3_interactive.cpp)
   target_link_libraries(string_example_3_interactive muscle)

   add_executable(string_example_4_interpolation string/example_4_interpolation.cpp)
   target_link_libraries(string_example_4_interpolation muscle)

   add_executable(stringmatcher_example_1_glob_matching stringmatcher/example_1_glob_matching.cpp)
   target_link_libraries(stringmatcher_example_1_glob_matching muscle)

   add_executable(stringmatcher_example_2_regex_matching stringmatcher/example_2_regex_matching.cpp)
   target_link_libraries(stringmatcher_example_2_regex_matching muscle)

   add_executable(stringtokenizer_example_1_basic_usage stringtokenizer/example_1_basic_usage.cpp)
   target_link_libraries(stringtokenizer_example_1_basic_usage muscle)

   add_executable(systeminfo_example_1_basic_usage systeminfo/example_1_basic_usage.cpp)
   target_link_libraries(systeminfo_example_1_basic_usage muscle)

   add_executable(tarfilewriter_example_1_basic_usage tarfilewriter/example_1_basic_usage.cpp)
   target_link_libraries(tarfilewriter_example_1_basic_usage muscle)

   add_executable(thread_example_1_basic_usage thread/example_1_basic_usage.cpp)
   target_link_libraries(thread_example_1_basic_usage muscle)

   add_executable(thread_example_2_dumb_server_with_thread thread/example_2_dumb_server_with_thread.cpp)
   target_link_libraries(thread_example_2_dumb_server_with_thread muscle)

   add_executable(threadlocalstorage_example_1_basic_usage threadlocalstorage/example_1_basic_usage.cpp)
   target_link_libraries(threadlocalstorage_example_1_basic_usage muscle)

   add_executable(timeutilityfunctions_example_1_monotonic_clock timeutilityfunctions/example_1_monotonic_clock.cpp)
   target_link_libraries(timeutilityfunctions_example_1_monotonic_clock muscle)

   add_executable(timeutilityfunctions_example_2_calendar_time timeutilityfunctions/example_2_calendar_time.cpp)
   target_link_libraries(timeutilityfunctions_example_2_calendar_time muscle)

   add_executable(timeutilityfunctions_example_3_idioms timeutilityfunctions/example_3_idioms.cpp)
   target_link_libraries(timeutilityfunctions_example_3_idioms muscle)

   add_executable(zipfileutilityfunctions_example_1_read_zip zipfileutilityfunctions/example_1_read_zip.cpp)
   target_link_libraries(zipfileutilityfunctions_example_1_read_zip muscle)

   add_executable(zipfileutilityfunctions_example_2_write_zip zipfileutilityfunctions/example_2_write_zip.cpp)
   target_link_libraries(zipfileutilityfunctions_example_2_write_zip muscle)

   add_executable(zlibcodec_example_1_basic_usage zlibcodec/example_1_basic_usage.cpp)
   target_link_libraries(zlibcodec_example_1_basic_usage muscle)

   add_executable(zlibcodec_example_2_dependent_series zlibcodec/example_2_dependent_series.cpp)
   target_link_libraries(zlibcodec_example_2_dependent_series muscle)

   add_executable(zlibutilityfunctions_example_1_byte_buffers zlibutilityfunctions/example_1_byte_buffers.cpp)
   target_link_libraries(zlibutilityfunctions_example_1_byte_buffers muscle)

   add_executable(zlibutilityfunctions_example_2_messages zlibutilityfunctions/example_2_messages.cpp)
   target_link_libraries(zlibutilityfunctions_example_2_messages muscle)
endif ()
