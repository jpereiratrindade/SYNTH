cmake_minimum_required(VERSION 3.28)

project(SYNTH VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(synth src/main.cpp)
target_compile_definitions(synth PRIVATE
  SYNTH_VERSION="${PROJECT_VERSION}"
  SYNTH_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
target_compile_options(synth PRIVATE -Wall -Wextra -Wpedantic -Wconversion)

include(GNUInstallDirs)
install(TARGETS synth RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CTest)
if(BUILD_TESTING)
  add_test(
    NAME foundation-conformance
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance.sh
            $<TARGET_FILE:synth> ${CMAKE_CURRENT_SOURCE_DIR}
  )
endif()
