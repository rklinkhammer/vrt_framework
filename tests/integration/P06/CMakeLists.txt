vita_add_test(p06_loopback_transaction ${CMAKE_CURRENT_SOURCE_DIR}/chain.cpp)
set_tests_properties(p06_loopback_transaction PROPERTIES LABELS "integration;transaction;ownership")
