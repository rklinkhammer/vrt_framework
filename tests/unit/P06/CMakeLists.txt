vita_add_test(p06_engine ${CMAKE_CURRENT_SOURCE_DIR}/engine.cpp)
set_tests_properties(p06_engine PROPERTIES LABELS "transaction;integration;budget")
vita_add_test(p06_outcomes ${CMAKE_CURRENT_SOURCE_DIR}/outcomes.cpp)
set_tests_properties(p06_outcomes PROPERTIES LABELS "transaction")
