#include <string>
#include <cctype>
#include <print>

#include "include/data.hpp"
#include "include/storage_handler.hpp"

#ifndef _WIN32
  #define STB_IMAGE_IMPLEMENTATION
  #define STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#define STB_IMAGE_RESIZE_IMPLEMENTATION

// Disable warnings produced by external libs
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include "../external/lib/stb/stb_image.h"
#include "../external/lib/stb/stb_image_resize2.h"
#include "../external/lib/stb/stb_image_write.h"

#pragma GCC diagnostic pop // Enable all warnings

#ifdef _WIN32
  #define POPEN _popen
  #define PCLOSE _pclose
#else
  #define POPEN popen
  #define PCLOSE pclose
#endif

void log(const std::string msg, const LogType type) {
  std::string log_string = "";

  switch (type) {
    case INFO:
      log_string += "info";
      break;
    case WARN:
      log_string += "warning";
      break;
    case ERROR:
      log_string += "error";
      break;
    case REQUEST:
      log_string += "request";
    default:
      break;
  }

  log_string += " [";

  std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::tm tm{};
  #ifdef _WIN32
    localtime_s(&tm, &now);
  #else
    localtime_r(&now, &tm);
  #endif
  char time_buf[64];
  std::strftime(time_buf, sizeof(time_buf), "%d/%b/%Y:%H:%M:%S %z", &tm);

  log_string += time_buf;

  log_string += "] ";
  log_string += msg;

  std::println("{}", log_string);
}

void log_request(const httplib::Request& req) {
  log(req.method + req.path + " from " + req.remote_addr + ":" + std::to_string(req.remote_port), REQUEST);

  auto user_agent = req.get_header_value("User-Agent");
  if (!user_agent.empty())
    log("  User-Agent: " + user_agent, REQUEST);

  for (const auto& [key, value] : req.params)
    log("  param " + key + " = '" + value + "'", REQUEST);
}

// Source - https://stackoverflow.com/a/478960
// Posted by waqas, modified by community. See post 'Timeline' for change history
// Retrieved 2026-03-28, License - CC BY-SA 4.0
std::string exec(const char* cmd) {
  std::println("[Info] Executing '{}'.", cmd);

  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, int(*)(FILE*)> pipe(POPEN(cmd, "r"), PCLOSE);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

// ==============================================================================

struct StbiFree { void operator()(unsigned char* p) const { stbi_image_free(p); } };

bool resize_image(std::string path, std::string output, sf::Vector2u target_size) {
  int w, h, channels;
  std::unique_ptr<unsigned char, StbiFree> data(
      stbi_load(path.c_str(), &w, &h, &channels, 0));

  if (!data)
    throw std::runtime_error("Failed to decode image from " + path + ": " + stbi_failure_reason());

  stbir_pixel_layout layout;
  switch (channels) {
    case 1: layout = STBIR_1CHANNEL; break;
    case 2: layout = STBIR_2CHANNEL; break;
    case 3: layout = STBIR_RGB; break;
    case 4: layout = STBIR_RGBA; break;
    default:
      throw std::runtime_error("Unsupported channel count for cover art image.");
  }

  std::vector<unsigned char> resized(target_size.x * target_size.y * channels);

  stbir_resize(
    data.get(), w, h, 0,
    resized.data(), target_size.x, target_size.y, 0,
    layout, STBIR_TYPE_UINT8, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);

  if (!stbi_write_png(output.c_str(), target_size.x, target_size.y, channels,
                      resized.data(), target_size.x * channels))
    throw std::runtime_error("Failed to write resized image.");

  return true;
}

bool is_string_valid_name(const std::string& s) {
  if (s.empty()) return false;

  // Invalid character list from: https://stackoverflow.com/questions/4814040/allowed-characters-in-filename
  return std::count_if(s.begin(), s.end(), [](unsigned char c){ return (
    c == '\0' ||
    c == '\\' ||
    c == '/'  ||
    c == ':'  ||
    c == '*'  ||
    c == '?'  ||
    c == '"'  ||
    c == '<'  ||
    c == '>'  ||
    c == '|'  ||
    c == '.'
  );}) == 0;
}

void mkdir(std::string path) {
  if (!std::filesystem::exists(path))
    std::filesystem::create_directory(path);
}

bool must_exist(std::string path) {
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error("Path not found '" + path + "'.");
  }
  return true;
}

std::string get_stem(std::string path) {
  return std::filesystem::path(path).stem().string();
}

std::string insert_in_stem(std::string path, std::string s) {
  size_t insert_at = path.size();

  auto last_dot = path.find_last_of('.');
  if (last_dot != std::string::npos && last_dot > path.find_last_of('/')) { // Has extension
    insert_at = last_dot;
  }

  return path.insert(insert_at, s);
}

std::string get_next_available_path(std::string path) {
  if (!std::filesystem::exists(path))
    return path;

  int i = 1;
  std::string new_path;
  do {
    new_path = insert_in_stem(path, path_counter_prefix + std::to_string(i));
    i++;
  } while (std::filesystem::exists(new_path));

  return new_path;
}

std::string stripNonAlphaNum(const std::string& str) {
  std::string result;
  for (char ch : str) {
    if (std::isalnum(ch) || ch == ' ') {
      result += ch;
    }
  }
  return result;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c);});
    return s;
}

