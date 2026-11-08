/*
* data.cpp
*
* Copyright (C) 2026  Lenart Kladnik
* You may use, distribute and modify this code under the terms of the GPL license,
* see LICENSE file.
*
* For version information, see main.cpp
*/

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cctype>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <array>
#include <unordered_map>
#include <chrono>
#include <thread>

#include "include/utils.hpp"
#include "include/storage_handler.hpp"
#include "include/data.hpp"

std::string yt_dlp_path;

const size_t MAX_PAST_QUEUE_SIZE = 999;
const size_t NO_REPEAT_ZONE = 3;
const std::chrono::milliseconds MIN_AUTOCOMPLETE_RESPONSE_TIME = std::chrono::milliseconds(500);
const size_t MIN_CHAR_COUNT_SEARCH = 3;
const int MIN_MATCH_DIFF = 2;
const size_t MAX_DOWNLOAD_ATTEMPTS = 3;
const size_t _hardware_concurrency = std::thread::hardware_concurrency();
const size_t MAX_THREAD_COUNT = _hardware_concurrency != 0 ? _hardware_concurrency : 4;
const size_t _total_download_progress_step_count = 5;

bool download_lock = false;
std::string download_status;
size_t _download_progress_step = 0;
std::atomic<uint64_t> ac_generation{0};
