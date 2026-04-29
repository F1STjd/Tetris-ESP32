#pragma once

#if defined(ARDUINO)

#include "game.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <WiFi.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace tetris_server
{

inline AsyncWebServer server(80);

constexpr char wifi_ssid[] { "" };
constexpr char wifi_password[] { "" };
constexpr char fallback_ap_ssid[] { "Tetris-ESP32" };
constexpr char scoreboard_file_path[] { "/scores.json" };
constexpr std::size_t max_scoreboard_entries { 20U };
constexpr std::size_t player_name_max_length { 20U };

const char dashboard_html[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>ESP32 Tetris Dashboard</title>
  <style>
    body { font-family: Arial, sans-serif; margin: 0; background: #10141f; color: #e8eefc; }
    .wrap { max-width: 980px; margin: 0 auto; padding: 20px; }
    h1 { margin-top: 0; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(170px, 1fr)); gap: 12px; }
    .card { background: #1b2233; border: 1px solid #33415f; border-radius: 10px; padding: 12px; }
    .label { font-size: 0.78rem; opacity: 0.8; }
    .value { font-size: 1.4rem; font-weight: 700; }
    .row { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }
    button, input { border-radius: 8px; border: 1px solid #445375; background: #1c2947; color: #f4f7ff; padding: 8px 12px; }
    button { cursor: pointer; }
    table { width: 100%; border-collapse: collapse; margin-top: 10px; }
    th, td { text-align: left; padding: 8px; border-bottom: 1px solid #2f3a56; }
    #score-form { display: none; margin-top: 12px; }
    .muted { opacity: 0.75; font-size: 0.9rem; }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>ESP32 Tetris Dashboard</h1>
    <p class="muted" id="network">Connecting...</p>

    <div class="grid">
      <div class="card"><div class="label">Score</div><div class="value" id="score">0</div></div>
      <div class="card"><div class="label">Lines</div><div class="value" id="lines">0</div></div>
      <div class="card"><div class="label">Level</div><div class="value" id="level">1</div></div>
      <div class="card"><div class="label">Next Block</div><div class="value" id="next">-</div></div>
      <div class="card"><div class="label">Game State</div><div class="value" id="state">RUNNING</div></div>
      <div class="card"><div class="label">Free Heap</div><div class="value" id="heap">-</div></div>
    </div>

    <form id="score-form" class="row">
      <span>Game finished. Save your score:</span>
      <input id="player-name" type="text" maxlength="20" placeholder="Player name" />
      <button type="submit">Submit score</button>
    </form>

    <div class="row" style="margin-top:10px;">
      <button id="restart-btn" type="button">Restart game</button>
      <span class="muted" id="pending"></span>
    </div>

    <h2>Scoreboard</h2>
    <table>
      <thead><tr><th>#</th><th>Name</th><th>Score</th><th>Lines</th><th>Level</th><th>Duration (s)</th></tr></thead>
      <tbody id="scores-body"></tbody>
    </table>
  </div>

  <script>
    const scoreEl = document.getElementById('score');
    const linesEl = document.getElementById('lines');
    const levelEl = document.getElementById('level');
    const nextEl = document.getElementById('next');
    const stateEl = document.getElementById('state');
    const heapEl = document.getElementById('heap');
    const networkEl = document.getElementById('network');
    const pendingEl = document.getElementById('pending');
    const scoresBody = document.getElementById('scores-body');
    const scoreForm = document.getElementById('score-form');
    const playerName = document.getElementById('player-name');
    const restartBtn = document.getElementById('restart-btn');

    function renderScores(entries) {
      scoresBody.textContent = '';
      entries.forEach((entry, i) => {
        const row = scoresBody.insertRow();
        row.insertCell().textContent = String(i + 1);
        row.insertCell().textContent = entry.name;
        row.insertCell().textContent = String(entry.score);
        row.insertCell().textContent = String(entry.lines);
        row.insertCell().textContent = String(entry.level);
        row.insertCell().textContent = String(Math.floor(entry.durationMs / 1000));
      });
    }

    async function updateState() {
      try {
        const response = await fetch('/api/state');
        if (!response.ok) { return; }
        const state = await response.json();

        scoreEl.textContent = String(state.score);
        linesEl.textContent = String(state.lines);
        levelEl.textContent = String(state.level);
        nextEl.textContent = state.nextBlock;
        heapEl.textContent = String(state.heap);
        stateEl.textContent = state.gameOver ? 'GAME OVER' : 'RUNNING';
        networkEl.textContent = 'Network mode: ' + state.networkMode + ' | Server IP: ' + state.serverIp;

        if (state.awaitingName) {
          scoreForm.style.display = 'flex';
          pendingEl.textContent =
            'Pending score: ' + state.pending.score + ' (' +
            Math.floor(state.pending.durationMs / 1000) + ' s)';
        } else {
          scoreForm.style.display = 'none';
          pendingEl.textContent = '';
        }

        renderScores(state.scoreboard || []);
      } catch (error) {
        networkEl.textContent = 'Dashboard disconnected. Retrying...';
      }
    }

    scoreForm.addEventListener('submit', async (event) => {
      event.preventDefault();
      await fetch('/api/score', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name: playerName.value })
      });
      playerName.value = '';
      await updateState();
    });

    restartBtn.addEventListener('click', async () => {
      await fetch('/api/restart', { method: 'POST' });
      await updateState();
    });

    updateState();
    setInterval(updateState, 800);
  </script>
</body>
</html>
)HTML";

struct scoreboard_entry
{
  String name { "anonymous" };
  std::uint32_t score { 0U };
  std::uint32_t lines { 0U };
  std::uint32_t level { 1U };
  std::uint32_t duration_ms { 0U };
  std::uint32_t ended_at_ms { 0U };
};

struct pending_score_data
{
  bool ready { false };
  std::uint32_t score { 0U };
  std::uint32_t lines { 0U };
  std::uint32_t level { 1U };
  std::uint32_t duration_ms { 0U };
  std::uint32_t ended_at_ms { 0U };
};

inline std::vector<scoreboard_entry> scoreboard {};
inline pending_score_data pending_score {};
inline std::uint32_t game_started_ms { 0U };
inline bool previous_game_over { false };
inline bool spiffs_ready { false };

[[nodiscard]] inline auto
block_name_for(const tetris::block_t& block) -> const char*
{
  switch (block.index())
  {
  case 0U:
    return "O";
  case 1U:
    return "I";
  case 2U:
    return "S";
  case 3U:
    return "Z";
  case 4U:
    return "J";
  case 5U:
    return "L";
  case 6U:
  default:
    return "T";
  }
}

[[nodiscard]] inline auto
sanitize_player_name(String name) -> String
{
  name.trim();
  name.replace("<", "");
  name.replace(">", "");

  if (name.isEmpty()) { return "anonymous"; }
  if (name.length() > player_name_max_length)
  {
    name.remove(player_name_max_length);
  }
  return name;
}

inline void
sort_and_trim_scoreboard()
{
  std::sort(
    scoreboard.begin(), scoreboard.end(),
    [](const scoreboard_entry& left, const scoreboard_entry& right) -> bool
    {
      if (left.score != right.score) { return left.score > right.score; }
      if (left.lines != right.lines) { return left.lines > right.lines; }
      return left.ended_at_ms > right.ended_at_ms;
    }
  );

  if (scoreboard.size() > max_scoreboard_entries)
  {
    scoreboard.resize(max_scoreboard_entries);
  }
}

inline void
save_scoreboard()
{
  if (!spiffs_ready) { return; }

  DynamicJsonDocument doc(8192U);
  JsonArray scoreboard_array = doc.to<JsonArray>();

  for (const auto& entry : scoreboard)
  {
    JsonObject node = scoreboard_array.createNestedObject();
    node[ "name" ] = entry.name;
    node[ "score" ] = entry.score;
    node[ "lines" ] = entry.lines;
    node[ "level" ] = entry.level;
    node[ "durationMs" ] = entry.duration_ms;
    node[ "endedAtMs" ] = entry.ended_at_ms;
  }

  File output = SPIFFS.open(scoreboard_file_path, FILE_WRITE);
  if (!output) { return; }
  serializeJson(doc, output);
  output.close();
}

inline void
load_scoreboard()
{
  scoreboard.clear();
  scoreboard.reserve(max_scoreboard_entries);

  if (!spiffs_ready || !SPIFFS.exists(scoreboard_file_path)) { return; }

  File input = SPIFFS.open(scoreboard_file_path, FILE_READ);
  if (!input) { return; }

  DynamicJsonDocument doc(8192U);
  const auto error = deserializeJson(doc, input);
  input.close();
  if (error) { return; }

  const auto scoreboard_array = doc.as<JsonArray>();
  for (JsonVariant score_variant : scoreboard_array)
  {
    const auto score_object = score_variant.as<JsonObject>();

    scoreboard_entry entry {};
    entry.name = score_object[ "name" ] | "anonymous";
    entry.score = score_object[ "score" ] | 0U;
    entry.lines = score_object[ "lines" ] | 0U;
    entry.level = score_object[ "level" ] | 1U;
    entry.duration_ms = score_object[ "durationMs" ] | 0U;
    entry.ended_at_ms = score_object[ "endedAtMs" ] | 0U;

    scoreboard.push_back(entry);
  }

  sort_and_trim_scoreboard();
}

inline void
add_score_from_pending(const String& player_name)
{
  if (!pending_score.ready) { return; }

  scoreboard_entry entry {};
  entry.name = sanitize_player_name(player_name);
  entry.score = pending_score.score;
  entry.lines = pending_score.lines;
  entry.level = pending_score.level;
  entry.duration_ms = pending_score.duration_ms;
  entry.ended_at_ms = pending_score.ended_at_ms;

  scoreboard.push_back(entry);
  sort_and_trim_scoreboard();
  save_scoreboard();
  pending_score.ready = false;
}

inline void
capture_pending_score(tetris::game& game, std::uint32_t now_ms)
{
  pending_score.ready = true;
  pending_score.score = game.score();
  pending_score.lines = game.lines();
  pending_score.level = game.level();
  pending_score.duration_ms = now_ms - game_started_ms;
  pending_score.ended_at_ms = now_ms;
}

inline void
restart_game(
  tetris::game& game, std::uint32_t gravity_interval_ms, std::uint32_t now_ms
)
{
  game.reset();
  game.set_gravity_interval_ms(gravity_interval_ms);
  game.reset_gravity_timer(now_ms);
  game_started_ms = now_ms;
  previous_game_over = false;
}

[[nodiscard]] inline auto
network_mode_label() -> const char*
{
  if (WiFi.status() == WL_CONNECTED) { return "station"; }
  return "access-point";
}

[[nodiscard]] inline auto
server_ip_text() -> String
{
  if (WiFi.status() == WL_CONNECTED) { return WiFi.localIP().toString(); }
  return WiFi.softAPIP().toString();
}

inline void
connect_network()
{
  const auto has_wifi_credentials = std::strlen(wifi_ssid) > 0U;
  if (has_wifi_credentials)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_password);
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
      Serial.print("WiFi connected. IP: ");
      Serial.println(WiFi.localIP());
      return;
    }
    Serial.println("WiFi STA connection failed, starting fallback AP.");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(fallback_ap_ssid);
  Serial.print("Fallback AP started. IP: ");
  Serial.println(WiFi.softAPIP());
}

