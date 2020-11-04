if(CPPCORO_NO_GOLD_LINKER)
  return()
endif()

find_program(GOLD_EXE NAMES ld.gold gold)

if(GOLD_EXE)
  message(STATUS "Using gold linker (${GOLD_EXE})")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_CXX_LINKER_FLAGS} -fuse-ld=gold")
else()
  message(STATUS "gold linker not found")
endif()