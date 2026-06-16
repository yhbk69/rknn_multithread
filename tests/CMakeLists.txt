cmake_minimum_required(VERSION 3.16)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

add_library(catch_main OBJECT catch_amalgamated.cpp)

function(add_unit_test name source)
    add_executable(${name} ${source} $<TARGET_OBJECTS:catch_main>)
    target_link_libraries(${name} ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_unit_test(test_stats_collector unit/test_stats_collector.cpp)

add_unit_test(test_frame_reader unit/test_frame_reader.cpp ${OpenCV_LIBS})

if(ENABLE_QT)
    add_unit_test(test_result_renderer unit/test_result_renderer.cpp ${OpenCV_LIBS} Qt5::Gui)
endif()
