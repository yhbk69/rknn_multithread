cmake_minimum_required(VERSION 3.16)
project(yolo_detection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- RK3588 platform ----
message(STATUS "Platform: RK3588 (Linux aarch64)")

# ---- Qt5 ----
option(ENABLE_QT "Enable Qt GUI support" ON)

if(ENABLE_QT)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    
    find_package(Qt5 QUIET COMPONENTS Core Widgets Gui WebSockets)
    if(NOT Qt5_FOUND)
        message(WARNING "Qt5 not found, GUI disabled")
        set(ENABLE_QT OFF)
    endif()
endif()

# ---- OpenCV ----
find_package(OpenCV REQUIRED)

# ---- RKNN ----
set(RKNN_API_PATH "${CMAKE_SOURCE_DIR}/../../runtime/RK3588/${CMAKE_SYSTEM_NAME}/librknn_api")
set(RKNN_RT_LIB "${CMAKE_SOURCE_DIR}/include/librknnrt.so")
set(RGA_PATH "${CMAKE_SOURCE_DIR}/include/3rdparty/rga/RK3588")
set(RGA_LIB "${RGA_PATH}/lib/Linux/aarch64/librga.so")
include_directories(${RKNN_API_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/3rdparty)
include_directories(${RGA_PATH}/include)

# ---- Threads ----
find_package(Threads REQUIRED)

# ---- Header paths ----
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# ---- Source files ----
set(COMMON_SOURCES
    src/postprocess.cpp
    src/preprocess.cpp
    src/rkYolov5s.cpp
)

set(QT_SOURCES
    ${COMMON_SOURCES}
    src/websocket.cpp
    include/websocket.hpp
    src/rk/main_rk.cpp
    src/rk/mainwindow.cpp
    src/rk/mainwindow_helpers.cpp
    src/rk/mainwindow_slots.cpp
    src/rk/mainwindow.hpp
)

set(CLI_SOURCES
    ${COMMON_SOURCES}
    src/main.cpp
)

# ---- Executables ----
if(ENABLE_QT)
    add_executable(yolo_rk3588_qt ${QT_SOURCES})
    target_link_libraries(yolo_rk3588_qt
        ${RKNN_RT_LIB}
        ${OpenCV_LIBS}
        ${RGA_LIB}
        Qt5::Core Qt5::Widgets Qt5::Gui Qt5::WebSockets Qt5::Network
        Threads::Threads
    )
    message(STATUS "Target: yolo_rk3588_qt")

    install(TARGETS yolo_rk3588_qt DESTINATION rknn_yolov5_qt_demo_Linux)
    install(FILES ${CMAKE_SOURCE_DIR}/config.json DESTINATION rknn_yolov5_qt_demo_Linux)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/model DESTINATION rknn_yolov5_qt_demo_Linux)
    install(FILES ${RKNN_RT_LIB} ${RGA_LIB} DESTINATION rknn_yolov5_qt_demo_Linux/lib)
endif()

add_executable(yolo_rk3588 ${CLI_SOURCES})
target_link_libraries(yolo_rk3588
    ${RKNN_RT_LIB}
    ${OpenCV_LIBS}
    ${RGA_LIB}
    Threads::Threads
)
message(STATUS "Target: yolo_rk3588")

install(TARGETS yolo_rk3588 DESTINATION rknn_yolov5_demo_Linux)
install(FILES ${CMAKE_SOURCE_DIR}/config.json DESTINATION rknn_yolov5_demo_Linux)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/model DESTINATION rknn_yolov5_demo_Linux)

# ---- Tests ----
option(ENABLE_TESTS "Build unit tests" OFF)
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
install(FILES ${RKNN_RT_LIB} ${RGA_LIB} DESTINATION rknn_yolov5_demo_Linux/lib)
