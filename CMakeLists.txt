cmake_minimum_required(VERSION 3.25)

project(SYNTH VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(synth src/main.cpp)
set_target_properties(synth PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(synth PRIVATE SYNTH_VERSION="${PROJECT_VERSION}")
target_compile_options(synth PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

set(SYNTH_BUILD_DATA_DIR "${CMAKE_BINARY_DIR}/share/synth")
file(MAKE_DIRECTORY "${SYNTH_BUILD_DATA_DIR}/schemas")
configure_file(LICENSE "${SYNTH_BUILD_DATA_DIR}/LICENSE" COPYONLY)
configure_file(docs/SYNTH-FOUNDATION-001-v0.4.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-FOUNDATION-001-v0.4.0.md" COPYONLY)
configure_file(docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md" COPYONLY)
configure_file(docs/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md" COPYONLY)
configure_file(schemas/surface.schema.json "${SYNTH_BUILD_DATA_DIR}/schemas/surface.schema.json" COPYONLY)
configure_file(schemas/relation.schema.json "${SYNTH_BUILD_DATA_DIR}/schemas/relation.schema.json" COPYONLY)

include(GNUInstallDirs)
install(TARGETS synth RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES
  LICENSE
  docs/SYNTH-FOUNDATION-001-v0.4.0.md
  docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md
  docs/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md
  DESTINATION ${CMAKE_INSTALL_DATADIR}/synth
)
install(DIRECTORY schemas/ DESTINATION ${CMAKE_INSTALL_DATADIR}/synth/schemas)

include(CTest)
if(BUILD_TESTING)
  add_test(
    NAME foundation-conformance
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance.sh
            $<TARGET_FILE:synth> ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}
  )
endif()
