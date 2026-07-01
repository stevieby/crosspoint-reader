#include "TodoistQuery.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "TodoistTask.h"

TEST(TodoistQuery, TodayQueryIsLiteral) { EXPECT_EQ(todoist::buildTodayQuery(), "today"); }

TEST(TodoistQuery, GoalsQueryWrapsLabel) {
  EXPECT_EQ(todoist::buildGoalsQuery("Goal"), "@Goal & 7 days");
  EXPECT_EQ(todoist::buildGoalsQuery("BigRocks"), "@BigRocks & 7 days");
}

TEST(TodoistQuery, UrlEncodeEscapesReservedChars) {
  // Space -> %20, & -> %26, @ -> %40. Unreserved chars pass through.
  EXPECT_EQ(todoist::urlEncode("@Goal & 7 days"), "%40Goal%20%26%207%20days");
  EXPECT_EQ(todoist::urlEncode("a-b_c.d~e"), "a-b_c.d~e");
}

TEST(TodoistQuery, PriorityMarker) {
  EXPECT_STREQ(todoist::priorityMarker(4), "!!!");
  EXPECT_STREQ(todoist::priorityMarker(3), "!!");
  EXPECT_STREQ(todoist::priorityMarker(2), "!");
  EXPECT_STREQ(todoist::priorityMarker(1), "");
  EXPECT_STREQ(todoist::priorityMarker(0), "");
}

TEST(TodoistQuery, DueTimeExtraction) {
  EXPECT_EQ(todoist::dueTimeHHMM("2024-06-30T14:30:00Z"), "14:30");
  EXPECT_EQ(todoist::dueTimeHHMM("2024-06-30T09:05:00.000000Z"), "09:05");
  EXPECT_EQ(todoist::dueTimeHHMM("2024-06-30"), "");  // date only, no time
  EXPECT_EQ(todoist::dueTimeHHMM(""), "");
  EXPECT_EQ(todoist::dueTimeHHMM("2024-06-30Tab:cd"), "");  // malformed time
}

namespace {
TodoistTask makeTask(const std::string& id, int priority, const std::string& datetime) {
  TodoistTask t;
  t.id = id;
  t.priority = priority;
  t.dueDatetime = datetime;
  return t;
}
}  // namespace

TEST(TodoistQuery, SortTimedFirstThenByPriority) {
  std::vector<TodoistTask> tasks;
  tasks.push_back(makeTask("untimed-low", 1, ""));
  tasks.push_back(makeTask("timed-late", 2, "2024-06-30T18:00:00"));
  tasks.push_back(makeTask("untimed-high", 4, ""));
  tasks.push_back(makeTask("timed-early", 1, "2024-06-30T08:00:00"));

  todoist::sortTasks(tasks);

  // Timed tasks come first, ordered by time ascending.
  EXPECT_EQ(tasks[0].id, "timed-early");
  EXPECT_EQ(tasks[1].id, "timed-late");
  // Then untimed tasks, highest priority first.
  EXPECT_EQ(tasks[2].id, "untimed-high");
  EXPECT_EQ(tasks[3].id, "untimed-low");
}

TEST(TodoistQuery, SortIsStableForEqualKeys) {
  std::vector<TodoistTask> tasks;
  tasks.push_back(makeTask("a", 2, ""));
  tasks.push_back(makeTask("b", 2, ""));
  tasks.push_back(makeTask("c", 2, ""));

  todoist::sortTasks(tasks);

  EXPECT_EQ(tasks[0].id, "a");
  EXPECT_EQ(tasks[1].id, "b");
  EXPECT_EQ(tasks[2].id, "c");
}
