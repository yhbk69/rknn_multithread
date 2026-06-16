cmake_minimum_required(VERSION 3.16)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include/nlohmann)

add_library(catch_main OBJECT catch_amalgamated.cpp)

function(add_unit_test name)
    add_executable(${name} ${ARGN} $<TARGET_OBJECTS:catch_main>)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

function(add_unit_test_with_fixtures name)
    add_executable(${name} ${ARGN} $<TARGET_OBJECTS:catch_main>)
    add_test(NAME ${name} COMMAND ${name}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# ---- P1: Pipeline component tests ----
add_unit_test(test_stats_collector unit/test_stats_collector.cpp)
add_unit_test(test_frame_reader unit/test_frame_reader.cpp)
target_link_libraries(test_frame_reader ${OpenCV_LIBS})

if(ENABLE_QT)
    add_unit_test(test_result_renderer unit/test_result_renderer.cpp)
    target_link_libraries(test_result_renderer ${OpenCV_LIBS} Qt5::Gui)
endif()

# ---- P2: Core logic tests ----
add_unit_test_with_fixtures(test_postprocess
    unit/test_postprocess.cpp
    ${CMAKE_SOURCE_DIR}/src/postprocess.cpp)
