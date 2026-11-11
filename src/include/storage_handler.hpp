/*
* storage_handler.hpp
*
* Copyright (C) 2026  Lenart Kladnik
* You may use, distribute and modify this code under the terms of the GPL license,
* see LICENSE file.
*
* For version information, see main.cpp
*/

#pragma once

extern std::string base_path;
extern std::string base_path_external;
extern std::string base_path_external_prog;
extern std::string base_music_path;
extern std::string base_music_path_songs;
extern std::string base_music_path_playlists;
extern const std::string path_counter_prefix;

bool ensure_storage(std::string new_base_path = "./");
void remove_playlist(std::string playlist);
void remove_song(int id);
size_t get_next_available_song_id();
bool exists_playlist(std::string playlist_name);
bool exists_song(int id);
std::string create_new_playlist(int song_id);
std::string rename_playlist(std::string old_playlist, std::string new_playlist);
void add_to_playlist(std::string playlist, int song_id);
void remove_from_playlist(std::string playlist, int song_id);
std::string construct_song_path(int id);
std::string get_song_title(int id);
std::string get_song_artist(int id);
std::vector<std::string> get_all_playlists();
std::vector<int> get_playlist(const std::string& name);
std::vector<int> search_all_songs(const std::string& query);
