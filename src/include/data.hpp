#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <future>

#include "utils.hpp"

extern std::string yt_dlp_path;

extern const size_t MAX_PAST_QUEUE_SIZE;
extern const size_t NO_REPEAT_ZONE; // A song won't repeat for at least this many songs (if the number of songs in the playlist is more than this value)
extern const std::chrono::milliseconds MIN_AUTOCOMPLETE_RESPONSE_TIME; // If autocomplete API takes longer then this an empty list is returned
extern const size_t MIN_CHAR_COUNT_SEARCH;
extern const int MIN_MATCH_DIFF;
extern const size_t MAX_DOWNLOAD_ATTEMPTS; // Max amount of tries to redownload
extern const size_t MAX_THREAD_COUNT;
extern const size_t _total_download_progress_step_count; // Should store the number of calls to '_download_progres_step++' and by extension 'set_download_status()'

extern bool download_lock;
extern std::string download_status;
extern size_t _download_progress_step;
extern std::atomic<uint64_t> ac_generation;

struct AutocompleteResult {
  std::string query;
  std::vector<std::pair<std::string, std::string>> pairs;
  std::vector<std::string> results;
};
