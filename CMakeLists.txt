cmake_minimum_required(VERSION 3.20)
project(ArcademiaLeaderboards CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(arcademia_leaderboards SHARED
  src/arcademia_leaderboards.cpp
  src/sandbox_transport.cpp
  src/launcher_transport.cpp
)

target_include_directories(arcademia_leaderboards PUBLIC include)
target_link_libraries(arcademia_leaderboards PRIVATE winhttp)

set_target_properties(arcademia_leaderboards PROPERTIES
  OUTPUT_NAME "arcademia_leaderboards"
)

add_executable(quickstart samples/quickstart/main.cpp)
target_link_libraries(quickstart PRIVATE arcademia_leaderboards)
target_include_directories(quickstart PRIVATE include)
