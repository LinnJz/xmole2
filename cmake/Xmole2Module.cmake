include_guard(GLOBAL)

function(xmole2_configure_target target visibility)
  target_compile_features(${target} ${visibility} cxx_std_23)

  if(MSVC)
    target_compile_options(${target} ${visibility}
      /bigobj
      /diagnostics:column
      /permissive-
      /nologo
      /sdl
      /utf-8
      /EHsc
      /FC
      /Gd
      /MP
      /W4
      /Zc:__cplusplus
      /Zc:forScope
      /Zc:inline
      /Zc:preprocessor
      /Zc:wchar_t
    )
  else()
    target_compile_options(${target} ${visibility}
      -fexec-charset=UTF-8
      -finput-charset=UTF-8
      -fstack-protector-strong
      -D_FORTIFY_SOURCE=2
      -Wall
      -Wconversion
      -Wdouble-promotion
      -Wextra
      -Wformat-security
      -Wpedantic
      -Wshadow
    )
  endif()
endfunction()

function(xmole2_add_interface_module logical_name)
  string(REPLACE "-" "_" target_suffix "${logical_name}")
  set(target "xmole2_${target_suffix}")

  add_library(${target} INTERFACE)
  add_library("xmole2::${logical_name}" ALIAS ${target})
  set_target_properties(${target} PROPERTIES EXPORT_NAME "${logical_name}")
  xmole2_configure_target(${target} INTERFACE)

  install(TARGETS ${target} EXPORT xmole2Targets)
endfunction()

