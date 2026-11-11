/*
* main.cpp
*
* Copyright (C) 2026  Lenart Kladnik
* You may use, distribute and modify this code under the terms of the GPL license,
* see LICENSE file.
*
* version 1.0.0, 8 Nov 2026
*/

#include <print>
#include <string>
#include <vector>
#include <atomic>
#include <future>
#include <cmath>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <random>

#include "include/download.hpp"
#include "include/storage_handler.hpp"
#include "include/utils.hpp"

// Disable warnings produced by external libs
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "../external/lib/httplib.h"
#include "../external/lib/nlohmann/json.hpp"
#include "../external/lib/atomic_queue/atomic_queue.h"

#pragma GCC diagnostic pop // Enable all warnings

#define VERSION "1.0.0"

void help() {
  std::println("melodiad (v{}) - Melodia music player backend", VERSION);
  std::println("");
  std::println("Usage:  melodiad [options]");
  std::println("");
  std::println("melodiad is a music server backend with an API built on top of httplib that");
  std::println("supports searching, downloading (via yt-dlp), and managing songs and playlists");
  std::println("");
  std::println("For documentation on the API please refer to API.md (in the root of the git repository).");
  std::println("");
  std::println("Options:");
  std::println("  -h, --help  Shows this screen");
  std::println("  -p, --path  Path where the songs and related data are stored");
  std::println("  -P, --port  Port the API is listening on (default is 5400)");
  std::println("  -H, --host  Host the API is listening on (default is 0.0.0.0)");
}

void start_message() {
  std::println("melodiad  Copyright (C) 2026  Lenart Kladnik");
  std::println("This program comes with ABSOLUTELY NO WARRANTY.");
  std::println("This is free software, and you are welcome to redistribute it");
  std::println("under certain conditions.");
  std::println("");
}

