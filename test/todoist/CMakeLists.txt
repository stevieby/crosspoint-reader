add_executable(TodoistQueryTest
  TodoistQueryTest.cpp
  ${REPO_ROOT}/lib/Todoist/TodoistQuery.cpp
)

target_include_directories(TodoistQueryTest PRIVATE
  ${REPO_ROOT}/lib/Todoist
)

target_link_libraries(TodoistQueryTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(TodoistQueryTest)
