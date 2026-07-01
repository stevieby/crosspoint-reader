#include "TodoistClient.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <TodoistQuery.h>

#include <cstdio>

#include "network/HttpDownloader.h"

namespace {
constexpr const char* API_BASE = "https://api.todoist.com/api/v1";
constexpr int PAGE_LIMIT = 50;  // tasks/projects per page; keeps each JSON doc small
constexpr int MAX_PAGES = 30;   // safety cap so a bad next_cursor can't loop forever

// Todoist v1 ids are strings, but tolerate integers defensively.
std::string idToString(JsonVariantConst v) {
  const char* s = v.as<const char*>();
  if (s) return s;
  if (v.is<long long>()) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%lld", v.as<long long>());
    return buf;
  }
  return {};
}
}  // namespace

bool TodoistClient::fetchTasksByQuery(const std::string& query, std::vector<TodoistTask>& out) {
  // Field filter: only deserialize the fields we render, to bound heap use.
  JsonDocument filter;
  filter["next_cursor"] = true;
  JsonObject el = filter["results"][0].to<JsonObject>();
  el["id"] = true;
  el["content"] = true;
  el["description"] = true;
  el["priority"] = true;
  el["project_id"] = true;
  el["section_id"] = true;
  el["due"] = true;

  std::string cursor;
  for (int page = 0; page < MAX_PAGES; ++page) {
    std::string url = std::string(API_BASE) + "/tasks/filter?query=" + todoist::urlEncode(query) +
                      "&limit=" + std::to_string(PAGE_LIMIT);
    if (!cursor.empty()) url += "&cursor=" + todoist::urlEncode(cursor);

    std::string response;
    if (!HttpDownloader::fetchBearer(url, response, token)) {
      LOG_ERR("TODO", "tasks fetch failed");
      return false;
    }

    JsonDocument doc;
    const auto err = deserializeJson(doc, response, DeserializationOption::Filter(filter));
    if (err) {
      LOG_ERR("TODO", "tasks JSON parse: %s", err.c_str());
      return false;
    }

    JsonArrayConst results = doc["results"].as<JsonArrayConst>();
    out.reserve(out.size() + results.size());
    for (JsonObjectConst obj : results) {
      TodoistTask t;
      t.id = idToString(obj["id"]);
      t.content = obj["content"] | "";
      t.description = obj["description"] | "";
      t.priority = obj["priority"] | 1;
      t.projectId = idToString(obj["project_id"]);
      t.sectionId = idToString(obj["section_id"]);
      JsonObjectConst due = obj["due"];
      if (!due.isNull()) {
        t.dueDate = due["date"] | "";
        t.dueDatetime = due["datetime"] | "";
      }
      out.push_back(std::move(t));
    }

    const char* next = doc["next_cursor"];
    if (!next || !*next) break;
    cursor = next;
  }
  return true;
}

bool TodoistClient::fetchNameMap(const std::string& path, NameMap& out) {
  JsonDocument filter;
  filter["next_cursor"] = true;
  JsonObject el = filter["results"][0].to<JsonObject>();
  el["id"] = true;
  el["name"] = true;

  std::string cursor;
  for (int page = 0; page < MAX_PAGES; ++page) {
    std::string url = std::string(API_BASE) + path + "?limit=" + std::to_string(PAGE_LIMIT);
    if (!cursor.empty()) url += "&cursor=" + todoist::urlEncode(cursor);

    std::string response;
    if (!HttpDownloader::fetchBearer(url, response, token)) {
      LOG_ERR("TODO", "map fetch failed: %s", path.c_str());
      return false;
    }

    JsonDocument doc;
    const auto err = deserializeJson(doc, response, DeserializationOption::Filter(filter));
    if (err) {
      LOG_ERR("TODO", "map JSON parse: %s", err.c_str());
      return false;
    }

    JsonArrayConst results = doc["results"].as<JsonArrayConst>();
    out.reserve(out.size() + results.size());
    for (JsonObjectConst obj : results) {
      out.emplace_back(idToString(obj["id"]), std::string(obj["name"] | ""));
    }

    const char* next = doc["next_cursor"];
    if (!next || !*next) break;
    cursor = next;
  }
  return true;
}

bool TodoistClient::ensureMaps() {
  if (mapsLoaded) return true;
  if (!fetchNameMap("/projects", projects)) return false;
  if (!fetchNameMap("/sections", sections)) return false;
  mapsLoaded = true;
  return true;
}

void TodoistClient::resolveNames(std::vector<TodoistTask>& tasks) const {
  const auto find = [](const NameMap& m, const std::string& id) -> std::string {
    if (id.empty()) return {};
    for (const auto& entry : m) {
      if (entry.first == id) return entry.second;
    }
    return {};
  };
  for (auto& t : tasks) {
    t.projectName = find(projects, t.projectId);
    t.sectionName = find(sections, t.sectionId);
  }
}

bool TodoistClient::fetchTodayTasks(std::vector<TodoistTask>& out) {
  out.clear();
  if (!fetchTasksByQuery(todoist::buildTodayQuery(), out)) return false;
  if (!ensureMaps()) return false;
  resolveNames(out);
  todoist::sortTasks(out);
  return true;
}

bool TodoistClient::closeTask(const std::string& id) {
  if (id.empty()) {
    LOG_ERR("TODO", "closeTask: empty id");
    return false;
  }
  const std::string url = std::string(API_BASE) + "/tasks/" + id + "/close";
  return HttpDownloader::postBearer(url, token);
}

bool TodoistClient::fetchGoals(const std::string& label, std::vector<TodoistTask>& out) {
  out.clear();
  if (label.empty()) {
    LOG_ERR("TODO", "no goal label configured");
    return false;
  }
  if (!fetchTasksByQuery(todoist::buildGoalsQuery(label), out)) return false;
  if (!ensureMaps()) return false;
  resolveNames(out);
  todoist::sortTasks(out);
  return true;
}
