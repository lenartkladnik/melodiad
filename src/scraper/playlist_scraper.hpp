/*
* playlist_scraper.hpp
*
* Copyright (C) 2026  Lenart Kladnik
* You may use, distribute and modify this code under the terms of the GPL license,
* see LICENSE file.
*
* For version information, see main.cpp
*/

#pragma once

/*
auto r = scrape_playlist("https://music.youtube.com/playlist?list=PLC7BP2QZenrVUrZPciCuqma4JpL8mtP_B");
auto r = scrape_playlist("https://open.spotify.com/playlist/2bYs7IlWQqyuLHmcd4bt9g");
for (const auto& i : r) {
  // i
}
*/
std::vector<std::string> scrape_playlist(const std::string& url);
