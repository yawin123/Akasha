if(NOT DEFINED AKASHA_AR OR AKASHA_AR STREQUAL "")
    message(FATAL_ERROR "AKASHA_AR is required")
endif()

if(NOT DEFINED AKASHA_OUTPUT OR AKASHA_OUTPUT STREQUAL "")
    message(FATAL_ERROR "AKASHA_OUTPUT is required")
endif()

if(NOT DEFINED AKASHA_INPUTS OR AKASHA_INPUTS STREQUAL "")
    message(FATAL_ERROR "AKASHA_INPUTS is required")
endif()

set(_bundle_dir "${CMAKE_CURRENT_BINARY_DIR}/akasha_bundle_tmp")
file(REMOVE_RECURSE "${_bundle_dir}")
file(MAKE_DIRECTORY "${_bundle_dir}")

set(_archive_index 0)
foreach(_archive IN LISTS AKASHA_INPUTS)
    if(NOT EXISTS "${_archive}")
        message(FATAL_ERROR "Archive not found: ${_archive}")
    endif()

    math(EXPR _archive_index "${_archive_index} + 1")
    set(_extract_dir "${_bundle_dir}/archive_${_archive_index}")
    file(MAKE_DIRECTORY "${_extract_dir}")

    execute_process(
        COMMAND "${AKASHA_AR}" -x "${_archive}"
        WORKING_DIRECTORY "${_extract_dir}"
        RESULT_VARIABLE _extract_result
        OUTPUT_VARIABLE _extract_stdout
        ERROR_VARIABLE _extract_stderr
    )

    if(NOT _extract_result EQUAL 0)
        message(FATAL_ERROR
            "Failed extracting '${_archive}' with ar (${_extract_result}).\n"
            "STDOUT: ${_extract_stdout}\n"
            "STDERR: ${_extract_stderr}")
    endif()
endforeach()

file(GLOB_RECURSE _object_files "${_bundle_dir}/*.o" "${_bundle_dir}/*.obj")
if(_object_files STREQUAL "")
    message(FATAL_ERROR "No object files found while bundling static archives")
endif()

get_filename_component(_output_dir "${AKASHA_OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_dir}")
file(REMOVE "${AKASHA_OUTPUT}")

execute_process(
    COMMAND "${AKASHA_AR}" qc "${AKASHA_OUTPUT}" ${_object_files}
    RESULT_VARIABLE _create_result
    OUTPUT_VARIABLE _create_stdout
    ERROR_VARIABLE _create_stderr
)

if(NOT _create_result EQUAL 0)
    message(FATAL_ERROR
        "Failed creating bundled archive '${AKASHA_OUTPUT}' (${_create_result}).\n"
        "STDOUT: ${_create_stdout}\n"
        "STDERR: ${_create_stderr}")
endif()

if(DEFINED AKASHA_RANLIB AND NOT AKASHA_RANLIB STREQUAL "")
    execute_process(
        COMMAND "${AKASHA_RANLIB}" "${AKASHA_OUTPUT}"
        RESULT_VARIABLE _ranlib_result
        OUTPUT_VARIABLE _ranlib_stdout
        ERROR_VARIABLE _ranlib_stderr
    )

    if(NOT _ranlib_result EQUAL 0)
        message(FATAL_ERROR
            "ranlib failed for '${AKASHA_OUTPUT}' (${_ranlib_result}).\n"
            "STDOUT: ${_ranlib_stdout}\n"
            "STDERR: ${_ranlib_stderr}")
    endif()
endif()

message(STATUS "Bundled archive created at: ${AKASHA_OUTPUT}")
