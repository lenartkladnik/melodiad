#include <fstream>
#include <iostream>
#include <filesystem>
#include <future>
#include <print>

#include "include/data.hpp"
#include "include/storage_handler.hpp"
#include "include/utils.hpp"

#ifndef _WIN32
  #define STB_IMAGE_IMPLEMENTATION
  #define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#define STB_IMAGE_RESIZE_IMPLEMENTATION

// Disable warnings produced by external libs
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "../external/lib/httplib.h"
#include "../external/lib/nlohmann/json.hpp"

#pragma GCC diagnostic pop // Enable all warnings

using json = nlohmann::json;

void set_download_status(const std::string& s) {
  log(s);

  download_status = "{ \
    \"completed\": " + std::to_string(_download_progress_step) + ", \
    \"total\": " + std::to_string(_total_download_progress_step_count - 1) + ", \
    \"good\": true, \
    \"message\": \"" + s + "\" \
  }";
  _download_progress_step++;
}

void reset_download_status() {
  download_lock = false;
  _download_progress_step = 0;
  // Don't reset download_status since it still might be
  // used by the /download websocket and it will clear it
}

AutocompleteResult get_search_autocomplete_yt_music(const std::string& query) {
  if (query.size() < MIN_CHAR_COUNT_SEARCH) {
    return AutocompleteResult(query, {}, {});
  }

  std::string host = "https://music.youtube.com";
  std::string path = "/youtubei/v1/music/get_search_suggestions?prettyPrint=false";

  nlohmann::json body = {
    {"input", query},
    {"context", {{"client", {
      {"clientName", "WEB_REMIX"},
      {"clientVersion", "1.20240101.01.00"}
    }}}}
  };

  httplib::Client cli(host);
  cli.set_connection_timeout(MIN_AUTOCOMPLETE_RESPONSE_TIME);
  cli.set_read_timeout(MIN_AUTOCOMPLETE_RESPONSE_TIME);

  httplib::Headers headers = {
      {"Origin", "https://music.youtube.com"},
      {"X-Youtube-Client-Name", "67"},
      {"X-Youtube-Client-Version", "1.20240101.01.00"},
  };

  httplib::Result res;

  try {
    res = cli.Post(path, headers, body.dump(), "application/json");
  } catch (...) {
    log("Failed to perform a POST request for autocomplete.", ERROR);
    return AutocompleteResult(query, {}, {}); // Failed to get results fast enough or something went wrong in the request, return no suggestions
  }

  if (!res || res->status != 200) {
    log("Failed to fetch valid response for autocomplete (" + (res ? ("status code is " + std::to_string(res->status)) : "no response was given") + ").", ERROR);
    if (res)
      std::println("        Error body is:\n{}", res->body);

    return AutocompleteResult(query, {}, {}); // Something went wrong, return no suggestion
  }

  nlohmann::json root = nlohmann::json::parse(res->body);

  auto join_runs = [](const nlohmann::json& node) {
    std::string text;
    for (const auto& run : node.value("runs", nlohmann::json::array()))
        text += run.value("text", "");
    return text;
  };

  std::vector<std::string> suggestions;
  for (const auto& section : root.value("contents", nlohmann::json::array())) {
    auto renderers = section.value("searchSuggestionsSectionRenderer", nlohmann::json::object()).value("contents", nlohmann::json::array());
    for (const auto& item : renderers) {
      for (const char* key : {"searchSuggestionRenderer", "historySuggestionRenderer"}) {
        if (item.contains(key))
          suggestions.push_back(title_string(join_runs( // title_string makes every first letter of a word uppercase
            item[key].value("suggestion", nlohmann::json::object())
          )));
      }
    }
  }

  return AutocompleteResult(query, {}, suggestions);
}

