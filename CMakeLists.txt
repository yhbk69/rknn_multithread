cmake_minimum_required(VERSION 3.4.1)

project(rknn_yolov5_demo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread")

# skip 3rd-party lib dependencies
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# install target and libraries
set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn_yolov5_demo_${CMAKE_SYSTEM_NAME})

set(CMAKE_SKIP_INSTALL_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# rknn api
set(RKNN_API_PATH ${CMAKE_SOURCE_DIR}/../../runtime/RK3588/${CMAKE_SYSTEM_NAME}/librknn_api)
set(LIB_ARCH aarch64)
set(RKNN_RT_LIB ${CMAKE_SOURCE_DIR}/include/librknnrt.so)

#rga
set(RGA_PATH ${CMAKE_SOURCE_DIR}/include/3rdparty/rga/RK3588)
set(RGA_LIB ${RGA_PATH}/lib/Linux//${LIB_ARCH}/librga.so)


include_directories(${RKNN_API_PATH}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/3rdparty)
include_directories(${RGA_PATH}/include)

# opencv
find_package(OpenCV REQUIRED)

set(CMAKE_INSTALL_RPATH "lib")

# Qt5 (optional)
option(ENABLE_QT "Enable Qt GUI support" OFF)
if(ENABLE_QT)
  find_package(Qt5 REQUIRED COMPONENTS Core Widgets Gui)
  set(CMAKE_AUTOMOC ON)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn_yolov5_qt_demo_${CMAKE_SYSTEM_NAME})
endif()


# rknn_yolov5_demo
include_directories( ${CMAKE_SOURCE_DIR}/include)

if(ENABLE_QT)
  add_executable(rknn_yolov5_qt_demo
          src/qt_main.cpp
          src/MainWindow.cpp
          src/postprocess.cpp
          src/preprocess.cpp
          src/rkYolov5s.cpp
  )
  target_link_libraries(rknn_yolov5_qt_demo
    ${RKNN_RT_LIB}
    ${OpenCV_LIBS}
    ${RGA_LIB}
    Qt5::Core
    Qt5::Widgets
    Qt5::Gui
  )
else()
  add_executable(rknn_yolov5_demo
          src/main.cpp
          src/postprocess.cpp
          src/preprocess.cpp
          src/rkYolov5s.cpp
  )
  target_link_libraries(rknn_yolov5_demo
    ${RKNN_RT_LIB}
    ${OpenCV_LIBS}
    ${RGA_LIB}
  )
endif()


# install target and libraries
if(ENABLE_QT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn_yolov5_qt_demo_${CMAKE_SYSTEM_NAME})
  install(TARGETS rknn_yolov5_qt_demo DESTINATION ./)
else()
  set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/rknn_yolov5_demo_${CMAKE_SYSTEM_NAME})
  install(TARGETS rknn_yolov5_demo DESTINATION ./)
endif()
install(PROGRAMS ${RKNN_RT_LIB} DESTINATION lib)
install(PROGRAMS ${RGA_LIB} DESTINATION lib)
install(DIRECTORY model DESTINATION ./)
