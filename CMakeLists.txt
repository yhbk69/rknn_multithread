cmake_minimum_required(VERSION 3.16)
project(yolo_detection_platform LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 平台检测 ----
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
    message(STATUS "Platform: Windows")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(PLATFORM_RK3588 TRUE)
    message(STATUS "Platform: RK3588")
else()
    set(PLATFORM_LINUX TRUE)
    message(STATUS "Platform: Linux (generic)")
endif()

# ---- Windows 编译选项 ----
if(PLATFORM_WINDOWS)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_definitions("$<$<CXX_COMPILER_ID:MSVC>:_HAS_STDEXT_ITERATOR=1>")
    add_compile_definitions("$<$<CXX_COMPILER_ID:MSVC>:_SILENCE_STDEXT_ITERATOR_DEPRECATION_WARNING>")
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_SOURCE_DIR}/include/ui")
endif()

# ---- Qt5 (双平台) ----
option(ENABLE_QT "Enable Qt GUI support" ON)

if(ENABLE_QT)
    if(PLATFORM_WINDOWS)
        # 自动检测或在命令行指定: -DQt5_DIR="C:/Qt/5.15.2/msvc2019_64/lib/cmake/Qt5"
        find_package(Qt5 REQUIRED COMPONENTS Widgets)
        message(STATUS "Qt5 found: ${Qt5Widgets_INCLUDE_DIRS}")
    elseif(PLATFORM_RK3588)
        find_package(Qt5 QUIET COMPONENTS Core Widgets Gui)
        if(NOT Qt5_FOUND)
            message(WARNING "Qt5 not found on RK3588, GUI disabled")
            set(ENABLE_QT OFF)
        endif()
    endif()
endif()

# ---- OpenCV (双平台) ----
if(PLATFORM_WINDOWS)
    set(OpenCV_DIR "C:/opencv412/opencv/build")
    find_package(OpenCV REQUIRED)
    message(STATUS "OpenCV found: ${OpenCV_INCLUDE_DIRS}")
elseif(PLATFORM_RK3588)
    find_package(OpenCV REQUIRED)
endif()

# ---- ONNX Runtime (Windows only) ----
if(PLATFORM_WINDOWS)
    set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/build_temp/onnxruntime/onnxruntime-win-x64-1.26.0"
        CACHE PATH "ONNX Runtime install directory")
    set(ONNXRUNTIME_INCLUDE_DIR "${ONNXRUNTIME_DIR}/include")
    set(ONNXRUNTIME_LIB_DIR "${ONNXRUNTIME_DIR}/lib")
    set(ONNXRUNTIME_BIN_DIR "${ONNXRUNTIME_DIR}/lib")
    message(STATUS "ONNX Runtime: ${ONNXRUNTIME_DIR}")
endif()

# ---- RKNN (RK3588 only) ----
if(PLATFORM_RK3588)
    set(RKNN_API_PATH "${CMAKE_SOURCE_DIR}/../../runtime/RK3588/${CMAKE_SYSTEM_NAME}/librknn_api")
    set(RKNN_RT_LIB "${CMAKE_SOURCE_DIR}/include/librknnrt.so")
    set(RGA_PATH "${CMAKE_SOURCE_DIR}/include/3rdparty/rga/RK3588")
    set(RGA_LIB "${RGA_PATH}/lib/Linux/aarch64/librga.so")
    include_directories(${RKNN_API_PATH}/include)
    include_directories(${CMAKE_SOURCE_DIR}/include/3rdparty)
    include_directories(${RGA_PATH}/include)
endif()

# ---- 头文件路径 ----
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/core)
include_directories(${CMAKE_SOURCE_DIR}/include/engine)
include_directories(${CMAKE_SOURCE_DIR}/include/ui)
include_directories(${CMAKE_SOURCE_DIR}/include/manager)
include_directories(${CMAKE_SOURCE_DIR}/include/io)
include_directories(${CMAKE_SOURCE_DIR}/include/pipeline)
include_directories(${CMAKE_SOURCE_DIR}/include/server)
if(PLATFORM_WINDOWS)
    include_directories(${ONNXRUNTIME_INCLUDE_DIR})
endif()

# ---- 源文件（按平台区分） ----
set(WIN_SOURCES
    src/win/main_win.cpp
    src/win/mainwindow.cpp
    src/onnx_engine.cpp
    src/inference_worker.cpp
    src/video_source.cpp
    src/gui_logger.cpp
    src/settings_dialog.cpp
    src/camera_manager.cpp
    src/model_manager.cpp
    src/inference_manager.cpp
    include/ui/mainwindow.hpp
    include/ui/mainwindow.ui
    include/ui/settings_dialog.hpp
    include/ui/detection_utils.hpp
    include/ui/gui_logger.hpp
    include/manager/inference_worker.hpp
    include/manager/camera_manager.hpp
    include/manager/model_manager.hpp
    include/manager/inference_manager.hpp
    include/io/video_source.hpp
    include/io/output_sink.hpp
    include/engine/inference_engine.hpp
    include/engine/onnx_engine.hpp
    include/core/types.hpp
    include/core/config.hpp
    include/core/runtime_config.hpp
    include/core/logger.hpp
)

set(RK3588_SOURCES
    src/main.cpp
    src/postprocess.cpp
    src/preprocess.cpp
    src/rkYolov5s.cpp
)

if(ENABLE_QT AND PLATFORM_RK3588)
    list(APPEND RK3588_SOURCES src/MainWindow.cpp)
endif()

# ---- 可执行文件 ----
if(PLATFORM_WINDOWS)
    add_executable(yolo_win ${WIN_SOURCES})
    target_link_libraries(yolo_win PRIVATE
        Qt5::Widgets
        ${OpenCV_LIBS}
        "${ONNXRUNTIME_LIB_DIR}/onnxruntime.lib"
    )
    # 复制 DLL 到输出目录
    add_custom_command(TARGET yolo_win POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_BIN_DIR}/onnxruntime.dll"
            "$<TARGET_FILE_DIR:yolo_win>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_BIN_DIR}/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:yolo_win>/"
        COMMENT "Copying ONNX Runtime DLLs..."
    )
    message(STATUS "Target: yolo_win.exe")

elseif(PLATFORM_RK3588)
    if(ENABLE_QT)
        add_executable(yolo_rk3588_qt ${RK3588_SOURCES})
        target_link_libraries(yolo_rk3588_qt
            ${RKNN_RT_LIB}
            ${OpenCV_LIBS}
            ${RGA_LIB}
            Qt5::Core Qt5::Widgets Qt5::Gui
        )
        message(STATUS "Target: yolo_rk3588_qt")
    else()
        add_executable(yolo_rk3588 ${RK3588_SOURCES})
        target_link_libraries(yolo_rk3588
            ${RKNN_RT_LIB}
            ${OpenCV_LIBS}
            ${RGA_LIB}
        )
        message(STATUS "Target: yolo_rk3588")
    endif()
endif()
