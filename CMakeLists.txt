cmake_minimum_required(VERSION 3.25)

project(SYNTH VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json 3.11 REQUIRED)
find_package(OpenSSL 3.0 REQUIRED COMPONENTS Crypto)

find_package(jsoncons 1.9 CONFIG QUIET)
if(NOT jsoncons_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    jsoncons
    URL https://github.com/danielaparker/jsoncons/archive/refs/tags/v1.9.0.tar.gz
    URL_HASH SHA256=f1017b36e4e034acd5c0f5f616bacf5d7a161d6d3a43ff9ddb73fd8dca4d3cd9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR synth-no-cmake-subdirectory
  )
  FetchContent_MakeAvailable(jsoncons)
  add_library(synth_jsoncons INTERFACE)
  target_include_directories(synth_jsoncons SYSTEM INTERFACE "${jsoncons_SOURCE_DIR}/include")
  set(SYNTH_JSONCONS_TARGET synth_jsoncons)
else()
  if(TARGET jsoncons::jsoncons)
    set(SYNTH_JSONCONS_TARGET jsoncons::jsoncons)
  else()
    set(SYNTH_JSONCONS_TARGET jsoncons)
  endif()
endif()

add_executable(synth src/main.cpp src/realization.cpp)
set_target_properties(synth PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
target_compile_definitions(synth PRIVATE SYNTH_VERSION="${PROJECT_VERSION}")
target_compile_options(synth PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
get_target_property(SYNTH_JSONCONS_INCLUDE_DIRS ${SYNTH_JSONCONS_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(synth SYSTEM PRIVATE ${SYNTH_JSONCONS_INCLUDE_DIRS})
target_link_libraries(synth PRIVATE nlohmann_json::nlohmann_json OpenSSL::Crypto ${SYNTH_JSONCONS_TARGET})

set(SYNTH_BUILD_DATA_DIR "${CMAKE_BINARY_DIR}/share/synth")
file(MAKE_DIRECTORY "${SYNTH_BUILD_DATA_DIR}/schemas")
configure_file(LICENSE "${SYNTH_BUILD_DATA_DIR}/LICENSE" COPYONLY)
configure_file(docs/SYNTH-FOUNDATION-001-v0.4.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-FOUNDATION-001-v0.4.0.md" COPYONLY)
configure_file(docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md" COPYONLY)
configure_file(docs/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md" COPYONLY)
configure_file(docs/SYNTH-FOUNDATION-HARDENING-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-FOUNDATION-HARDENING-001-v0.1.0.md" COPYONLY)
configure_file(docs/SYNTH-REALIZATION-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-REALIZATION-001-v0.1.0.md" COPYONLY)
configure_file(docs/SYNTH-ECOSYSTEM-PROJECTION-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-ECOSYSTEM-PROJECTION-001-v0.1.0.md" COPYONLY)
configure_file(docs/SYNTH-LIVE-ECOSYSTEM-001-v0.1.0.md "${SYNTH_BUILD_DATA_DIR}/SYNTH-LIVE-ECOSYSTEM-001-v0.1.0.md" COPYONLY)
configure_file(schemas/surface.schema.json "${SYNTH_BUILD_DATA_DIR}/schemas/surface.schema.json" COPYONLY)
configure_file(schemas/relation.schema.json "${SYNTH_BUILD_DATA_DIR}/schemas/relation.schema.json" COPYONLY)
configure_file(schemas/artifact-manifest.schema.json "${SYNTH_BUILD_DATA_DIR}/schemas/artifact-manifest.schema.json" COPYONLY)
configure_file(schemas/realization-witness.schema.json "${SYNTH_BUILD_DATA_DIR}/schemas/realization-witness.schema.json" COPYONLY)
configure_file(schemas/ecosystem-projection.schema.json "${SYNTH_BUILD_DATA_DIR}/schemas/ecosystem-projection.schema.json" COPYONLY)

include(GNUInstallDirs)
install(TARGETS synth RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES
  LICENSE
  docs/SYNTH-FOUNDATION-001-v0.4.0.md
  docs/SYNTH-PATCH-FOUNDATION-001-v0.1.0.md
  docs/SYNTH-FOUNDATION-CORRECTION-001-v0.1.0.md
  docs/SYNTH-FOUNDATION-HARDENING-001-v0.1.0.md
  docs/SYNTH-REALIZATION-001-v0.1.0.md
  docs/SYNTH-ECOSYSTEM-PROJECTION-001-v0.1.0.md
  docs/SYNTH-LIVE-ECOSYSTEM-001-v0.1.0.md
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
  add_test(
    NAME realization-integration
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/realization_integration.sh
            $<TARGET_FILE:synth> ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()
