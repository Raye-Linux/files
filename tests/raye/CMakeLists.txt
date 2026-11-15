# Raye tests. Every tests/raye/test_*.cpp is one QtTest linked like Dolphin's
# own tests (the private library plus the static part of the app), every
# tests/raye/test_*.sh runs with sh, and every tests/raye/test_*.py runs with
# python3 -I -B. All of them carry the label "raye":
#   ctest --test-dir build -L raye
# Strings in Raye tests use the raye-files catalog, like src/raye.

# Dolphin's headers are built with the implicit QString conversions on, so
# anything that includes them needs the same relaxation src/ uses.
remove_definitions(
    -DQT_NO_CAST_FROM_BYTEARRAY
    -DQT_NO_CAST_FROM_ASCII
    -DQT_NO_CAST_TO_ASCII
)

remove_definitions(-DTRANSLATION_DOMAIN=\"dolphin\")
add_definitions(-DTRANSLATION_DOMAIN=\"raye-files\")

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Test)
include(ECMAddTests)

set(RAYE_TEST_ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen"
    "QT_QUICK_BACKEND=software"
    "RAYE_FILES_SOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "RAYE_FILES_BINARY_DIR=${CMAKE_BINARY_DIR}"
)

file(GLOB raye_cpp_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
foreach(test_source ${raye_cpp_tests})
    get_filename_component(test_name "${test_source}" NAME_WE)
    ecm_add_test("${test_source}"
        TEST_NAME raye_${test_name}
        LINK_LIBRARIES dolphinprivate dolphinstatic Qt6::Test
    )
    target_include_directories(raye_${test_name} PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${CMAKE_BINARY_DIR}/src"
    )
    target_compile_definitions(raye_${test_name} PRIVATE
        RAYE_FILES_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        RAYE_FILES_BINARY_DIR="${CMAKE_BINARY_DIR}"
    )
    # The catalogs are built into ${CMAKE_BINARY_DIR}/locale before tests run.
    if(TARGET pofiles)
        add_dependencies(raye_${test_name} pofiles)
    endif()
    set_tests_properties(raye_${test_name} PROPERTIES
        LABELS raye
        ENVIRONMENT "${RAYE_TEST_ENVIRONMENT}"
    )
endforeach()

file(GLOB raye_sh_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.sh")
foreach(test_script ${raye_sh_tests})
    get_filename_component(test_name "${test_script}" NAME_WE)
    add_test(NAME raye_${test_name} COMMAND sh "${test_script}")
    set_tests_properties(raye_${test_name} PROPERTIES
        LABELS raye
        ENVIRONMENT "${RAYE_TEST_ENVIRONMENT}"
    )
endforeach()

# -I so the interpreter ignores the environment, -B so it writes no
# __pycache__ into the source tree: -I alone makes it ignore
# PYTHONDONTWRITEBYTECODE, and unittest compiles the test file before the
# file itself can say anything about bytecode.
file(GLOB raye_py_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.py")
foreach(test_script ${raye_py_tests})
    get_filename_component(test_name "${test_script}" NAME_WE)
    add_test(NAME raye_${test_name} COMMAND python3 -I -B "${test_script}")
    set_tests_properties(raye_${test_name} PROPERTIES
        LABELS raye
        ENVIRONMENT "${RAYE_TEST_ENVIRONMENT}"
    )
endforeach()
