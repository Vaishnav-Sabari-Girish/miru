find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner)
if(NOT WAYLAND_SCANNER_EXECUTABLE)
  message(FATAL_ERROR "wayland-scanner not found, install with: pacman -S wayland")
endif()

function(wayland_generate_protocol xml_path protocol_name)
  set(out_header "${CMAKE_BINARY_DIR}/protocol/${protocol_name}-client-protocol.h")
  set(out_source "${CMAKE_BINARY_DIR}/protocol/${protocol_name}-client-protocol.c")

  add_custom_command(
    OUTPUT ${out_header}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${xml_path} ${out_header}
    DEPENDS ${xml_path}
    COMMENT "Generating ${protocol_name} client header"
  )
  add_custom_command(
    OUTPUT ${out_source}
    COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${xml_path} ${out_source}
    DEPENDS ${xml_path}
    COMMENT "Generating ${protocol_name} private code"
  )

  set(${protocol_name}_HEADER ${out_header} PARENT_SCOPE)
  set(${protocol_name}_SOURCE ${out_source} PARENT_SCOPE)
endfunction()