int main(int argc, char *argv[]) {
  std::string host = "0.0.0.0";
  int port = 5400;
  std::string path = "./";

  auto is_args = [](const std::vector<std::string>& v, char* s) -> bool {
    for (const auto& e : v) { if (e.compare(s) == 0) return true; }
    return false;
  };

  for (int i = 1; i < argc; i++) {
    if (is_args({"-h", "--help"}, argv[i])) {
      help();
      return 0;
    }

    else if (is_args({"-v", "--version"}, argv[i])) {
      std::println(VERSION);
      return 0;
    }

    else if (is_args({"-H", "--host"}, argv[i])) {
      if (i + 1 < argc) {
        host = argv[i + 1];

        i++;
      } else {
        log("Missing value for argument host.", ERROR);
        return 1;
      }
    }

    else if (is_args({"-P", "--port"}, argv[i])) {
      if (i + 1 < argc) {
        try {
          port = std::stoi(argv[i + 1]);
        } catch (std::exception&) {
          log("Argument port must be an integer.", ERROR);
          return 1;
        }

        i++;
      } else {
        log("Missing value for argument port.", ERROR);
        return 1;
      }
    }

    else if (is_args({"-p", "--path"}, argv[i])) {
      if (i + 1 < argc) {
        path = argv[i + 1];

        i++;
      } else {
        log("Missing value for argument path.", ERROR);
        return 1;
      }
    }

    else {
      log("Unknown argument '" + std::string(argv[i]) + "', try --help to see the list of arguments.", ERROR);
      return 1;
    }
  }

  start_message();

  log("Setting up environment...");

  if (!ensure_storage(path)) {
    log("Failed to initialize storage at '" + path + "'.", ERROR);
    return 1;
  }

  yt_dlp_path = find_yt_dlp(); // find yt-dlp on PATH and download if isn't found

  httplib::Server server;
  server.new_task_queue = []{return new httplib::ThreadPool(32);};

  server.set_mount_point("/data", base_music_path);

  server.Get("/api/status", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    res.set_content("Running.", "text/plain");
  });

  // Get search results
  server.Get("/api/search", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    // default output
    res.set_content("{}", "application/json");

    if (!req.has_param("query")) {
      res.status = 400;
      return;
    }

    auto query = req.get_param_value("query");

    nlohmann::json json_response = search_all_songs(query);

    res.set_content(json_response.dump(), "application/json");
  });

  // Get song
  server.Get("/api/song", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    // default output
    res.set_content("{}", "application/json");

    if (!req.has_param("song_id")) {
      res.status = 400;
      return;
    }

    auto song_id = req.get_param_value("song_id");

    if (song_id == "-1") { // Default "null" song id
      res.status = 404;
      return;
    }

    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::exception&) {
      res.status = 400;
      log("song_id is not an integer.", ERROR);
      return;
    }

    if (!exists_song(int_song_id)) {
      res.status = 404;
      return;
    }

    nlohmann::json json_response;
    json_response["title"] = get_song_title(int_song_id);
    json_response["artist"] = get_song_artist(int_song_id);
    json_response["base"] = "/data/songs/" + song_id;
    json_response["audio"] = "/data/songs/" + song_id + ".mp3";
    json_response["image"] = "/data/songs/" + song_id + ".png";
    json_response["thumbnail"] = "/data/songs/" + song_id + ".small.png";

    res.set_content(json_response.dump(), "application/json");
  });

  // Get song audio
  server.Get("/api/song/stream", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    // default output
    res.set_content("", "audio/mpeg");

    if (!req.has_param("song_id")) {
      res.status = 400;
      return;
    }

    auto song_id = req.get_param_value("song_id");

    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::exception&) {
      res.status = 400;
      log("song_id is not a valid integer.", ERROR);
      return;
    }

    res.set_file_content(construct_song_path(int_song_id) + ".mp3", "audio/mpeg");
  });

  // Get playlist
  server.Get("/api/playlist", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    // default output
    res.set_content("{}", "application/json");

    if (!req.has_param("playlist_name")) {
      res.status = 400;
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");

    if (!exists_playlist(playlist_name)) {
      res.status = 400;
      log("Requested playlist doesn't exist.", ERROR);
      return;
    }

    nlohmann::json json_response;
    json_response["image"] = "/data/playlists/" + url_encode(playlist_name) + ".png";

    res.set_content(json_response.dump(), "application/json");
  });

  // Get playlist size
  server.Get("/api/playlist/size", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    // default output
    res.set_content("{}", "application/json");

    if (!req.has_param("playlist_name")) {
      res.status = 400;
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");

    if (!exists_playlist(playlist_name)) {
      res.status = 400;
      log("Requested playlist doesn't exist.", ERROR);
      return;
    }

    nlohmann::json json_response;
    json_response["size"] = get_playlist(playlist_name).size();

    res.set_content(json_response.dump(), "application/json");
  });


  // Get playlist items
  server.Get("/api/playlist/items", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    res.set_content("{}", "application/json");

    if (!req.has_param("playlist_name")) {
      res.status = 400;
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");

    if (!exists_playlist(playlist_name)) {
      res.status = 400;
      log("Requested items for a playlist that doesn't exist.", ERROR);
      return;
    }

    nlohmann::json json_response;
    json_response["songs"] = get_playlist(playlist_name);

    res.set_content(json_response.dump(), "application/json");
  });

  // Get song for index in playlist
  server.Get("/api/playlist/next", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    res.set_content("{}", "application/json");

    if (!(req.has_param("playlist_name") && req.has_param("index"))) {
      res.status = 400;
      return;
    }

    bool shuffle = false;
    int seed;
    if (req.has_param("shuffle")) {
      if (!req.has_param("seed")) {
        res.status = 400;
        return;
      }
      try {
        seed = std::stoi(req.get_param_value("seed"));
      } catch (const std::exception&) {
        res.status = 400;
        return;
      }
      shuffle = req.get_param_value("shuffle") == "true";
    }
    bool loop = req.get_param_value("loop") == "true";
    auto playlist_name = req.get_param_value("playlist_name");
    int index;
    try {
      index = std::stoi(req.get_param_value("index"));
    } catch (const std::exception&) {
      res.status = 400;
      return;
    }

    if (index < 0) {
      res.status = 400;
      return;
    }

    auto songs = get_playlist(playlist_name);

    if (songs.size() == 0) {
      return;
    }

    if (shuffle) {
      std::mt19937 rng(seed);
      std::shuffle(songs.begin(), songs.end(), rng);
    }

    int next_song = songs[index % songs.size()];
    if (!loop) {
      if (index >= (int)songs.size()) {
        next_song = -1;
      }
    }

    nlohmann::json json_response;
    json_response["song_id"] = next_song;

    res.set_content(json_response.dump(), "application/json");
  });

  // Get all playlists
  server.Get("/api/playlists", [](const httplib::Request& req, httplib::Response& res) {
    log_request(req);
    nlohmann::json json_response = get_all_playlists();

    res.set_content(json_response.dump(), "application/json");
  });

  // Download song(s)
  server.WebSocket("/api/download", [](const httplib::Request& req, httplib::ws::WebSocket& ws) {
    log_request(req);
    std::vector<std::future<void>> download_futures;
    download_futures.reserve(MAX_THREAD_COUNT);

    int first_id = get_next_available_song_id();

    auto remove_finished_futures = [](std::vector<std::future<void>>& futures){
      futures.erase(std::remove_if(futures.begin(), futures.end(),
        [](const std::future<void>& fut){return fut.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;}
      ), futures.end());
    };

    std::string song;
    size_t index = 0;
    while (ws.read(song)) {
      if (song.empty()) // Last message should be empty string
        break;

      log("Downloading song '" + song + "'.");

      while (download_futures.size() >= MAX_THREAD_COUNT) {
        // Remove futures that have finished
        remove_finished_futures(download_futures);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      download_futures.emplace_back(std::async(std::launch::async, [&, song, index](){
        auto status = download_from_search(song, first_id + index, 0, true, true);

        nlohmann::json json_response;
        json_response["id"] = first_id + index;
        json_response["success"] = status;

        ws.send(json_response.dump());
      }));
      index++;
    }

    // Exhaust download_futures
    while (download_futures.size() > 0) {
      remove_finished_futures(download_futures);
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    ws.close();
  });

  // Create playlist
  server.Get("/api/playlist/new", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    if (!req.has_param("song_id")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto song_id = req.get_param_value("song_id");
    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::exception&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    std::string name = create_new_playlist(int_song_id);

    res.set_content(name, "text/plain");
  });

  // Rename playlist
  server.Get("/api/playlist/rename", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    if (!(req.has_param("old_name") && req.has_param("new_name"))) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto old_name = req.get_param_value("old_name");
    auto new_name = req.get_param_value("new_name");

    std::string name = rename_playlist(old_name, new_name);

    res.set_content(name, "text/plain");
  });

  // Add song to playlist
  server.Get("/api/playlist/add", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    if (!(req.has_param("playlist_name") && req.has_param("song_id"))) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");
    auto song_id = req.get_param_value("song_id");
    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::exception&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    add_to_playlist(playlist_name, int_song_id);

    res.set_content("Ok", "text/plain");
  });

  // Remove song from playlist
  server.Get("/api/playlist/remove", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    if (!(req.has_param("playlist_name") && req.has_param("song_id"))) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");
    auto song_id = req.get_param_value("song_id");
    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::exception&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    remove_from_playlist(playlist_name, int_song_id);

    res.set_content("Ok", "text/plain");
  });

  // Delete song
  server.Get("/api/song/delete", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    if (!req.has_param("song_id")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto song_id = req.get_param_value("song_id");
    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::exception&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    remove_song(int_song_id);

    res.set_content("Ok", "text/plain");
  });

  // Delete playlist
  server.Get("/api/playlist/delete", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    if (!req.has_param("playlist_name")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");

    remove_playlist(playlist_name);

    res.set_content("Ok", "text/plain");
  });

  // Autocomplete search
  server.Get("/api/autocomplete/fast", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    // default output
    res.set_content("{}", "application/json");

    if (!req.has_param("query")) {
      res.status = 400;
      return;
    }

    const uint64_t my_gen = ++ac_generation;

    auto query = req.get_param_value("query");

    // check if stale
    if (ac_generation.load() != my_gen) {
      res.status = 409;
      return;
    }

    auto mf = get_search_autocomplete(query);
    while (!mf.is_done(0)) {
      // check if stale
      if (ac_generation.load() != my_gen) {
        res.status = 409;
        return;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    auto autocomplete_result = mf.get();

    if (!autocomplete_result) {
      res.status = 500;
      std::println("Failed to get autocomplete result (empty result).");
      return;
    }

    nlohmann::json json_response = autocomplete_result->results;

    res.set_content(json_response.dump(), "application/json");
  });

  server.Get("/api/autocomplete/slow", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    // default output
    res.set_content("{}", "application/json");

    if (!req.has_param("query")) {
      res.status = 400;
      return;
    }

    const uint64_t my_gen = ++ac_generation;

    auto query = req.get_param_value("query");

    // check if stale
    if (ac_generation.load() != my_gen) {
      res.status = 409;
      return;
    }

    auto mf = get_search_autocomplete(query);
    while (!mf.is_done()) {
      // check if stale
      if (ac_generation.load() != my_gen) {
        res.status = 409;
        return;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    auto autocomplete_result = mf.get();

    if (!autocomplete_result) {
      res.status = 500;
      log("Failed to get autocomplete result (empty result).", ERROR);
      return;
    }

    nlohmann::json json_response = autocomplete_result->results;

    res.set_content(json_response.dump(), "application/json");
  });

  // Update yt-dlp
  server.Get("/api/update", [](const httplib::Request& req, httplib::Response& res){
    log_request(req);
    if (yt_dlp_path.empty()) {
      res.status = 500;
      log("YtDlp doesn't exist on the system.", ERROR);
      return;
    }

    std::string command = yt_dlp_path + " -U";

    if (system(command.c_str()) != 0) {
      res.status = 500;
      log("YtDlp update command failed ('" + command + "').", ERROR);
      res.set_content("Failed", "text/plain");
      return;
    }

    res.set_content("Ok", "text/plain");
  });

  log("Web server running on " + host + ":" + std::to_string(port));

  server.listen(host, port);

  return 0;
}
