cmake_minimum_required(VERSION 3.7)

add_library(avm SHARED argon.cc areval.cc frame.cc ar_routine.cc routine_queue.cc runtime.cc context.cc import.cc)
add_dependencies(avm memory object)
target_link_libraries(avm memory object)