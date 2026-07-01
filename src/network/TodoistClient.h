#pragma once

#include <TodoistTask.h>

#include <string>
#include <utility>
#include <vector>

/**
 * Minimal read-only client for the Todoist unified API v1.
 *
 * Fetches active tasks via the filter endpoint and resolves project/section
 * names for display. All requests authenticate with a Bearer token and stream
 * through HttpDownloader (verified HTTPS). Responses are parsed with a field
 * filter to keep the JSON document small on the constrained heap.
 */
class TodoistClient {
 public:
  explicit TodoistClient(std::string bearerToken) : token(std::move(bearerToken)) {}

  // Fetch tasks due today, ordered timed-first, with project/section names
  // resolved. Returns false on any network or parse failure.
  bool fetchTodayTasks(std::vector<TodoistTask>& out);

  // Fetch tasks carrying `label` due within the next 7 days, same ordering and
  // name resolution. Returns false on failure.
  bool fetchGoals(const std::string& label, std::vector<TodoistTask>& out);

  // Mark the task with the given id complete (Todoist "close"). Returns true on
  // success.
  bool closeTask(const std::string& id);

 private:
  using NameMap = std::vector<std::pair<std::string, std::string>>;  // id -> name

  std::string token;
  NameMap projects;
  NameMap sections;
  bool mapsLoaded = false;

  bool fetchTasksByQuery(const std::string& query, std::vector<TodoistTask>& out);
  bool ensureMaps();
  bool fetchNameMap(const std::string& path, NameMap& out);
  void resolveNames(std::vector<TodoistTask>& tasks) const;
};
