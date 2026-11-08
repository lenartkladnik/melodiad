# Melodia Server API Documentation

Melodiad is a local music server backend for [Melodia](https://github.com/lenartkladnik/Melodia-desktop) with an API built on top of [httplib](https://github.com/yhirose/cpp-httplib) that supports searching, downloading (via `yt-dlp`), and managing songs and playlists. The server listens on **`0.0.0.0:5400`** by default.

## Command Line Options

|Option|Description|
|-|-|
|`-h`, `--help`|Show the help screen.|
|`-v`, `--version`|Print the version.|
|`-P`, `--port`|Port the API listens on (default `5400`).|
|`-H`, `--host`|Host the API listens on (default `0.0.0.0`).|

## Base URL

```text
http://<host>:5400
```

## Static File Mount

|Path|Description|
|-|-|
|`/data`|Serves files directly from the music storage directory.|

---

## Endpoints

### `GET /`

Status check.

**Response:** `200 OK` - plain text `Server running Melodia`.

---

### `GET /search`

Search through downloaded songs.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`query`|string|Yes|Search term.|

**Responses**

- `200 OK` - `application/json` array of song ids.

  ```json
  [
    1,
    4,
    5
  ]
  ```

- `400 Bad Request` - missing `query` parameter.

---

### `GET /song`

Fetch metadata for a song.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`song_id`|integer|Yes|ID of the song.|

**Responses**

- `200 OK` - `application/json`:

  ```json
  {
    "title": "Bohemian Rhapsody",
    "artist": "Queen",
    "base": "/data/songs/0",
    "audio": "/data/songs/0.mp3",
    "image": "/data/songs/0.png",
    "thumbnail": "/data/songs/0.small.png"
  }
  ```

- `400 Bad Request` - missing `song_id` or `song_id` is not an integer.

---

### `GET /song/stream`

Stream the audio of a song.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`song_id`|integer|Yes|ID of the song.|

**Responses**

- `200 OK` - `audio/mpeg` file content.
- `400 Bad Request` - missing `song_id` or `song_id` is not an integer.

---

### `GET /song/delete`

Delete a song by ID.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`song_id`|integer|Yes|ID of the song to delete.|

**Responses**

- `200 OK` - `Ok`
- `400 Bad Request` - missing `song_id` or `song_id` is not an integer.

---

### `GET /playlist`

Fetch metadata for a playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Name of the playlist.|

**Responses**

- `200 OK` - `application/json`:

  ```json
  {
    "image": "/data/playlists/My%20music.png",
  }
  ```

- `400 Bad Request` - missing `playlist_name` or the playlist doesn't exist.

---

### `GET /playlist/items`

Fetch items for a playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Name of the playlist.|

**Responses**

- `200 OK` - `application/json`:

  ```json
  {
    "songs": [
        2,
        1
    ]
  }
  ```

- `400 Bad Request` - missing `playlist_name` or the playlist doesn't exist.

---

### `GET /playlist/next`

Get the song at a given index of a playlist. The index wraps around (`index % playlist length`), so it can be incremented indefinitely. Optionally the playlist can be shuffled deterministically using a seed, so the same `seed` always yields the same order.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Name of the playlist.|
|`index`|integer|Yes|Non-negative position in the playlist.|
|`shuffle`|bool|No|Shuffles the playlist when set to `true`. If this parameter is present, `seed` is required.|
|`seed`|integer|With `shuffle`|Seed for the shuffle.|
|`loop`|bool|Yes|Loops the playlist when set to `true`. If it is set to `false`, `-1` is returned if a song past the end of the playlist is requested.|

**Responses**

- `200 OK` - `application/json`:

  ```json
  {
    "song_id": 3
  }
  ```

  If the playlist is empty, `{}` is returned.

- `400 Bad Request` - missing `playlist_name` or `index`, `index` is not an integer or is negative, or `shuffle` is given without a valid integer `seed`.

---

### `GET /playlists`

List all playlists.

**Responses**

- `200 OK` - `application/json` list of all playlists by name.

```json
[
    "My music",
    "Rock hits"
]
```

---

### `GET /playlist/new`

Create a new playlist with a song.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`song_id`|integer|Yes|ID of the song to put in the playlist.|

**Responses**

- `200 OK` - new playlist name
- `400 Bad Request` - missing `song_id` or `song_id` is not an integer.

---

### `GET /playlist/rename`

Rename an existing playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`old_name`|string|Yes|Current playlist name.|
|`new_name`|string|Yes|New playlist name.|

**Responses**

- `200 OK` - new playlist name
- `400 Bad Request` - missing parameter(s).

---

### `GET /playlist/add`

Add a song to a playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Target playlist name.|
|`song_id`|integer|Yes|Song ID to add.|

**Responses**

- `200 OK` - `Ok`
- `400 Bad Request` - missing parameter(s) or `song_id` is not an integer.

---

### `GET /playlist/remove`

Remove a song from a playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Target playlist name.|
|`song_id`|integer|Yes|Song ID to remove.|

**Responses**

- `200 OK` - `Ok`
- `400 Bad Request` - missing parameter(s) or `song_id` is not an integer.

---

### `GET /playlist/delete`

Delete a playlist by name.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Name of the playlist to delete.|

**Responses**

- `200 OK` - `Ok`
- `400 Bad Request` - missing `playlist_name`.

---

### `Web Socket /download`

Download song(s).

**Send**

Each text message sent to this web socket is used as a search query and a download is started for it. Song ids are assigned sequentially, starting at the next available id. Downloads run concurrently (up to a fixed maximum; additional requests wait for a free slot). Send an **empty message** to signal that there are no more songs.

**Receive**

For each song that finishes downloading the following is sent back to the client:

```json
{
  "id": int,
  "success": bool
}
```

Once the empty message has been received and all downloads have finished, the server closes the connection.

---

### `GET /autocomplete/fast`

Get autocomplete suggestions from yt-dlp (generally lower accuracy).

Only the most recent autocomplete request (fast or slow) is served; older in-flight requests are abandoned.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`query`|string|Yes|Partial search term.|

**Responses**

- `200 OK` - `application/json` (top most result is the best match):

  ```json
  [
    "Bohemian Rhapsody Queen",
    "Queen Bohemian Rhapsody",
    "Bohemian Rhapsody Queen Live",
    "Bohemian Rhapsody Queen Reaction",
    "Bohemian Rhapsody Queen Lyrics",
    "Bohemian Rhapsody Queen Gets Back Together"
  ]
  ```

- `400 Bad Request` - missing `query`.
- `409 Conflict` - the request was superseded by a newer autocomplete request.
- `500 Internal Server Error` - autocomplete failed.

---

### `GET /autocomplete/slow`

Get autocomplete suggestions from yt music (generally higher accuracy).

Only the most recent autocomplete request (fast or slow) is served; older in-flight requests are abandoned.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`query`|string|Yes|Partial search term.|

**Responses**

- `200 OK` - `application/json` (top most result is the best match):

  ```json
  [
    "Bohemian Rhapsody Queen",
    "Queen Bohemian Rhapsody",
    "Bohemian Rhapsody Queen Live",
    "Bohemian Rhapsody Queen Reaction",
    "Bohemian Rhapsody Queen Lyrics",
    "Bohemian Rhapsody Queen Gets Back Together"
  ]
  ```

- `400 Bad Request` - missing `query`.
- `409 Conflict` - the request was superseded by a newer autocomplete request.
- `500 Internal Server Error` - autocomplete failed.

---

### `GET /update`

Update yt-dlp.

**Responses**

- `200 OK` - `Ok`
- `500 Internal Server Error`
  - Empty body - yt-dlp doesn't exist on the system.
  - `Failed` - the update command (`<yt_dlp_path> -U`) failed.

---
