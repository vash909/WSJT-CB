if (NOT WSJT_BUILD_DECODER_TESTS)
  return ()
endif ()

if (NOT OPENMP_FOUND)
  message (FATAL_ERROR "FT8 concurrency regression tests require OpenMP")
endif ()

foreach (test_name ft8_decode_ranges ft8_mtd_residual ft8_mtd_transaction_concurrent cb_mtd)
  add_executable (test_${test_name} ft8/test_${test_name}.f90)
  target_compile_options (test_${test_name} PRIVATE -fopenmp)
  target_link_libraries (test_${test_name} wsjt_fort_omp wsjt_cxx)
  add_test (NAME ${test_name} COMMAND test_${test_name})
  set_tests_properties (${test_name} PROPERTIES TIMEOUT 60
    ENVIRONMENT "OMP_NUM_THREADS=4;OMP_THREAD_LIMIT=4;OMP_DYNAMIC=FALSE")
endforeach ()

find_package (Python3 REQUIRED COMPONENTS Interpreter)
set (autoseq_methods ${CMAKE_CURRENT_BINARY_DIR}/cb_autoseq_methods.inc)
add_custom_command (OUTPUT ${autoseq_methods}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ft8/extract_autoseq.py
    ${CMAKE_SOURCE_DIR}/widgets/mainwindow.cpp ${autoseq_methods}
  DEPENDS ft8/extract_autoseq.py ${CMAKE_SOURCE_DIR}/widgets/mainwindow.cpp
  VERBATIM)
add_executable (test_cb_autoseq ft8/test_cb_autoseq.cpp ${autoseq_methods}
  ${CMAKE_SOURCE_DIR}/Radio.cpp ${CMAKE_SOURCE_DIR}/Decoder/decodedtext.cpp)
target_include_directories (test_cb_autoseq PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR})
target_compile_options (test_cb_autoseq PRIVATE -include ${CMAKE_BINARY_DIR}/wsjtx_config.h)
target_link_libraries (test_cb_autoseq Qt5::Core Qt5::Network wsjt_fort wsjt_cxx)
add_test (NAME cb_autoseq COMMAND test_cb_autoseq)
set_tests_properties (cb_autoseq PROPERTIES TIMEOUT 30)

add_executable (test_ft8sim ${CMAKE_SOURCE_DIR}/lib/ft8/ft8sim.f90)
target_link_libraries (test_ft8sim wsjt_fort wsjt_cxx)
add_test (NAME cb_decoder_integration
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ft8/test_decoder_integration.py
    $<TARGET_FILE:jt9> $<TARGET_FILE:test_ft8sim> $<TARGET_FILE:test_cb_autoseq>)
set_tests_properties (cb_decoder_integration PROPERTIES TIMEOUT 240)
