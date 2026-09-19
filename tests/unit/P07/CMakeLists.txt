vita_add_test(p07_transactions ${CMAKE_CURRENT_SOURCE_DIR}/transactions.cpp)
set_tests_properties(p07_transactions PROPERTIES LABELS "transaction;integration;budget")
vita_add_test(p07_timing ${CMAKE_CURRENT_SOURCE_DIR}/timing.cpp)
set_tests_properties(p07_timing PROPERTIES LABELS "transaction;timing")

vita_add_test(p07_controller ${CMAKE_CURRENT_SOURCE_DIR}/controller.cpp)
set_tests_properties(p07_controller PROPERTIES LABELS "transaction;controller;budget")