bool isSubstring(const std::string& s1, const std::string& s2) {
  return s1.find(s2) != std::string::npos || s2.find(s1) != std::string::npos;
}

// From: https://stackoverflow.com/a/43914385
int OptimalStringAlignmentDistance(std::string p_string1, std::string p_string2) {
    int l_string_length1 = p_string1.length();
    int l_string_length2 = p_string2.length();
    int d[l_string_length1+1][l_string_length2+1];

    int i;
    int j;
    int l_cost;

    for (i = 0;i <= l_string_length1;i++)
    {
        d[i][0] = i;
    }
    for(j = 0; j<= l_string_length2; j++)
    {
        d[0][j] = j;
    }
    for (i = 1;i <= l_string_length1;i++)
    {
        for(j = 1; j<= l_string_length2; j++)
        {
            if( p_string1[i-1] == p_string2[j-1] )
            {
                l_cost = 0;
            }
            else
            {
                l_cost = 1;
            }
            d[i][j] = std::min(
            d[i-1][j] + 1,                  // delete
            std::min(d[i][j-1] + 1,         // insert
            d[i-1][j-1] + l_cost)           // substitution
            );
            if( (i > 1) &&
            (j > 1) &&
            (p_string1[i-1] == p_string2[j-2]) &&
            (p_string1[i-2] == p_string2[j-1])
            )
            {
            d[i][j] = std::min(
            d[i][j],
             d[i-2][j-2] + l_cost   // transposition
            );
            }
        }
    }
    return d[l_string_length1][l_string_length2];
 }

float strings_match(std::string s1, std::string s2, int threshold) {
  std::transform(s1.begin(), s1.end(), s1.begin(), [](unsigned char c){ return std::tolower(c);});
  std::transform(s2.begin(), s2.end(), s2.begin(), [](unsigned char c){ return std::tolower(c);});

  if (isSubstring(s1, s2))
    return 1.f;

  auto dist = std::min(OptimalStringAlignmentDistance(s1, s2), OptimalStringAlignmentDistance(s2, s1));
  if (dist <= threshold)
    return 1.f - dist / threshold; // The score is based on the distance -> smaller distance = smaller the score

  return 0.f;
}

float chunks_match(const std::string& full_string, const std::string& small_string, int chunk_size) {
  if (chunk_size <= 0 || full_string.size() < static_cast<size_t>(chunk_size))
    return 0.f;

  for (size_t i = 0; i + chunk_size <= full_string.size(); i++) {
    std::string chunk = full_string.substr(i, chunk_size);
    return strings_match(small_string, chunk, 0);
  }

  return 0.f;
}

float matching_song(std::string query, std::string song_title, std::string song_artist, size_t threshold) {
  // matching returns a score out of 1 of how good the match is 0 being the worst and 1 being the best

  if (query.empty()) return 1.f; // Show all results if no query is inputted

  auto query_split = split_string(query, ' ');
  auto artist_split = split_string(song_artist, ' ');
  auto title_split = split_string(song_title, ' ');

  const float penalty = 1.f;

  float score = 0.f;
  float temporary_best_score = 0.f;

  // Check each word in the query against each word
  // in the string and sum the score this prioritizes
  // the closest match
  for (auto query_part : query_split) {
    for (auto part : title_split) {
      float match_score = strings_match(query_part, part, threshold);

      if (match_score > temporary_best_score) {
        temporary_best_score = match_score;
      }
    }

    // Skip artist check if it is already at the max score
    if (temporary_best_score != 1.f) {
      for (auto part : artist_split) {
        float match_score = strings_match(query_part, part, threshold);

        if (match_score > temporary_best_score) {
          temporary_best_score = match_score;
        }
      }
    }

    if (temporary_best_score > 0.f) {
      score += temporary_best_score;
    } else {
      score -= penalty; // Penalize if a word is not a match
    }

    temporary_best_score = 0.f;
  }

  // Get the score into the 0 to 1 range
  float normal_score = score / (query_split.size());

  return normal_score;
}

std::string escape_csv(const std::string& s) {
  std::string result = s;
  size_t pos = 0;
  while ((pos = result.find('"', pos)) != std::string::npos) {
    result.replace(pos, 1, "\"\"");
    pos += 2;
  }
  return result;
}

std::string title_string(const std::string& s) {
  std::string result;
  bool next_upper = true;

  for (auto c : s) {
    if (next_upper) {
      c = std::toupper(c);
      next_upper = false;
    }

    if (c == ' ') {
      next_upper = true;
    }

    result += c;
  }

  return result;
}

// From: https://stackoverflow.com/a/17708801
std::string url_encode(const std::string &value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;

  for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
    std::string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
      continue;
    }

    // Any other characters are percent-encoded
    escaped << std::uppercase;
    escaped << '%' << std::setw(2) << int((unsigned char) c);
    escaped << std::nouppercase;
  }

  return escaped.str();
}

void cleanup() {
  log("Exiting...");
  try {
    // Wait for any ongoing tasks to finish
    multistate_future_pool.wait();
  } catch (...) {
    log("Force exiting.", WARN);
  }
}