AutocompleteResult get_search_autocomplete_ytdlp(const std::string& query) {
  if (query.size() < MIN_CHAR_COUNT_SEARCH) {
    return AutocompleteResult(query, {}, {});
  }

  if (yt_dlp_path.empty()) {
    log("Can't get search autocompletion results with YtDlp since it doesn't exist on the system.", ERROR);
    return AutocompleteResult(query, {}, {});
  }

  int n = 1; // Number of songs
  auto yt_dlp_search_args = " -I \"1:" + std::to_string(n) + "\" \"https://music.youtube.com/search?q=" + url_encode(query) + "\"" + " --no-playlist --skip-download ";
  auto primary_keys = "--print \"%(track)s\" --print \"%(artist)s\"";
  auto fallback_keys = "--print \"%(title)s\" --print \"%(uploader)s\"";

  std::string r = exec((yt_dlp_path + yt_dlp_search_args + primary_keys).c_str());

  if (r.find("NA") != std::string::npos) { // NA means key doesn't exist
    r = exec((yt_dlp_path + yt_dlp_search_args + fallback_keys).c_str());
  }

  std::vector<std::pair<std::string, std::string>> title_artist_pairs;
  std::vector<std::string> suggestions;

  std::pair<std::string, std::string> tmp_pair;

  bool is_ln_title = true; // First line is title
  for (const auto& ln : split_string(r, '\n')) {
    if (is_ln_title) {
      tmp_pair.first = ln;
    } else {
      tmp_pair.second = ln;

      title_artist_pairs.push_back(tmp_pair);
      suggestions.push_back(tmp_pair.first + " - " + tmp_pair.second);
    }

    is_ln_title = !is_ln_title; // Toggle
  }

  return AutocompleteResult(query, title_artist_pairs, suggestions);
}

bool _download_file(std::string url, const std::string& out_path, int max_redirects = 5) {
  for (int i = 0; i < max_redirects; i++) {
    auto path_start = url.find('/', url.find("://") + 3);
    std::string host = url.substr(0, path_start);
    std::string path = url.substr(path_start);

    httplib::Client cli(host);
    cli.set_follow_location(true);
    cli.set_connection_timeout(10);
    cli.set_read_timeout(30);

    auto res = cli.Get(path);

    if (!res) {
      // Request failed TODO: Add std::cerr
      return false;
    }

    if (res->status == 301 || res->status == 302 || res->status == 307 || res->status == 308) {
      auto new_location = res->get_header_value("location");
      url = new_location;
      continue;
    }

    if (res->status != 200) {
      // Failed to download
      return false;
    }

    std::ofstream out(out_path, std::ios::binary);
    if (!out) return false;
    out.write(res->body.data(), res->body.size());

    return true;
  }
  return false;
}

inline std::string get_yt_dlp_download_url() {
#ifdef _WIN32
    return "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
#elif __APPLE__
    return "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos";
#elif __linux__
    #ifdef __aarch64__
        return "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_linux_aarch64";
    #else
        return "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp";
    #endif
#endif
}

inline std::string get_yt_dlp_downloaded_path() {
#ifdef _WIN32
  return ".\\external\\programs\\yt-dlp.exe";
#else
  return "./external/programs/yt-dlp";
#endif
}

bool _resize_cover_art(const std::string& temp_file_path, const std::string& output, int target_w, int target_h) {
  set_download_status("Resizing cover art");
 
  if (!resize_image(temp_file_path, output, {(unsigned)target_w, (unsigned)target_h})) {
    return false;
  }
 
  log("Resized cover art image and saved to '" + output + "'");
 
  return true;
}

bool _yt_dlp_download_all(const std::string& query, int new_id) {
  const std::string base  = base_music_path_songs + std::to_string(new_id);
  const std::string thumb = base + ".cover_art.png.tmp";

  const std::string command = yt_dlp_path +
    " -I 1 \"https://music.youtube.com/search?q=" + url_encode(query) + "\""
    " --no-playlist -xciw -f \"bestaudio/best\" --audio-format mp3 --audio-quality 0"
    // Audio file
    " -o \"" + base + ".mp3\""
    // Cover art
    " --write-thumbnail --convert-thumbnails png"
    " --ppa \"ThumbnailsConvertor+FFmpeg_o:-vf crop=ih:ih\""
    " -o \"thumbnail:" + thumb + ".%(ext)s\""
    " -o \"pl_thumbnail:\""
    // Metadata
    " --print-to-file \"%(track,title)s\" \"" + base + ".title\""
    " --print-to-file \"%(artist,uploader)s\" \"" + base + ".artist\"";

  log("Calling system with '" + command + "'");

  if (system(command.c_str()) != 0) return false;

  if (!std::filesystem::exists(thumb + ".png")) return false;
  std::filesystem::rename(thumb + ".png", thumb);

  return true;
}

