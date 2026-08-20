cmake_minimum_required(VERSION 3.20)
project(miru VERSION 0.6.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # so clangd/your LSP can find flags and generated headers

set(MIRU_VERSION "${PROJECT_VERSION}")
message(STATUS "miru version: ${MIRU_VERSION}")

configure_file(
  ${CMAKE_SOURCE_DIR}/src/version.h.in
  ${CMAKE_BINARY_DIR}/generated/version.h
  @ONLY
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)

# ask pkg-config where the system wayland-protocols xml files live
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
if(NOT WAYLAND_PROTOCOLS_DIR)
  message(FATAL_ERROR "wayland-protocols not found, install with: pacman -S wayland-protocols")
endif()

include(cmake/WaylandScanner.cmake)

# wlr-screencopy-unstable-v1 is vendored, Niri doesn't advertise ext-image-copy-capture-v1 yet
set(SCREENCOPY_XML "${CMAKE_SOURCE_DIR}/protocol/wlr-screencopy-unstable-v1.xml")
if(NOT EXISTS ${SCREENCOPY_XML})
  message(FATAL_ERROR "protocol/wlr-screencopy-unstable-v1.xml missing, see README for the fetch command")
endif()


# wlr-layer-shell-unstable-v1 is vendored ourselves, see protocol/ dir
set(LAYER_SHELL_XML "${CMAKE_SOURCE_DIR}/protocol/wlr-layer-shell-unstable-v1.xml")
if(NOT EXISTS ${LAYER_SHELL_XML})
  message(FATAL_ERROR "protocol/wlr-layer-shell-unstable-v1.xml missing, see README for the fetch command")
endif()

# layer-shell references xdg_popup, so xdg-shell has to be linked in too
set(XDG_SHELL_XML "${WAYLAND_PROTOCOLS_DIR}/stable/xdg-shell/xdg-shell.xml")
if(NOT EXISTS ${XDG_SHELL_XML})
  message(FATAL_ERROR "xdg-shell.xml not found at ${XDG_SHELL_XML}")
endif()

# ext-image-copy-capture-v1 references ext_image_capture_source_v1, defined here
set(EXT_CAPTURE_SOURCE_XML "${WAYLAND_PROTOCOLS_DIR}/staging/ext-image-capture-source/ext-image-capture-source-v1.xml")
if(NOT EXISTS ${EXT_CAPTURE_SOURCE_XML})
  message(FATAL_ERROR "ext-image-capture-source-v1.xml not found at ${EXT_CAPTURE_SOURCE_XML}")
endif()

# ext-image-capture-source-v1's toplevel variant references ext_foreign_toplevel_handle_v1, defined here
set(EXT_FOREIGN_TOPLEVEL_XML "${WAYLAND_PROTOCOLS_DIR}/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml")
if(NOT EXISTS ${EXT_FOREIGN_TOPLEVEL_XML})
  message(FATAL_ERROR "ext-foreign-toplevel-list-v1.xml not found at ${EXT_FOREIGN_TOPLEVEL_XML}")
endif()

wayland_generate_protocol(${SCREENCOPY_XML} wlr-screencopy-unstable-v1)
wayland_generate_protocol(${LAYER_SHELL_XML} wlr-layer-shell-unstable-v1)
wayland_generate_protocol(${XDG_SHELL_XML} xdg-shell)
wayland_generate_protocol(${EXT_CAPTURE_SOURCE_XML} ext-image-capture-source-v1)
wayland_generate_protocol(${EXT_FOREIGN_TOPLEVEL_XML} ext-foreign-toplevel-list-v1)

pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLESV2 REQUIRED glesv2)
pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)

add_executable(miru-daemon
  src/main.c
  src/logo.c
  src/wayland_state.c
  src/layer_surface.c
  src/shm_buffer.c
  src/capture.c
  src/ipc_server.c
  src/input.c
  src/toml.c
  src/config.c
  src/config_watch.c
  src/egl_context.c
  src/gl_renderer.c
  ${wlr-layer-shell-unstable-v1_SOURCE}
  ${wlr-layer-shell-unstable-v1_HEADER}
  ${xdg-shell_SOURCE}
  ${xdg-shell_HEADER}
  ${wlr-screencopy-unstable-v1_SOURCE}
  ${wlr-screencopy-unstable-v1_HEADER}
)

add_executable(miructl 
  ctl/miructl.c
  src/logo.c
)

target_include_directories(miru-daemon PRIVATE
  ${WAYLAND_CLIENT_INCLUDE_DIRS}
  ${EGL_INCLUDE_DIRS}
  ${GLESV2_INCLUDE_DIRS}
  ${WAYLAND_EGL_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/protocol       # so #include "*-client-protocol.h" in main.c resolves
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(miru-daemon PRIVATE ${WAYLAND_CLIENT_LIBRARIES}
  ${EGL_LIBRARIES}
  ${GLESV2_LIBRARIES}
  ${WAYLAND_EGL_LIBRARIES}
)
target_compile_options(miru-daemon PRIVATE ${WAYLAND_CLIENT_CFLAGS_OTHER})

include(GNUInstallDirs)

install(TARGETS miru-daemon miructl
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

set(MAN_PAGES 
  "${CMAKE_SOURCE_DIR}/miru-daemon.1"
  "${CMAKE_SOURCE_DIR}/miructl.1"
)

foreach(MAN_PAGE ${MAN_PAGES})
  if(EXISTS "${MAN_PAGE}")
    get_filename_component(MAN_NAME ${MAN_PAGE} NAME)
    get_filename_component(MAN_EXT ${MAN_PAGE} EXT)
    string(SUBSTRING "${MAN_EXT}" 1 -1 SECTION_NUM)

    install(
      FILES "${MAN_PAGE}"
      DESTINATION "${CMAKE_INSTALL_MANDIR}/man${SECTION_NUM}"
    )

    else()
      message(WARNING "Pre-compiled man page not found ${MAN_PAGE}")
  endif()
endforeach()
