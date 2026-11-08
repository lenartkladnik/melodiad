/*
* storage_handler.cpp
*
* Copyright (C) 2026  Lenart Kladnik
* You may use, distribute and modify this code under the terms of the GPL license,
* see LICENSE file.
*
* For version information, see main.cpp
*/

#include <iostream>
#include <filesystem>
#include <fstream>
#include <cctype>
#include <algorithm>
#include <sys/stat.h>
#include "include/data.hpp"
#include "include/utils.hpp"
#include "include/storage_handler.hpp"

extern const std::string base_path = "./";
extern const std::string base_path_external = base_path + "external/";
extern const std::string base_path_external_prog = base_path_external + "prog/";
extern const std::string base_music_path = ".music_data/";
extern const std::string base_music_path_songs = base_music_path + "songs/";
extern const std::string base_music_path_playlists = base_music_path + "playlists/";
extern const std::string path_counter_prefix = " #";

bool ensure_storage() {
  try {
    if (!(must_exist(base_path)))
      return false;

    mkdir(base_music_path);
    mkdir(base_music_path_songs);
    mkdir(base_music_path_playlists);
    mkdir(base_path_external);
    mkdir(base_path_external_prog);
  } catch (const std::filesystem::filesystem_error& err) {
    throw std::runtime_error("Failed to check and create necessary directories '" + std::string(err.what()) + "'.");
  }

  return true;
}

void remove_playlist(std::string playlist) {
  // Try to remove all items that make up a playlist
  auto playlist_path = base_music_path_playlists + playlist;
  try {
    std::filesystem::remove(playlist_path);
    std::filesystem::remove(playlist_path + ".png");
  } catch (...) {}
}

void remove_song(int id) {
  // Try to remove all items which make up a song
  std::vector<std::string> items = {"title", "artist", "mp3", "png", "small.png"};
  for (const auto& item : items) {
    try {
      std::filesystem::remove(base_music_path_songs + std::to_string(id) + "." + item);
    } catch (...) {}
  }

  // Remove the song from all playlists
  auto playlists = get_all_playlists();
  for (const auto& playlist : playlists) {
    remove_from_playlist(playlist, id);
  }
}

size_t get_next_available_song_id() {
  int max_id = -1;

  for (const auto& entry : std::filesystem::directory_iterator(base_music_path_songs)) {
    int id = std::stoi(entry.path().stem().string());

    if (id > max_id)
      max_id = id;
  }

  size_t new_id = max_id + 1;
  return new_id;
}

bool exists_playlist(std::string playlist_name) {
  return std::filesystem::exists(base_music_path_playlists + playlist_name);
}

bool exists_song(int id) {
  return std::filesystem::exists(base_music_path_songs + std::to_string(id) + ".mp3");
}

std::string create_new_playlist(int song_id) {
  std::string playlist = "New playlist";
  auto playlist_path = get_next_available_path(base_music_path_playlists + playlist);
  auto playlist_stem = get_stem(playlist_path);

  std::ofstream playlist_file(playlist_path);
  playlist_file << song_id << "\n";
  playlist_file.close();

  std::filesystem::copy(base_music_path_songs + std::to_string(song_id) + ".png", base_music_path_playlists + playlist_stem + ".png");

  return playlist_stem;
}

// TODO: Support utf32 in playlist names
std::string rename_playlist(std::string old_playlist, std::string new_playlist) {
  if (old_playlist == new_playlist)
    return new_playlist; // Already correct name

  if (!is_string_valid_name(new_playlist)) {
    log("Failed to rename playlist: Playlist name ('" + new_playlist + "') contains invalid characters.", WARN);
    return old_playlist;
  }

  auto new_playlist_path = get_next_available_path(base_music_path_playlists + new_playlist);
  std::filesystem::rename(base_music_path_playlists + old_playlist, new_playlist_path);
  std::filesystem::rename(base_music_path_playlists + old_playlist + ".png", new_playlist_path + ".png");

  return get_stem(new_playlist_path);
}

