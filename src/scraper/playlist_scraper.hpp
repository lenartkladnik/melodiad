#pragma once

/*
auto r = scrape_playlist("https://music.youtube.com/playlist?list=PLC7BP2QZenrVUrZPciCuqma4JpL8mtP_B");
auto r = scrape_playlist("https://open.spotify.com/playlist/2bYs7IlWQqyuLHmcd4bt9g");
for (const auto& i : r) {
  // i
}
*/
std::vector<std::string> scrape_playlist(const std::string& url);
