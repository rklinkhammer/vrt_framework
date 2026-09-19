vita_add_test(p00_verify_features features.cpp)
vita_add_test(p00_verify_multi_tu main.cpp policy_a.cpp policy_b.cpp)
set_tests_properties(p00_verify_features p00_verify_multi_tu PROPERTIES LABELS "compile")
add_test(NAME p00_verify_bad_library
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_bad_library.py" "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_FLAGS}")
set_tests_properties(p00_verify_bad_library PROPERTIES LABELS "compile")
add_test(NAME p00_verify_core_only
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/check_core_only.py" "${CMAKE_CXX_COMPILER}" "${CMAKE_CXX_FLAGS}")
set_tests_properties(p00_verify_core_only PROPERTIES LABELS "compile")
