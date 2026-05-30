# MyTV - Local Video Search and Playback Aggregator

MyTV is a lightweight C++17 application for searching video resources from multiple API sites, caching the raw JSON results locally, aggregating them by title, and serving a built-in web UI for browsing and playback.

The project uses a native C++ backend plus a plain HTML/CSS/JavaScript frontend. It is designed for local use: search across configured provider APIs, persist the responses under `output/`, and browse the merged catalog in the browser at `http://localhost:8080`.

## Features

- Multi-site search driven by `input/source.json`
- Concurrent search across multiple provider APIs with a concurrency cap
- Local JSON caching under `output/`
- Aggregated catalog grouped by video title
- Built-in web UI for search, browsing, source switching, and playback
- Artplayer + Hls.js based playback for `m3u8` and common video URLs
- Non-blocking in-page search status feedback
- Custom modal UI for confirm and error flows
- Plain-text description cleanup for HTML-rich `vod_content`
- Fault-tolerant JSON parsing: bad files or bad entries are skipped instead of aborting the whole load
- Timestamped backend logs for search and catalog loading
- Site display names resolved from the `name` field in `input/source.json`

## Tech Stack

- C++17
- CMake
- Crow
- libcurl
- nlohmann/json
- Plain HTML/CSS/JavaScript frontend
- Artplayer
- Hls.js

## Project Structure

```text
mytv/
|- 3rdparty/
|  |- crow/
|  `- nlohmann/
|- front/
|  |- css/
|  |- js/
|  `- index.html
|- input/
|  `- source.json
|- output/
|- src/
|  |- main.cpp
|  |- web_server.cpp
|  |- web_server.h
|  |- json_parser.cpp
|  |- json_parser.h
|  |- https_json_client.cpp
|  `- https_json_client.h
`- CMakeLists.txt
```

## How It Works

1. The backend reads provider definitions from `input/source.json`.
2. A search request calls each configured API site in parallel with a max concurrency of `4`.
3. Raw JSON responses are saved into `output/*.json`.
4. The backend parses all cached JSON files and aggregates videos by `vod_name`.
5. The frontend requests the aggregated catalog from `/api/videos`.
6. The user can browse titles, switch sources, choose episodes, and play streams in the browser.

## Requirements

- A C++17 compiler
- CMake 3.15+
- libcurl development package
- pthread-compatible runtime on Linux/WSL

The current build file links against:

- `curl`
- `-pthread`

## Build

Typical CMake build:

```bash
cmake -S . -B build
cmake --build build
```

## Run

Start the executable from the `build/` directory so the relative paths resolve correctly.

The program expects these paths relative to the executable working directory:

- `../front/`
- `../input/`
- `../output/`

Example:

```bash
cd build
./mytv
```

Then open:

```text
http://localhost:8080
```

The server runs on port `8080` by default.

## Frontend Notes

The current frontend includes:

- Dark cinema-style UI with restrained gold accents
- Search box and title list browsing
- Source tabs using provider display names from `source.json`
- Episode selection grid
- Bottom status bar for non-blocking feedback
- Custom in-page confirm and alert dialogs
- Clean plain-text description rendering

## Backend Notes

### Search behavior

- Search requests are trimmed before execution
- Cached JSON files are reset before a new search
- Search runs across all configured sites with limited concurrency
- A search is considered successful only if at least one valid response is saved

### Parsing behavior

- Missing or malformed files are skipped
- Malformed individual video entries are skipped
- Catalog loading continues even if one file fails
- Per-file and total parsing statistics are logged

### Logging

Backend logs use timestamped module-prefixed output such as:

```text
[2026-05-30 12:34:56] [WebServer] [INFO] ...
[2026-05-30 12:34:56] [JsonParser] [ERROR] ...
```

## Output and Caching

Search results are written to:

```text
output/
```

Each provider response is saved as one JSON file. File names are derived from the provider domain with dots converted to underscores.

Before a new search, existing JSON cache files are cleared. The backend also keeps a backup copy of old cached files under a sibling backup directory when cleanup runs.

## Known Notes

- This project is intended for local/self-hosted use
- Playback availability depends on third-party provider data and stream validity
- Network failures, malformed provider payloads, or site-side changes can affect results
- `cache_time` is currently present in config but not enforced by backend cache expiration logic
- `main.cpp` currently waits for Enter key input to exit after launching the web server

## Future Improvements

- Configurable search concurrency
- Retry and timeout strategy per provider
- Better provider failure summaries in API responses
- Unified logging helper shared across backend modules
- Richer catalog artwork and hero presentation in the frontend

## License

No license file is currently included in this repository.
