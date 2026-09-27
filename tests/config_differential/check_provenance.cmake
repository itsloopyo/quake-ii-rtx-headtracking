# Fails when a file provenance.txt lists no longer has the SHA-256 it records, so the differential
# test cannot go on claiming to compile the dev build's reader after either copy has changed.
# Run as: cmake -DREPO=<repo root> -P check_provenance.cmake
file(STRINGS "${REPO}/tests/config_differential/provenance.txt" lines)
set(checked 0)
foreach(line IN LISTS lines)
    if(line MATCHES "^[ \t]*(#|$)")
        continue()
    endif()
    string(REGEX MATCH "^([0-9a-f]+)[ \t]+([^ \t]+)" _ "${line}")
    set(expected "${CMAKE_MATCH_1}")
    set(path "${CMAKE_MATCH_2}")
    file(SHA256 "${REPO}/${path}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "${path} has changed: sha256 ${actual}, provenance.txt records ${expected}")
    endif()
    math(EXPR checked "${checked} + 1")
endforeach()
message(STATUS "provenance: ${checked} files hash as recorded")
