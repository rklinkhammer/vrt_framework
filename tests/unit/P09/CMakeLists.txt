vita_add_test(p09_context ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp)
set_tests_properties(p09_context PROPERTIES LABELS "context;revision;budget")
vita_add_test(p09_receiver ${CMAKE_CURRENT_SOURCE_DIR}/receiver.cpp)
set_tests_properties(p09_receiver PROPERTIES LABELS "context;ownership;allocation")
