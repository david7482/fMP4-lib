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

pkg_check_modules(LIBSOUP REQUIRED libsoup-2.4)
if(LIBSOUP_FOUND)
    include_directories(${LIBSOUP_INCLUDE_DIRS})
    link_directories(${LIBSOUP_LIBRARY_DIRS})
endif()

pkg_check_modules(LIBSIGC REQUIRED sigc++-2.0)
if(LIBSIGC_FOUND)
    include_directories(${LIBSIGC_INCLUDE_DIRS})
    link_directories(${LIBSIGC_LIBRARY_DIRS})
endif()

pkg_check_modules(LIBGLIBMM REQUIRED glibmm-2.4)
if(LIBGLIBMM_FOUND)
    include_directories(${LIBGLIBMM_INCLUDE_DIRS})
    link_directories(${LIBGLIBMM_LIBRARY_DIRS})
endif()

pkg_check_modules(LIBGIOMM REQUIRED giomm-2.4)
if(LIBGIOMM_FOUND)
    include_directories(${LIBGIOMM_INCLUDE_DIRS})
    link_directories(${LIBGIOMM_LIBRARY_DIRS})
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D__STDC_CONSTANT_MACROS")

add_library(fMP4 STATIC fMP4.h fMP4.hpp fMP4-imp.hpp fMP4-imp.cpp fMP4.cpp)
target_link_libraries(fMP4
        ${LIBAVCODEC_LIBRARIES}
        ${LIBAVUTIL_LIBRARIES}
        ${LIBAVFORMAT_LIBRARIES}
        ${LIBSWSCALE_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${GSTCODECPARSERLIB_LIBRARIES}
)
install(TARGETS fMP4 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES fMP4.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/libfMP4)

add_executable(main-test main-test.cpp)
target_link_libraries(main-test
        fMP4
        ${MP4V2_LIBRARY}
)

add_executable(main-ws ws-client.hpp ws-client.cpp main-ws.cpp)
target_link_libraries(main-ws
        fMP4
        ${MP4V2_LIBRARY}
        ${LIBSOUP_LIBRARIES}
        ${LIBGIOMM_LIBRARIES}
        ${LIBGLIBMM_LIBRARIES}
        ${LIBSIGC_LIBRARIES}
)