inline void
mount_spiffs()
{
  if (SPIFFS.begin())
  {
    spiffs_ready = true;
    return;
  }

  Serial.println("SPIFFS mount failed. Formatting...");
  SPIFFS.format();
  spiffs_ready = SPIFFS.begin();
}

inline void
send_json_message(
  AsyncWebServerRequest* request, std::uint16_t code, bool ok,
  const char* message
)
{
  DynamicJsonDocument doc(192U);
  doc[ "ok" ] = ok;
  doc[ "message" ] = message;

  String payload {};
  serializeJson(doc, payload);
  request->send(code, "application/json", payload);
}

inline void
handle_score_submission_body(
  AsyncWebServerRequest* request, std::uint8_t* data, std::size_t len,
  std::size_t index, std::size_t total
)
{
  if (index == 0U)
  {
    auto* request_body = new String {};
    request_body->reserve(total);
    request->_tempObject = request_body;
  }

  auto* request_body = static_cast<String*>(request->_tempObject);
  if (request_body == nullptr)
  {
    send_json_message(request, 500U, false, "Body allocation failed.");
    return;
  }

  request_body->concat(reinterpret_cast<const char*>(data), len);
  if ((index + len) != total) { return; }

  std::unique_ptr<String> body_owner(request_body);
  request->_tempObject = nullptr;

  if (!pending_score.ready)
  {
    send_json_message(request, 409U, false, "No pending score to submit.");
    return;
  }

  DynamicJsonDocument doc(256U);
  const auto error = deserializeJson(doc, *body_owner);
  if (error)
  {
    send_json_message(request, 400U, false, "Invalid JSON body.");
    return;
  }

  add_score_from_pending(doc[ "name" ] | "anonymous");
  send_json_message(request, 200U, true, "Score saved.");
}

