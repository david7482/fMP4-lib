cmake_minimum_required(VERSION 2.6)
project(fMP4)

set( CMAKE_BUILD_TYPE Debug )

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(LIBAVCODEC REQUIRED libavcodec)
if(LIBAVCODEC_FOUND)
    include_directories(${LIBAVCODEC_INCLUDE_DIRS})
    link_directories(${LIBAVCODEC_LIBRARY_DIRS})
endif()

pkg_check_modules(LIBAVUTIL REQUIRED libavutil)
if(LIBAVUTIL_FOUND)
    include_directories(${LIBAVUTIL_INCLUDE_DIRS})
    link_directories(${LIBAVUTIL_LIBRARY_DIRS})
endif()

pkg_check_modules(LIBAVFORMAT REQUIRED libavformat)
if(LIBAVFORMAT_FOUND)
    include_directories(${LIBAVFORMAT_INCLUDE_DIRS})
    link_directories(${LIBAVFORMAT_LIBRARY_DIRS})
endif()

pkg_check_modules(LIBSWSCALE REQUIRED libswscale)
if(LIBSWSCALE_FOUND)
    include_directories(${LIBSWSCALE_INCLUDE_DIRS})
    link_directories(${LIBSWSCALE_LIBRARY_DIRS})
endif()

find_package(MP4V2 REQUIRED)
if(MP4V2_FOUND)
    include_directories(${MP4V2_INCLUDE_DIRS})
endif()

pkg_check_modules(GSTCODECPARSERLIB REQUIRED gstreamer-codecparsers-1.0)
if(GSTCODECPARSERLIB_FOUND)
    include_directories(${GSTCODECPARSERLIB_INCLUDE_DIRS})
    link_directories(${GSTCODECPARSERLIB_LIBRARY_DIRS})
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D__STDC_CONSTANT_MACROS")

add_library(fMP4-lib fMP4-lib.h fMP4-lib.hpp fMP4-lib-imp.hpp fMP4-lib-imp.cpp fMP4-lib.cpp)
target_link_libraries(fMP4-lib
        ${LIBAVCODEC_LIBRARIES}
        ${LIBAVUTIL_LIBRARIES}
        ${LIBAVFORMAT_LIBRARIES}
        ${LIBSWSCALE_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${GSTCODECPARSERLIB_LIBRARIES}
)

add_executable(main main.cpp)
target_link_libraries(main
        fMP4-lib
        ${MP4V2_LIBRARY}
)
