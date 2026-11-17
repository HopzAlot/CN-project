cmake_minimum_required(VERSION 3.16)
project(p2p_share CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# every .cpp under src/ is picked up automatically, so adding a file needs no CMake edit
file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS src/*.cpp)

add_executable(peer app/main.cpp ${CORE_SRC})
target_include_directories(peer PRIVATE src)

find_package(Threads REQUIRED)
target_link_libraries(peer PRIVATE Threads::Threads)

# Windows sockets
if(WIN32)
    target_link_libraries(peer PRIVATE ws2_32)
endif()

if(NOT MSVC)
    target_compile_options(peer PRIVATE -Wall -Wextra)
endif()
