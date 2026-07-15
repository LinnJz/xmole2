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

    if(XMOLE2_WARNINGS_AS_ERRORS)
      target_compile_options(${target} ${visibility} /WX)
    endif()
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

    if(XMOLE2_WARNINGS_AS_ERRORS)
      target_compile_options(${target} ${visibility} -Werror)
    endif()
  endif()
endfunction()

function(xmole2_add_interface_module logical_name)
  string(REPLACE "-" "_" target_suffix "${logical_name}")
  set(target "xmole2_${target_suffix}")

  add_library(${target} INTERFACE)
  add_library("xmole2::${logical_name}" ALIAS ${target})
  set_target_properties(${target} PROPERTIES EXPORT_NAME "${logical_name}")
  target_compile_features(${target} INTERFACE cxx_std_23)

  install(TARGETS ${target} EXPORT xmole2Targets)
endfunction()

function(xmole2_assert_direct_dependencies target)
  get_target_property(actual_dependencies ${target} INTERFACE_LINK_LIBRARIES)
  if(NOT actual_dependencies)
    set(actual_dependencies)
  endif()

  set(project_dependencies)
  foreach(dependency IN LISTS actual_dependencies)
    if(dependency MATCHES "^xmole2::")
      list(APPEND project_dependencies "${dependency}")
    elseif(dependency MATCHES "^\\$<LINK_ONLY:(xmole2::[^>]+)>$")
      list(APPEND project_dependencies "${CMAKE_MATCH_1}")
    endif()
  endforeach()

  set(expected_dependencies ${ARGN})
  list(SORT project_dependencies)
  list(SORT expected_dependencies)

  if(NOT "${project_dependencies}" STREQUAL "${expected_dependencies}")
    message(FATAL_ERROR
      "Architecture dependency violation for ${target}: "
      "expected [${expected_dependencies}], actual [${project_dependencies}]"
    )
  endif()
endfunction()

function(xmole2_validate_architecture)
  xmole2_assert_direct_dependencies(xmole2_io xmole2::base)
  xmole2_assert_direct_dependencies(xmole2_xml xmole2::base xmole2::io)
  xmole2_assert_direct_dependencies(xmole2_crypto xmole2::base xmole2::io)
  xmole2_assert_direct_dependencies(xmole2_zip xmole2::base xmole2::io)
  xmole2_assert_direct_dependencies(xmole2_cfb xmole2::base xmole2::io)
  xmole2_assert_direct_dependencies(xmole2_opc
    xmole2::base xmole2::io xmole2::xml xmole2::zip)
  xmole2_assert_direct_dependencies(xmole2_office_encryption
    xmole2::base xmole2::io xmole2::crypto xmole2::cfb)
  xmole2_assert_direct_dependencies(xmole2_ooxml_core xmole2::base xmole2::xml)
  xmole2_assert_direct_dependencies(xmole2_drawingml
    xmole2::base xmole2::xml xmole2::ooxml-core)
  xmole2_assert_direct_dependencies(xmole2_words_model xmole2::base)
  xmole2_assert_direct_dependencies(xmole2_words_docx
    xmole2::words-model xmole2::opc xmole2::ooxml-core xmole2::drawingml)
  xmole2_assert_direct_dependencies(xmole2_words_doc
    xmole2::words-model xmole2::cfb xmole2::crypto)
  xmole2_assert_direct_dependencies(xmole2_words
    xmole2::words-model xmole2::words-docx xmole2::words-doc)
  xmole2_assert_direct_dependencies(xmole2_cells_model xmole2::base)
  xmole2_assert_direct_dependencies(xmole2_cells_xlsx
    xmole2::cells-model xmole2::opc xmole2::ooxml-core xmole2::drawingml)
  xmole2_assert_direct_dependencies(xmole2_cells_xls
    xmole2::cells-model xmole2::cfb xmole2::crypto)
  xmole2_assert_direct_dependencies(xmole2_cells_calc xmole2::cells-model)
  xmole2_assert_direct_dependencies(xmole2_cells
    xmole2::cells-model xmole2::cells-xlsx xmole2::cells-xls)
  xmole2_assert_direct_dependencies(xmole2_slides_model xmole2::base)
  xmole2_assert_direct_dependencies(xmole2_slides_pptx
    xmole2::slides-model xmole2::opc xmole2::ooxml-core xmole2::drawingml)
  xmole2_assert_direct_dependencies(xmole2_slides_ppt
    xmole2::slides-model xmole2::cfb xmole2::crypto)
  xmole2_assert_direct_dependencies(xmole2_slides
    xmole2::slides-model xmole2::slides-pptx xmole2::slides-ppt)
  xmole2_assert_direct_dependencies(xmole2_office_runtime xmole2::base xmole2::io)
  xmole2_assert_direct_dependencies(xmole2_office
    xmole2::office-runtime xmole2::office-encryption
    xmole2::words xmole2::cells xmole2::slides)
endfunction()
