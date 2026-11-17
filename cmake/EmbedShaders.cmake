# Embeds GLSL files as C string constants
#
#   miru_embed_shaders(<output_dir> <shader> ...)
#
# Writes <output_dir>/shaders.c and shaders.h. Shader paths are relative to the
# source dir; "quad.vert" becomes `miru_shader_quad_vert`.
function(miru_embed_shaders output_dir)
  set(c_body "#include \"shaders.h\"\n\n")
  set(h_body "#ifndef MIRU_SHADERS_H\n#define MIRU_SHADERS_H\n\n")

  foreach(shader ${ARGN})
    get_filename_component(shader_name "${shader}" NAME)
    string(MAKE_C_IDENTIFIER "${shader_name}" shader_id)
    file(READ "${CMAKE_SOURCE_DIR}/${shader}" shader_src)
    string(REPLACE "\\" "\\\\" shader_src "${shader_src}")
    string(REPLACE "\"" "\\\"" shader_src "${shader_src}")
    string(REPLACE "\n" "\\n\"\n    \"" shader_src "${shader_src}")
    string(APPEND c_body "const char miru_shader_${shader_id}[] = \n    \"${shader_src}\";\n\n")
    string(APPEND h_body "extern const char miru_shader_${shader_id}[];\n")
    set_property(
      DIRECTORY
      APPEND
      PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${shader}"
    )
  endforeach()

  string(APPEND h_body "\n#endif\n")
  file(CONFIGURE OUTPUT "${output_dir}/shaders.c" CONTENT "${c_body}" @ONLY)
  file(CONFIGURE OUTPUT "${output_dir}/shaders.h" CONTENT "${h_body}" @ONLY)
endfunction()
