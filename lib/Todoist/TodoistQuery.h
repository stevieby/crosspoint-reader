#pragma once

#include <string>
#include <vector>

#include "TodoistTask.h"

// Pure, host-testable helpers for the Todoist screens: filter-query building,
// URL encoding, priority display, due-time formatting, and the timed-first
// ordering. Kept free of Arduino/ESP dependencies so the gtest suite can link
// them directly.
namespace todoist {

// Filter query for tasks due today (Todoist filter syntax).
std::string buildTodayQuery();

// Filter query for goal-labelled tasks due within the next 7 days.
// `label` is the bare label name (no leading '@').
std::string buildGoalsQuery(const std::string& label);

// Percent-encode a string for use in a URL query value.
std::string urlEncode(const std::string& s);

// Short display marker for a Todoist priority (4=highest). Returns "" for normal.
const char* priorityMarker(int priority);

// Extract "HH:MM" from an ISO datetime like "2024-06-30T14:30:00Z".
// Returns "" if no time component is present.
std::string dueTimeHHMM(const std::string& datetime);

// Order tasks for display: timed tasks first (ascending by time), then untimed
// tasks (descending by priority). Stable.
void sortTasks(std::vector<TodoistTask>& tasks);

}  // namespace todoist
