#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <future>
#include <chrono>
#include <print>

// Disable warnings produced by external libs
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "../../external/lib/httplib.h"

#pragma GCC diagnostic pop // Enable all warnings

#include "../../external/lib/BS_thread_pool.hpp"

template<typename TIterable>
void debug_print_iterable(TIterable& iterable, std::string sep = ", ") {
  std::print("{}", "{");
  for (const auto& i : iterable) {
    std::print("\"{}\"{}", i, sep);
  }
  std::print("\x1b[{}D", sep.size());
  std::println("{}", "}");
}

enum LogType {
  INFO,
  WARN,
  ERROR,
  REQUEST,
};

void log(const std::string msg, const LogType type = INFO);
void log_request(const httplib::Request& req);
std::string exec(const char* cmd);
bool resize_image(std::string path, std::string output, sf::Vector2u target_size);
bool is_string_valid_name(const std::string& s);
void mkdir(std::string path);
bool must_exist(std::string path);
std::string get_stem(std::string path);
std::string insert_in_stem(std::string path, std::string s);
std::string get_next_available_path(std::string path);
float matching_song(std::string query, std::string song_title, std::string song_artist, size_t threshold);
std::string escape_csv(const std::string& s);
std::string title_string(const std::string& s);
std::string url_encode(const std::string &value);
void cleanup();

template<typename TChar>
std::vector<std::basic_string<TChar>> split_string(const std::basic_string<TChar>& s, TChar delim) {
  std::vector<std::basic_string<TChar>> result;
  if (s.empty()) return result;
  size_t from = 0;
  while (1) {
    size_t pos = s.find(delim, from);
    if (pos == std::string::npos) {
      if (from < s.size()) {
        result.emplace_back(s.substr(from));
      }
      break;
    }
    auto subs = s.substr(from, pos - from);
    if (!subs.empty())
      result.emplace_back(subs);
    from = pos + 1;
  }
  return result;
}

template<typename T>
bool is_future_ready(const std::future<T>& f) {
  return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

inline BS::thread_pool<> multistate_future_pool;

template<typename T>
class MultistateFuture {
  public:
    std::vector<std::future<T>> futures;
    std::vector<size_t> desires;

    template<typename TFunc>
    void launch(TFunc func, size_t desire) {
      futures.emplace_back(multistate_future_pool.submit_task(std::move(func)));
      desires.push_back(desire);
    }

    std::optional<T> get() {
      size_t i = 0;
      int max_desire = -1;

      std::optional<T> result;

      for (auto& future : futures) {
        if (is_future_ready(future)) {
          if ((int)desires[i] > max_desire) {
            max_desire = (int)desires[i];
            result = future.get();
          }
        }

        i++;
      }

      return result;
    }

    void wait_for(size_t min_desire) {
      while (1) {
        bool all_ready = true;

        size_t i = 0;
        for (const auto& future : futures) {
          if (is_future_ready(future)) {
            if (desires[i] >= min_desire)
              break;
          } else {
            all_ready = false;
          }

          i++;
        }

        if (all_ready) // Even if min_desire wasn't hit exit when all the futures complete
          break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    void wait() {
      while (1) {
        bool all_ready = true;

        for (const auto& future : futures) {
          if (!is_future_ready(future))
            all_ready = false;
        }

        if (all_ready)
          break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }

    bool is_done(int desire = -1) {
      // -1 desire means wait for all to finish
      int i = 0;
      for (const auto& future : futures) {
        if (desire < 0 && !is_future_ready(future))
          return false;

        if (is_future_ready(future) && (int)desires[i] >= desire)
          return true;

        i++;
      }

      if (desire < 0)
        return true;

      return false;
    }
};
