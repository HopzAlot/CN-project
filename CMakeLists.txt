cmake_minimum_required(VERSION 3.16)
project(p2p_share CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

# every .cpp under src/ is picked up automatically, so adding a file needs no CMake edit
file(GLOB_RECURSE CORE_SRC CONFIGURE_DEPENDS src/*.cpp)

# all modules as one library, so the peer and every test link the same code
add_library(p2p_core STATIC ${CORE_SRC})
target_include_directories(p2p_core PUBLIC src)
target_link_libraries(p2p_core PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(p2p_core PUBLIC ws2_32)
endif()

add_executable(peer app/main.cpp)
target_link_libraries(peer PRIVATE p2p_core)

# every tests/test_*.cpp becomes its own program, run them all with: ctest --test-dir build
enable_testing()
file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/test_*.cpp)
foreach(test_file ${TEST_SRC})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE p2p_core)
    add_test(NAME ${test_name} COMMAND ${test_name})
    list(APPEND TEST_TARGETS ${test_name})
endforeach()

foreach(t p2p_core peer ${TEST_TARGETS})
    if(NOT MSVC)
        target_compile_options(${t} PRIVATE -Wall -Wextra)
    endif()
endforeach()
