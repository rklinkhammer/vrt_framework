vita_add_test(p11_runtime ${CMAKE_CURRENT_SOURCE_DIR}/runtime.cpp)
set_tests_properties(p11_runtime PROPERTIES LABELS "lifecycle;integration")

vita_add_test(p11_transactions ${CMAKE_CURRENT_SOURCE_DIR}/transactions.cpp)
set_tests_properties(p11_transactions PROPERTIES LABELS "lifecycle;transaction")