bool _postprocess_all(int new_id) {
  set_download_status("Resizing cover art");

  const std::string base = base_music_path_songs + std::to_string(new_id);
  const std::string thumb = base + ".cover_art.png.tmp";

  if (!_resize_cover_art(thumb, base + ".png", 1000, 1000)) return false;
  if (!_resize_cover_art(thumb, base + ".small.png", 100, 100)) return false;

  std::remove(thumb.c_str());
  log("Removed temporary file " + thumb);
  return true;
}

bool _download_song_from_query(const std::string& query, size_t new_id) {
  download_lock = true;

  log("Attempting to download song from query '" + query + "'");

  set_download_status("Downloading song file, metadata and cover art");
  if (!_yt_dlp_download_all(query, new_id)) return false;
  if (!_postprocess_all(new_id)) return false;

  log("Downloaded song for query '" + query + "'");
  return true;
}

std::string find_yt_dlp() {
  log("Checking if YtDlp exists on the system");

  std::string yt_dlp_path = "yt-dlp";
  if (!std::filesystem::exists(get_yt_dlp_downloaded_path()) &&
      !std::system("yt-dlp --version >"
    #ifdef _WIN32
      "nul 2>nul"
    #else
      "/dev/null 2>&1"
    #endif
    ) == 0) {
    log("YtDlp was not found on the system and will be downloaded");

    yt_dlp_path = get_yt_dlp_downloaded_path();
    if (!_download_file(get_yt_dlp_download_url(), yt_dlp_path)) {
      throw std::runtime_error("Failed to download the YtDlp binary form '" + yt_dlp_path + "'. Consider installing YtDlp yourself systemwide");
    }
    #ifndef _WIN32
    // On POSIX like systems also chmod +x the file
    system(("chmod +x " + yt_dlp_path).c_str());
    #endif
  } else {
    log("YtDlp was found on the system");
  }

  return yt_dlp_path;
}

bool download_from_search(const std::string& query, int new_id, size_t attempt_count, bool bypass_lock, bool blocking) {
  if (attempt_count > MAX_DOWNLOAD_ATTEMPTS) {
    reset_download_status();
    download_status = "{ \
      \"completed\": 0, \
      \"total\": 0, \
      \"good\": false, \
      \"message\": \"Failed to download '" + query + "'\" \
    }";
    return false;
  }

  if (!bypass_lock && download_lock) {
    log("Download lock is active, will not download");
    reset_download_status();
    download_status = "{ \
      \"completed\": 0, \
      \"total\": 0, \
      \"good\": false, \
      \"message\": \"To start another download, you must wait for the current one to finish.\" \
    }";
    return false; // Exit if a download is ongoing
  }

  download_lock = true;

  if (yt_dlp_path.empty()) {
    yt_dlp_path = find_yt_dlp();

    if (yt_dlp_path.empty()) {
      log("Can't download since YtDlp doesn't exist on the system", ERROR);
      reset_download_status();
      download_status = "{ \
        \"completed\": 0, \
        \"total\": 0, \
        \"good\": false, \
        \"message\": \"YtDlp can't be found or downloaded, consider installing it manually.\" \
      }";
      return false;
    }
  }

  set_download_status("Getting song name and title for query '" + query + "'");

  std::thread download_thread([=](){
    bool success;
    int song_id = new_id == -1 ? get_next_available_song_id() : new_id;

    success = _download_song_from_query(query, song_id);

    if (!success) {
      log("Failed to download '" + query + "'", ERROR);
      remove_song(song_id);
      download_status = "{ \
        \"completed\": 0, \
        \"total\": 0, \
        \"good\": false, \
        \"message\": \"Download failed, trying again.\" \
      }";

      // Retry the download
      reset_download_status();
      download_from_search(query, new_id, attempt_count + 1, bypass_lock, blocking);
      return;
    } else {
      set_download_status("Successfully downloaded '" + query + "'");
    }

    reset_download_status();
  });

  if (blocking)
    download_thread.join();
  else
    download_thread.detach();

  return true;
}

MultistateFuture<AutocompleteResult> get_search_autocomplete(const std::string& query) {
  MultistateFuture<AutocompleteResult> mf;
  mf.launch([query](){return get_search_autocomplete_yt_music(query);}, 1);
  mf.launch([query](){return get_search_autocomplete_ytdlp(query);}, 2);

  return mf;
}
