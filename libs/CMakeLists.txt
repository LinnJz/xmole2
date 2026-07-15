add_subdirectory(base)
add_subdirectory(io)

xmole2_add_interface_module(xml)
target_link_libraries(xmole2_xml INTERFACE xmole2::base xmole2::io)

xmole2_add_interface_module(crypto)
target_link_libraries(xmole2_crypto INTERFACE xmole2::base xmole2::io)

add_subdirectory(zip)

xmole2_add_interface_module(cfb)
target_link_libraries(xmole2_cfb INTERFACE xmole2::base xmole2::io)

xmole2_add_interface_module(opc)
target_link_libraries(xmole2_opc INTERFACE xmole2::base xmole2::io xmole2::xml xmole2::zip)

xmole2_add_interface_module(office-encryption)
target_link_libraries(xmole2_office_encryption INTERFACE
  xmole2::base
  xmole2::io
  xmole2::crypto
  xmole2::cfb
)

xmole2_add_interface_module(ooxml-core)
target_link_libraries(xmole2_ooxml_core INTERFACE xmole2::base xmole2::xml)

xmole2_add_interface_module(drawingml)
target_link_libraries(xmole2_drawingml INTERFACE
  xmole2::base
  xmole2::xml
  xmole2::ooxml-core
)

xmole2_add_interface_module(words-model)
target_link_libraries(xmole2_words_model INTERFACE xmole2::base)

xmole2_add_interface_module(words-docx)
target_link_libraries(xmole2_words_docx INTERFACE
  xmole2::words-model
  xmole2::opc
  xmole2::ooxml-core
  xmole2::drawingml
)

xmole2_add_interface_module(words-doc)
target_link_libraries(xmole2_words_doc INTERFACE
  xmole2::words-model
  xmole2::cfb
  xmole2::crypto
)

xmole2_add_interface_module(words)
target_link_libraries(xmole2_words INTERFACE
  xmole2::words-model
  xmole2::words-docx
  xmole2::words-doc
)

xmole2_add_interface_module(cells-model)
target_link_libraries(xmole2_cells_model INTERFACE xmole2::base)

xmole2_add_interface_module(cells-xlsx)
target_link_libraries(xmole2_cells_xlsx INTERFACE
  xmole2::cells-model
  xmole2::opc
  xmole2::ooxml-core
  xmole2::drawingml
)

xmole2_add_interface_module(cells-xls)
target_link_libraries(xmole2_cells_xls INTERFACE
  xmole2::cells-model
  xmole2::cfb
  xmole2::crypto
)

xmole2_add_interface_module(cells-calc)
target_link_libraries(xmole2_cells_calc INTERFACE xmole2::cells-model)

xmole2_add_interface_module(cells)
target_link_libraries(xmole2_cells INTERFACE
  xmole2::cells-model
  xmole2::cells-xlsx
  xmole2::cells-xls
)

xmole2_add_interface_module(slides-model)
target_link_libraries(xmole2_slides_model INTERFACE xmole2::base)

xmole2_add_interface_module(slides-pptx)
target_link_libraries(xmole2_slides_pptx INTERFACE
  xmole2::slides-model
  xmole2::opc
  xmole2::ooxml-core
  xmole2::drawingml
)

xmole2_add_interface_module(slides-ppt)
target_link_libraries(xmole2_slides_ppt INTERFACE
  xmole2::slides-model
  xmole2::cfb
  xmole2::crypto
)

xmole2_add_interface_module(slides)
target_link_libraries(xmole2_slides INTERFACE
  xmole2::slides-model
  xmole2::slides-pptx
  xmole2::slides-ppt
)

xmole2_add_interface_module(office-runtime)
target_link_libraries(xmole2_office_runtime INTERFACE xmole2::base xmole2::io)

xmole2_add_interface_module(office)
target_link_libraries(xmole2_office INTERFACE
  xmole2::office-runtime
  xmole2::office-encryption
  xmole2::words
  xmole2::cells
  xmole2::slides
)
