#pragma once

#include "data.hpp"
#include "utils.hpp"

std::string find_yt_dlp();
bool download_from_search(const std::string& query, int new_id = -1, size_t attempt_count = 0, bool bypass_lock = false, bool blocking = false);
MultistateFuture<AutocompleteResult> get_search_autocomplete(const std::string& query);