inline void
send_state(AsyncWebServerRequest* request, tetris::game& game)
{
  AsyncResponseStream* response =
    request->beginResponseStream("application/json");

  DynamicJsonDocument doc(8192U);
  doc[ "gameOver" ] = game.game_over();
  doc[ "score" ] = game.score();
  doc[ "lines" ] = game.lines();
  doc[ "level" ] = game.level();
  doc[ "nextBlock" ] = block_name_for(game.next_block());
  doc[ "activeBlock" ] = block_name_for(game.active_block());
  doc[ "heap" ] = ESP.getFreeHeap();
  doc[ "uptimeMs" ] = millis();
  doc[ "networkMode" ] = network_mode_label();
  doc[ "serverIp" ] = server_ip_text();
  doc[ "awaitingName" ] = pending_score.ready;

  if (pending_score.ready)
  {
    JsonObject pending = doc.createNestedObject("pending");
    pending[ "score" ] = pending_score.score;
    pending[ "lines" ] = pending_score.lines;
    pending[ "level" ] = pending_score.level;
    pending[ "durationMs" ] = pending_score.duration_ms;
    pending[ "endedAtMs" ] = pending_score.ended_at_ms;
  }

  JsonArray score_array = doc.createNestedArray("scoreboard");
  for (const auto& entry : scoreboard)
  {
    JsonObject row = score_array.createNestedObject();
    row[ "name" ] = entry.name;
    row[ "score" ] = entry.score;
    row[ "lines" ] = entry.lines;
    row[ "level" ] = entry.level;
    row[ "durationMs" ] = entry.duration_ms;
    row[ "endedAtMs" ] = entry.ended_at_ms;
  }

  serializeJson(doc, *response);
  request->send(response);
}

inline void
configure_server(tetris::game& game, std::uint32_t gravity_interval_ms)
{
  server.on(
    "/", HTTP_GET, [](AsyncWebServerRequest* request) -> void
    { request->send(200, "text/html; charset=utf-8", dashboard_html); }
  );

  server.on(
    "/api/state", HTTP_GET, [ &game ](AsyncWebServerRequest* request) -> void
    { send_state(request, game); }
  );

  server.on(
    "/api/restart", HTTP_POST,
    [ &game, gravity_interval_ms ](AsyncWebServerRequest* request) -> void
    {
      restart_game(game, gravity_interval_ms, millis());
      send_json_message(request, 200U, true, "Game restarted.");
    }
  );

  server.on(
    "/api/score", HTTP_POST,
    [](AsyncWebServerRequest* request) -> void
    {
      if (request->contentLength() == 0U)
      {
        send_json_message(request, 400U, false, "Missing request body.");
      }
    },
    nullptr, handle_score_submission_body
  );

  server.onNotFound(
    [](AsyncWebServerRequest* request) -> void
    { send_json_message(request, 404U, false, "Route not found."); }
  );

  server.begin();
}

} // namespace tetris_server
#endif