void add_to_playlist(std::string playlist, int song_id) {
  std::ofstream playlist_file(base_music_path_playlists + playlist, std::ios_base::app);
  playlist_file << song_id << "\n";
  playlist_file.close();
}

void remove_from_playlist(std::string playlist, int song_id) {
  std::ifstream playlist_file_i(base_music_path_playlists + playlist);
  std::string id;
  std::string new_ids = "";
  while (std::getline(playlist_file_i, id)) {
    if (id != std::to_string(song_id))
      new_ids += id + "\n";
  }
  playlist_file_i.close();

  std::ofstream playlist_file_o(base_music_path_playlists + playlist);
  playlist_file_o << new_ids;
  playlist_file_o.close();
}

std::string construct_song_path(int id) {
  return base_music_path_songs + std::to_string(id);
}

std::string get_song_title(int id) {
  auto song_path = construct_song_path(id);

  std::ifstream title_file(song_path + ".title");
  std::string title_string = "";
  if (title_file.good()) {
    std::getline(title_file, title_string);
  } else {
    throw std::runtime_error("Failed to read title from '" + song_path + ".title'.");
  }

  return title_string;
}

std::string get_song_artist(int id) {
  auto song_path = construct_song_path(id);

  std::ifstream artist_file(song_path + ".artist");
  std::string artist_string = "";
  if (artist_file.good()) {
    std::getline(artist_file, artist_string);
  } else {
    throw std::runtime_error("Failed to read artist name from '" + song_path + ".artist'.");
  }

  return artist_string;
}

std::vector<std::string> get_all_playlists() {
  std::vector<std::string> playlists;

  struct stat s;
  for (const auto& entry : std::filesystem::directory_iterator(base_music_path_playlists)) {
    auto path = entry.path();
    auto str_path = path.string();

    // First check that the path doesn't contain a dot, because the playlist files don't
    // have an extension. Then check that the path is a file and not a directory.
    if (path.filename().string().find(".") == std::string::npos && stat(str_path.c_str(), &s) == 0 && !(s.st_mode & S_IFDIR)) {
      auto u8 = path.stem().u8string();
      playlists.push_back(std::string(reinterpret_cast<const char*>(u8.c_str())));
    }
  }

  return playlists;
}

std::vector<int> get_playlist(const std::string& name) {
  std::vector<int> ids;
  std::string entry;
  std::ifstream playlist_file(base_music_path_playlists + name);

  while (std::getline(playlist_file, entry)) {
    int id = std::stoi(entry);

    if (std::find(ids.begin(), ids.end(), id) == ids.end())
      ids.push_back(id);
  }

  return ids;
}

std::vector<int> search_all_songs(const std::string& query) {
  std::vector<std::pair<float, int>> scored_results;

  struct stat s;
  for (const auto& entry : std::filesystem::directory_iterator(base_music_path_songs)) {
    auto path = entry.path();
    auto str_path = path.string();

    // First check that the path contains the .title extensions, since it is one of the
    // extensions every song has. Then check that the path is a file and not a directory.
    if (path.filename().string().find(".title") != std::string::npos && stat(str_path.c_str(), &s) == 0 && !(s.st_mode & S_IFDIR)) {
      int id = -1;
      try {
        auto u8 = path.stem().u8string();
        id = std::stoi(std::string(reinterpret_cast<const char*>(u8.c_str())));
      } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << std::endl;
        continue;
      } catch (const std::out_of_range& e) {
        std::cerr << e.what() << std::endl;
        continue;
      }

      if (id >= 0) {
        float score = matching_song(query, get_song_title(id), get_song_artist(id), MIN_MATCH_DIFF);
        if (score > 0.f)
          scored_results.emplace_back(score, id);
      }
    }
  }

  std::sort(scored_results.begin(), scored_results.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

  std::vector<int> results;
  results.reserve(scored_results.size());
  for (const auto& [_, id] : scored_results) {
    results.push_back(id);
  }

  return results;
}
