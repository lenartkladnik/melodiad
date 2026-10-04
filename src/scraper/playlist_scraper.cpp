#include <vector>
#include <string>
#include <iostream>
#include <cstdlib>

#include "include/utils.hpp"
#include "include/webview_helper.hpp"

// Disable warnings produced by external libs
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "../../external/lib/nlohmann/json.hpp"

#pragma GCC diagnostic pop // Enable all warnings

std::string _scrape_spotify_js = R"(
async function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function fromSpotify() {
  const songContainerCssPath = '[data-testid="tracklist-row"] div[role="gridcell"][aria-colindex="2"]';
  const titleCssPath = '[data-testid="internal-track-link"] div';
  const artistCssPath = 'a[href*="/artist/"]';

  const results = [];
  const seenSongs = new Set();

  async function waitForContainer(timeout = 10000) {
    const start = Date.now();
    while (Date.now() - start < timeout) {
      const el = document.querySelector(songContainerCssPath);
      if (el) return el;
      await sleep(100);
    }
    return null;
  }

  const testSongContainer = await waitForContainer(10000);
  if (!testSongContainer) {
    returnSongs([]);
    return;
  }

  let lastBottomSongKey = null;
  let stagnationCount = 0;

  while (true) {
    const allSongs = Array.from(document.querySelectorAll(songContainerCssPath));

    for (const songContainer of allSongs) {
      if (!document.body.contains(songContainer)) continue;

      const titleEl = songContainer.querySelector(titleCssPath);
      const artistEl = songContainer.querySelector(artistCssPath);

      if (!titleEl || !artistEl) continue;

      const title = titleEl.textContent.trim();
      const artist = artistEl.textContent.trim();
      const key = title + "," + artist;

      if (!seenSongs.has(key)) {
        seenSongs.add(key);
        results.push({"title": title, "artist": artist});
      }
    }

    if (results.length > 0) {
      const currentBottomKey = results[results.length - 1].title + "," + results[results.length - 1].artist;
      if (currentBottomKey === lastBottomSongKey) {
        stagnationCount++;
        await sleep(100);
        if (stagnationCount > 3) break;
      } else {
        stagnationCount = 0;
        lastBottomSongKey = currentBottomKey;
      }
    }

    if (allSongs.length > 0) {
      allSongs[allSongs.length - 1].scrollIntoView(true);
    }

    await sleep(500);
  }

  returnSongs(results);
}

window.addEventListener('DOMContentLoaded', () => {
  setTimeout(fromSpotify, 2000);
});
)";

std::string _scrape_yt_music_js = R"(
function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function fromYtmusic() {
  const rejectButtonCssPath = '[action="https://consent.youtube.com/save"]';
  const itemCssPath = 'ytmusic-playlist-shelf-renderer ytmusic-responsive-list-item-renderer';
  const rowCssPath = 'div.flex-columns.ytmusic-responsive-list-item-renderer';

  const results = [];
  const seenSongs = new Set();

  async function waitForSelector(selector, timeout = 10000) {
    const start = Date.now();
    while (Date.now() - start < timeout) {
      const el = document.querySelector(selector);
      if (el) return el;
      await sleep(100);
    }
    return null;
  }

  // Dismiss cookie prompt (if any)
  const rejectButton = await waitForSelector(rejectButtonCssPath, 5000);
  if (rejectButton) {
    const form = rejectButton.closest('form');
    if (form && form.requestSubmit) form.requestSubmit();
    else rejectButton.click();
    await sleep(2000); // page reloads
  }

  const firstItem = await waitForSelector(itemCssPath, 10000);
  if (!firstItem) {
    returnSongs([]);
    return;
  }

  function collectVisible() {
    const items = Array.from(document.querySelectorAll(itemCssPath));
    for (const item of items) {
      const songContainer = item.querySelector(rowCssPath);
      if (!songContainer) continue;

      const formattedStrings = songContainer.querySelectorAll('yt-formatted-string');
      const title = formattedStrings[0]?.textContent.trim();
      const artist = formattedStrings[1]?.textContent.trim();
      if (!title) continue;

      const key = title + "," + (artist || "");
      if (!seenSongs.has(key)) {
        seenSongs.add(key);
        results.push({"title": title, "artist": artist || ""});
      }
    }
    return items;
  }

  const maxStagnantRounds = 3; // Maybe loading fails to be fast enough or idk
  let stagnantRounds = 0;
  let lastCount = -1;

  while (stagnantRounds < maxStagnantRounds) {
    const items = collectVisible();

    if (results.length === lastCount) {
      stagnantRounds++;
    } else {
      stagnantRounds = 0;
      lastCount = results.length;
    }

    // Scroll down
    if (items.length > 0) {
      items[items.length - 1].scrollIntoView({ block: 'end' });
    }

    await sleep(stagnantRounds > 0 ? 1000 : 600);
  }

  collectVisible();
  returnSongs(results);
}

window.addEventListener('DOMContentLoaded', () => {
  setTimeout(fromYtmusic, 2000);
});
)";

std::vector<std::string> scrape_playlist(const std::string& query) {
  webview::webview w(true, nullptr);

  std::string url = query;
  
  if (query.find("https://") == std::string::npos) {
    url = "https://" + url;
  }

  init_window(w, url);

  std::vector<std::string> collected;

  // This will be called by the js when it finishes scraping
  w.bind("returnSongs", [&](const std::string&, const std::string& req, void* arg) {
    auto json_data = nlohmann::json::parse(req)[0];

    for (const auto& item : json_data) {
      collected.push_back('"' + escape_csv(item["title"]) + "\",\"" + escape_csv(item["artist"]) + '"');
    }

    webview::webview* w_ptr = static_cast<webview::webview*>(arg);
    w_ptr->terminate();
    return "";
  }, &w);

  if (url.find("open.spotify.com") != std::string::npos) {
    w.init(_scrape_spotify_js);
  } else if (url.find("music.youtube.com") != std::string::npos) {
    w.init(_scrape_yt_music_js);
  } else {
    throw std::runtime_error("Invalid URL.\n");
  }

  w.navigate(url);
  w.run();

  return collected;
}
