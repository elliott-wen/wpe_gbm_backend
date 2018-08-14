cmake_minimum_required(VERSION 3.0)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

option(WPE_MESA_GBM "Whether to enable support for the libgbm-based buffer sharing" ON)


find_package(EGL REQUIRED)
find_package(GLIB 2.40.0 REQUIRED COMPONENTS gio gio-unix gobject gthread gmodule)
find_package(Libxkbcommon 0.4.0 REQUIRED)
add_definitions(-DWPE_BACKEND_DRM=1)
add_definitions(-DWPE_BACKEND_WAYLAND=1)
add_definitions(${EGL_DEFINITIONS})

pkg_check_modules(WPE wpe-0.1 REQUIRED)

set(WPE_MESA_INCLUDE_DIRECTORIES
    "src/drm"
    "src/external"
    "src/libxkbcommon"
    "src/util"
    ${EGL_INCLUDE_DIRS}
    ${GIO_UNIX_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${LIBXKBCOMMON_INCLUDE_DIRS}
    ${WAYLAND_INCLUDE_DIRS}
    ${WAYLAND_CURSOR_INCLUDE_DIRS}
    ${WPE_INCLUDE_DIRS}
)
set(WPE_MESA_LIBRARIES
    ${EGL_LIBRARIES}
    ${GIO_UNIX_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${WPE_LIBRARIES}
)
set(WPE_MESA_SOURCES
    src/mesa.cpp

    
    src/libxkbcommon/input-libxkbcommon.cpp
    src/util/ipc.cpp

)

if (WPE_MESA_GBM)
    add_definitions(-DWPE_MESA_GBM=1)
    find_package(LibGBM REQUIRED)

    list(APPEND WPE_MESA_INCLUDE_DIRECTORIES
        "src/gbm"
        ${LIBDRM_INCLUDE_DIRS}
        ${LIBGBM_INCLUDE_DIRS}
    )

    list(APPEND WPE_MESA_LIBRARIES
        ${LIBDRM_LIBRARIES}
        ${LIBGBM_LIBRARIES}
    )

    list(APPEND WPE_MESA_SOURCES
        src/drm/view-backend-drm.cpp
        src/drm/inputserver.cpp
        src/drm/KeyboardEventHandlerLinuxInput.cpp
        src/gbm/renderer-backend-egl-gbm.cpp
        src/gbm/renderer-host-gbm.cpp

    )

  
endif ()





link_directories(${WPE_LIBRARY_DIRS})
add_library(WPEBackend-mesa SHARED ${WPE_MESA_SOURCES})
target_include_directories(WPEBackend-mesa PRIVATE ${WPE_MESA_INCLUDE_DIRECTORIES})
target_link_libraries(WPEBackend-mesa ${WPE_MESA_LIBRARIES})

set(WPE_MESA_VERSION_MAJOR 0)
set(WPE_MESA_VERSION_MINOR 1)
set(WPE_MESA_VERSION ${WPE_MESA_VERSION_MAJOR}.${WPE_MESA_VERSION_MINOR})

set_target_properties(WPEBackend-mesa PROPERTIES VERSION ${WPE_MESA_VERSION} SOVERSION ${WPE_MESA_VERSION_MAJOR})
install(TARGETS WPEBackend-mesa
  EXPORT WPEBackend-mesaTargets
  LIBRARY DESTINATION lib
  INCLUDES DESTINATION include
)

if(WPE_MESA_PUBLIC_HEADERS)
  install(FILES ${WPE_MESA_PUBLIC_HEADERS} DESTINATION include/wpe-mesa)
endif()

# Create a libWPEBackend-default.so symlink to libWPEBackend-mesa.so and install it.
add_custom_command(TARGET WPEBackend-mesa
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink libWPEBackend-mesa.so ${CMAKE_BINARY_DIR}/libWPEBackend-default.so
)
install(FILES ${CMAKE_BINARY_DIR}/libWPEBackend-default.so DESTINATION lib)

configure_file(wpe-mesa.pc.cmake wpe-mesa.pc @ONLY)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/wpe-mesa.pc"
    DESTINATION
    lib/pkgconfig
)
