cmake_minimum_required(VERSION 3.20)
project(miru C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # so clangd/your LSP can find flags and generated headers

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

add_executable(miru-daemon
  src/main.c
  src/wayland_state.c
  src/layer_surface.c
  src/shm_buffer.c
  ${wlr-layer-shell-unstable-v1_SOURCE}
  ${wlr-layer-shell-unstable-v1_HEADER}
  ${xdg-shell_SOURCE}
  ${xdg-shell_HEADER}
  ${wlr-screencopy-unstable-v1_SOURCE}
  ${wlr-screencopy-unstable-v1_HEADER}
)

target_include_directories(miru-daemon PRIVATE
  ${WAYLAND_CLIENT_INCLUDE_DIRS}
  ${CMAKE_BINARY_DIR}/protocol       # so #include "*-client-protocol.h" in main.c resolves
  ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(miru-daemon PRIVATE ${WAYLAND_CLIENT_LIBRARIES})
target_compile_options(miru-daemon PRIVATE ${WAYLAND_CLIENT_CFLAGS_OTHER})
