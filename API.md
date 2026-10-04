# Melodia Server API Documentation

Melodiad is a local music server backend for [Melodia](https://github.com/lenartkladnik/Melodia-desktop) with an API built on top of [httplib](https://github.com/yhirose/cpp-httplib) that supports searching, downloading (via `yt-dlp`), and managing songs and playlists. The server listens on **`0.0.0.0:5400`** by default.

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

Search trough downloaded songs.

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

- `400 Bad Request` - missing or bad `query` parameter.

---

### `GET /get-song`

Fetch metadata for a song.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`song_id`|integer|Yes|ID of the song.|

**Responses**

- `200 OK` - `application/json`:

  ```json
  {
    "artist": "Queen",
    "title": "Bohemian Rhapsody",
    "image": "/data/songs/0.png",
    "thumbnail": "/data/songs/0.small.png"
  }
  ```

- `400 Bad Request` - missing or bad `song_id`.

---

### `GET /get-playlist`

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
    "songs": [
        2,
        1
    ]
  }
  ```

- `400 Bad Request` - missing or bad `playlist_name`.

---

### `GET /get-all-playlists`

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

### `Web Socket /download`

Start to download a song matching a search query.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`query`|string|Yes|Search term to download.|

**Responses**

```json
{
  "completed": int,
  "total": int,
  "good": bool,
  "message": string
}
```

**Note:** For downloading a list of songs use [/download-list](#web-socket-download-list) since it downloads using many threads

---

### `Web Socket /download-list`

Download a list of songs.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`list`|json array|Yes|List of search terms to download.|

**Responses**

```json
{
  "completed": int,
  "total": int
}
```

---

### `GET /create-playlist`

Create a new playlist with a song.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`song_id`|integer|Yes|ID of the song to put in the playlist.|

**Responses**

- `200 OK` - new playlist name
- `400 Bad Request` - missing or bad `song_id`

---

### `GET /rename-playlist`

Rename an existing playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`old_name`|string|Yes|Current playlist name.|
|`new_name`|string|Yes|New playlist name.|

**Responses**

- `200 OK` - new playlist name
- `400 Bad Request` - missing or bad parameter(s).

---

### `GET /playlist-add-item`

Add a song to a playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Target playlist name.|
|`song_id`|integer|Yes|Song ID to add.|

**Responses**

- `200 OK` - `Ok.`
- `400 Bad Request` - missing or bad parameter(s).

---

### `GET /playlist-remove-item`

Remove a song from a playlist.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Target playlist name.|
|`song_id`|integer|Yes|Song ID to remove.|

**Responses**

- `200 OK` - `Ok.`
- `400 Bad Request` - missing or bad parameter(s).

---

### `GET /delete-song`

Delete a song by ID.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`song_id`|integer|Yes|ID of the song to delete.|

**Responses**

- `200 OK` - `Ok.`
- `400 Bad Request` - missing or bad `song_id`.

---

### `GET /delete-playlist`

Delete a playlist by name.

**Query Parameters**

|Name|Type|Required|Description|
|-|-|-|-|
|`playlist_name`|string|Yes|Name of the playlist to delete.|

**Responses**

- `200 OK` - `Ok.`
- `400 Bad Request` - missing or bad `playlist_name`.

---

### `GET /fast-autocomplete`

Get autocomplete suggestions from yt-dlp (generally lower accuracy).

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

---

### `GET /slow-autocomplete`

Get autocomplete suggestions from yt music (generally higher accuracy).

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

---

### `GET /update-yt-dlp`

Update yt-dlp.

**Response**

- `200 OK` - `Ok.`
- `500 Internal Server Error`
  - `YtDlp doesn't exist on the system.`
  - `YtDlp update command failed ('yt_dlp_path -U').`

---
