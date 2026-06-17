cmake_minimum_required(VERSION 3.16)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/include/nlohmann)

# RKNN & RGA headers (needed by engine/pipeline tests)
set(RKNN_API_PATH "${CMAKE_SOURCE_DIR}/../../runtime/RK3588/${CMAKE_SYSTEM_NAME}/librknn_api")
set(RGA_PATH "${CMAKE_SOURCE_DIR}/include/3rdparty/rga/RK3588")
include_directories(${RKNN_API_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/3rdparty)
include_directories(${RGA_PATH}/include)

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

# ---- P3: New tests ----
add_unit_test(test_preprocess unit/test_preprocess.cpp
    ${CMAKE_SOURCE_DIR}/src/preprocess.cpp)
target_link_libraries(test_preprocess ${OpenCV_LIBS}
    ${CMAKE_SOURCE_DIR}/include/3rdparty/rga/RK3588/lib/Linux/aarch64/librga.so)

add_unit_test_with_fixtures(test_config_loader unit/test_config_loader.cpp)

add_unit_test(test_core_num unit/test_core_num.cpp)
target_link_libraries(test_core_num Threads::Threads)

add_unit_test(test_iengine unit/test_iengine.cpp)
target_link_libraries(test_iengine ${OpenCV_LIBS})

# ---- P4: Hardware-optional tests ----
add_unit_test(test_rknn_pool unit/test_rknn_pool.cpp)
target_link_libraries(test_rknn_pool ${OpenCV_LIBS} Threads::Threads)

if(ENABLE_QT)
    add_unit_test(test_websocket unit/test_websocket.cpp)
    target_link_libraries(test_websocket Qt5::Core Qt5::WebSockets Qt5::Network)
endif()

add_unit_test(test_cascade unit/test_cascade.cpp
    ${CMAKE_SOURCE_DIR}/src/pipeline/CascadePipeline.cpp
    ${CMAKE_SOURCE_DIR}/src/rkYolov5s.cpp
    ${CMAKE_SOURCE_DIR}/src/engine/YOLO11Engine.cpp
    ${CMAKE_SOURCE_DIR}/src/postprocess.cpp
    ${CMAKE_SOURCE_DIR}/src/postprocess_yolo11.cpp
    ${CMAKE_SOURCE_DIR}/src/preprocess.cpp)
target_link_libraries(test_cascade ${OpenCV_LIBS} Threads::Threads
    ${CMAKE_SOURCE_DIR}/include/librknnrt.so
    ${CMAKE_SOURCE_DIR}/include/3rdparty/rga/RK3588/lib/Linux/aarch64/librga.so)
