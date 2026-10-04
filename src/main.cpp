#include <print>
#include <string>
#include <vector>
#include <atomic>
#include <future>
#include <cmath>

#include "include/download.hpp"
#include "include/storage_handler.hpp"

// Disable warnings produced by external libs
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "../external/lib/httplib.h"
#include "../external/lib/nlohmann/json.hpp"
#include "../external/lib/atomic_queue/atomic_queue.h"

#pragma GCC diagnostic pop // Enable all warnings

#define VERSION "2.2.1-dev"

void help() {
  std::println("melodiad (v{}) - Melodia music player backend", VERSION);
  std::println("");
  std::println("Usage:  melodiad [options]");
  std::println("");
  std::println("melodiad is a music server backend with an API built on top of httplib that");
  std::println("supports searching, downloading (via yt-dlp), and managing songs and playlists");
  std::println("");
  std::println("For documentation on the API please refer to API.md (in the git repository).");
  std::println("");
  std::println("Options:");
  std::println("  -h, --help  Shows this screen");
  std::println("  -P, --port  Port the API is listening on (default is 5400)");
  std::println("  -H, --host  Host the API is listening on (default is 0.0.0.0)");
}

int main(int argc, char *argv[]) {
  std::string host = "0.0.0.0";
  int port = 5400;

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
        std::println("[ERROR] Missing value for argument host.");
        return 1;
      }
    }

    else if (is_args({"-P", "--port"}, argv[i])) {
      if (i + 1 < argc) {
        try {
          port = std::stoi(argv[i + 1]);
        } catch (std::invalid_argument&) {
          std::println("[ERROR] Argument port must be an integer.");
          return 1;
        }

        i++;
      } else {
        std::println("[ERROR] Missing value for argument port.");
        return 1;
      }
    }

    else {
      std::println("[ERROR] Unknown argument '{}', try --help to see the list of arguments.", argv[i]);
      return 1;
    }
  }

  std::println("[INFO] Setting up environment...");

  if (!ensure_storage())
    return 1;

  yt_dlp_path = find_yt_dlp(); // find yt-dlp on PATH and download if isn't found

  httplib::Server server;

  server.set_mount_point("/data", base_music_path);

  server.Get("/", [](const httplib::Request&, httplib::Response& res) {
    res.set_content("Server running Melodia", "text/plain");
  });

  // Get search results
  server.Get("/search", [](const httplib::Request& req, httplib::Response& res){
    if (!req.has_param("query")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto query = req.get_param_value("query");

    nlohmann::json json_response = search_all_songs(query);

    res.set_content(json_response.dump(), "application/json");
  });

  // Get song
  server.Get("/get-song", [](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("song_id")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto song_id = req.get_param_value("song_id");

    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::invalid_argument&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
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

  // Get playlist
  server.Get("/get-playlist", [](const httplib::Request& req, httplib::Response& res) {
    if (!req.has_param("playlist_name")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");

    if (!exists_playlist(playlist_name)) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    nlohmann::json json_response;
    json_response["songs"] = get_playlist(playlist_name);
    json_response["image"] = "/data/playlists/" + url_encode(playlist_name) + ".png";

    res.set_content(json_response.dump(), "application/json");
  });

  // Get all playlists
  server.Get("/get-all-playlists", [](const httplib::Request&, httplib::Response& res) {
    nlohmann::json json_response = get_all_playlists();

    res.set_content(json_response.dump(), "application/json");
  });

  // Download song
  server.WebSocket("/download", [](const httplib::Request& req, httplib::ws::WebSocket& ws){
    if (!req.has_param("query")) {
      ws.close(httplib::ws::CloseStatus::InvalidPayload, "Bad request");
      return;
    }

    auto query = req.get_param_value("query");

    bool has_started = download_from_search(query);

    if (!has_started) {
      ws.close(httplib::ws::CloseStatus::NoStatus, download_status);
      return;
    }

    // Send status updates while a download is ongoing
    std::string last_status;
    while (download_lock) {
      if (last_status != download_status)
        ws.send(download_status);

      last_status = download_status;

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // One last check
    if (last_status != download_status)
      ws.send(download_status);

    download_status = ""; // Clear download_status here since reset_download_status
                          // can't because this websocket might still use it

    ws.close();
  });

  // Download a list of songs
  server.WebSocket("/download-list", [](const httplib::Request& req, httplib::ws::WebSocket& ws) {
    if (!req.has_param("list")) {
      ws.close(httplib::ws::CloseStatus::InvalidPayload, "Bad request");
      return;
    }

    auto list = req.get_param_value("list");

    std::vector<std::string> songs;
    try {
      nlohmann::json::parse(list).get_to(songs);
    } catch(...) {
      ws.close(httplib::ws::CloseStatus::InvalidPayload, "Malformed request");
      return;
    }

    std::atomic<size_t> processed = 0;

    std::vector<std::future<void>> download_futures;
    download_futures.reserve(MAX_THREAD_COUNT);

    int first_id = get_next_available_song_id();

    auto remove_finished_futures = [](std::vector<std::future<void>>& futures){
      futures.erase(std::remove_if(futures.begin(), futures.end(),
        [](const std::future<void>& fut){return fut.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;}
      ), futures.end());
    };

    size_t index = 0;
    for (const auto& song : songs) {
      while (download_futures.size() >= MAX_THREAD_COUNT) {
        // Remove futures that have finished
        remove_finished_futures(download_futures);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      download_futures.emplace_back(std::async(std::launch::async, [&, index](){
        download_from_search(song, first_id + index, 0, true, true);
        processed++;

        ws.send("{ \
          \"completed\": " + std::to_string(processed) + ", \
          \"total\": " + std::to_string(songs.size()) + " \
        }");
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
  server.Get("/create-playlist", [](const httplib::Request& req, httplib::Response& res){
    if (!req.has_param("song_id")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto song_id = req.get_param_value("song_id");
    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::invalid_argument&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    std::string name = create_new_playlist(int_song_id);

    res.set_content(name, "plain/text");
  });

  // Rename playlist
  server.Get("/rename-playlist", [](const httplib::Request& req, httplib::Response& res){
    if (!(req.has_param("old_name") && req.has_param("new_name"))) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto old_name = req.get_param_value("old_name");
    auto new_name = req.get_param_value("new_name");

    std::string name = rename_playlist(old_name, new_name);

    res.set_content(name, "plain/text");
  });

  // Add song to playlist
  server.Get("/playlist-add-item", [](const httplib::Request& req, httplib::Response& res){
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
    } catch (const std::invalid_argument&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    add_to_playlist(playlist_name, int_song_id);

    res.set_content("Ok", "plain/text");
  });

  // Remove song from playlist
  server.Get("/playlist-remove-item", [](const httplib::Request& req, httplib::Response& res){
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
    } catch (const std::invalid_argument&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    remove_from_playlist(playlist_name, int_song_id);

    res.set_content("Ok", "plain/text");
  });

  // Delete song
  server.Get("/delete-song", [](const httplib::Request& req, httplib::Response& res){
    if (!req.has_param("song_id")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto song_id = req.get_param_value("song_id");
    int int_song_id;
    try {
        int_song_id = std::stoi(song_id);
    } catch (const std::invalid_argument&) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    remove_song(int_song_id);

    res.set_content("Ok", "plain/text");
  });

  // Delete playlist
  server.Get("/delete-playlist", [](const httplib::Request& req, httplib::Response& res){
    if (!req.has_param("playlist_name")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto playlist_name = req.get_param_value("playlist_name");

    remove_playlist(playlist_name);

    res.set_content("Ok", "plain/text");
  });

  // Autocomplete search
  server.Get("/fast-autocomplete", [](const httplib::Request& req, httplib::Response& res){
    if (!req.has_param("query")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto query = req.get_param_value("query");

    auto mf = get_search_autocomplete(query);
    mf.wait_for(0);
    auto autocomplete_result = mf.get();

    if (!autocomplete_result) {
      res.status = 500;
      res.set_content("Failed to get autocomplete result (empty result).", "text/plain");
      return;
    }

    nlohmann::json json_response = autocomplete_result->results;

    res.set_content(json_response.dump(), "application/json");
  });

  server.Get("/slow-autocomplete", [](const httplib::Request& req, httplib::Response& res){
    if (!req.has_param("query")) {
      res.status = 400;
      res.set_content("Bad request", "text/plain");
      return;
    }

    auto query = req.get_param_value("query");

    auto mf = get_search_autocomplete(query);
    mf.wait();
    auto autocomplete_result = mf.get();

    if (!autocomplete_result) {
      res.status = 500;
      res.set_content("Failed to get autocomplete result (empty result).", "text/plain");
      return;
    }

    nlohmann::json json_response = autocomplete_result->results;

    res.set_content(json_response.dump(), "application/json");
  });

  // Update yt-dlp
  server.Get("/update-yt-dlp", [](const httplib::Request&, httplib::Response& res){
    if (yt_dlp_path.empty()) {
      res.status = 500;
      res.set_content("YtDlp doesn't exist on the system.", "text/plain");
      return;
    }

    std::string command = yt_dlp_path + " -U";
    
    if (!system(command.c_str()) == 0) {
      res.status = 500;
      res.set_content("YtDlp update command failed ('" + command + "').", "text/plain");
      return;
    }

    res.set_content("Ok", "text/plain");
  });

  std::println("[INFO] Web server running on {}:{}", host, port);

  server.listen(host, port);

  return 0;
}
