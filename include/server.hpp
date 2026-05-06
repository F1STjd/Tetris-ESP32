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
#include <ranges>
#include <utility>
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
<html lang="pl">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Panel Tetris ESP32</title>
  <link rel="preconnect" href="https://fonts.googleapis.com" />
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin />
  <link href="https://fonts.googleapis.com/css2?family=Press+Start+2P&family=VT323&display=swap" rel="stylesheet" />
  <style>
    :root {
      --bg-0: #0a0f16;
      --bg-1: #0d1520;
      --card: #1a2430;
      --card-2: #141c28;
      --edge: #2b3642;
      --text: #d9e1f0;
      --muted: #94a0b8;
      --neon: #b8ff86;
      --cyan: #7ad9ff;
      --amber: #ffcd7a;
      --danger: #ff7b7b;
      --shell-0: #9aa287;
      --shell-1: #7f886f;
      --shell-2: #626954;
      --screen-0: #0f1b16;
      --screen-1: #0b1411;
      --screen-grid: #16231f;
      --panel-0: #151e2b;
      --panel-1: #0f1622;
      --cell-size: 22px;
      --cell-gap: 5px;
    }

    body {
      font-family: 'VT323', ui-monospace, monospace;
      margin: 0;
      color: var(--text);
      background:
        radial-gradient(900px 450px at 10% -10%, rgba(122, 217, 255, 0.12), transparent 60%),
        radial-gradient(900px 450px at 110% 10%, rgba(184, 255, 134, 0.1), transparent 55%),
        linear-gradient(180deg, var(--bg-1), var(--bg-0));
    }

    .wrap {
      max-width: 1120px;
      margin: 0 auto;
      padding: 24px 20px 36px;
    }

    h1 {
      margin-top: 0;
      font-family: 'Press Start 2P', cursive;
      font-size: 1.25rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      color: var(--neon);
      text-shadow: 0 0 12px rgba(184, 255, 134, 0.35);
    }

    h2 {
      font-family: 'Press Start 2P', cursive;
      font-size: 0.8rem;
      text-transform: uppercase;
      letter-spacing: 0.12em;
      color: var(--cyan);
      text-shadow: 0 0 10px rgba(122, 217, 255, 0.35);
      margin-top: 22px;
    }

    #network {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 6px 10px;
      border-radius: 8px;
      background: rgba(9, 14, 20, 0.65);
      border: 1px solid rgba(122, 217, 255, 0.25);
      box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.05);
      text-transform: uppercase;
      letter-spacing: 0.06em;
    }

    .device {
      margin-top: 18px;
      padding: 26px 26px 30px;
      border-radius: 34px;
      background: linear-gradient(150deg, var(--shell-0), var(--shell-1));
      border: 2px solid rgba(0, 0, 0, 0.28);
      box-shadow: inset 0 2px 0 rgba(255, 255, 255, 0.35), inset 0 -4px 0 rgba(0, 0, 0, 0.35), 0 22px 45px rgba(0, 0, 0, 0.6);
      position: relative;
      overflow: hidden;
    }

    .device::before {
      content: '';
      position: absolute;
      inset: 10px;
      border-radius: 26px;
      border: 1px solid rgba(255, 255, 255, 0.18);
      pointer-events: none;
    }

    .device::after {
      content: '';
      position: absolute;
      right: 24px;
      bottom: 28px;
      width: 110px;
      height: 44px;
      border-radius: 12px;
      background: repeating-linear-gradient(90deg, rgba(0, 0, 0, 0.25), rgba(0, 0, 0, 0.25) 6px, transparent 6px, transparent 12px);
      opacity: 0.4;
    }

    .device-top {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 16px;
      text-transform: uppercase;
      letter-spacing: 0.16em;
      font-family: 'Press Start 2P', cursive;
      font-size: 0.6rem;
      color: #2b352d;
    }

    .device-title { color: #2b352d; text-shadow: 0 1px 0 rgba(255, 255, 255, 0.35); }
    .device-chip { color: #3b4537; }

    .device-layout {
      display: flex;
      justify-content: center;
    }

    .screen {
      width: 100%;
      background: linear-gradient(180deg, var(--screen-0), var(--screen-1));
      border-radius: 18px;
      border: 3px solid rgba(35, 45, 40, 0.9);
      padding: 16px;
      box-shadow: inset 0 0 0 2px rgba(0, 0, 0, 0.45), inset 0 0 18px rgba(0, 0, 0, 0.65);
    }

    .screen-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      font-size: 0.5rem;
      letter-spacing: 0.3em;
      color: rgba(210, 225, 210, 0.65);
      text-transform: uppercase;
      margin-bottom: 10px;
    }

    .screen-led {
      width: 8px;
      height: 8px;
      border-radius: 999px;
      background: #ff5757;
      box-shadow: 0 0 8px rgba(255, 87, 87, 0.7);
    }

    .screen-inner {
      width: 100%;
      display: grid;
      gap: 14px;
    }

    .screen-grid {
      grid-template-columns: minmax(180px, 0.9fr) minmax(260px, 1.2fr) minmax(180px, 0.9fr);
      align-items: stretch;
    }

    .screen-left,
    .screen-center,
    .screen-right {
      display: flex;
      flex-direction: column;
      gap: 10px;
    }

    .screen-left {
      justify-self: start;
      width: 100%;
      max-width: 220px;
    }

    .screen-center {
      align-items: center;
      width: 100%;
    }

    .screen-right {
      justify-self: end;
      width: 100%;
      max-width: 220px;
    }

    .screen-panel {
      background: rgba(7, 12, 12, 0.85);
      border: 1px solid rgba(184, 255, 134, 0.25);
      border-radius: 12px;
      padding: 10px 10px 12px;
      text-align: center;
      box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.04);
      width: 100%;
      max-width: 280px;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(170px, 1fr));
      gap: 12px;
      margin-top: 12px;
    }

    .card {
      position: relative;
      background: linear-gradient(150deg, var(--card), var(--card-2));
      border: 1px solid rgba(184, 255, 134, 0.18);
      border-radius: 12px;
      padding: 14px 14px 12px;
      box-shadow: 0 10px 26px rgba(0, 0, 0, 0.45), 0 0 0 1px rgba(184, 255, 134, 0.12);
      overflow: hidden;
      animation: card-in 520ms ease both;
    }

    .card::after {
      content: '';
      position: absolute;
      inset: 0;
      background: repeating-linear-gradient(180deg, rgba(255, 255, 255, 0.03), rgba(255, 255, 255, 0.03) 1px, transparent 1px, transparent 3px);
      opacity: 0.35;
      pointer-events: none;
    }

    .label {
      font-family: 'Press Start 2P', cursive;
      font-size: 0.52rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      color: var(--muted);
    }

    .value {
      font-size: 1.7rem;
      font-weight: 700;
      text-shadow: 0 0 10px rgba(184, 255, 134, 0.35);
    }

    .value.small { font-size: 1.1rem; text-shadow: none; }
    .muted.tiny { font-size: 0.85rem; }

    .stat-chip {
      background: linear-gradient(180deg, rgba(9, 14, 12, 0.85), rgba(6, 10, 9, 0.9));
      border: 1px solid rgba(122, 217, 255, 0.15);
      border-radius: 10px;
      padding: 8px 8px 10px;
      text-align: right;
      box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.04);
    }

    .timer-chip { text-align: center; }

    .stat-label {
      font-family: 'Press Start 2P', cursive;
      font-size: 0.5rem;
      letter-spacing: 0.12em;
      text-transform: uppercase;
      color: rgba(210, 225, 210, 0.7);
    }

    .stat-value {
      font-size: 1.35rem;
      color: var(--neon);
      text-shadow: 0 0 8px rgba(184, 255, 134, 0.35);
    }

    .timer-value {
      font-size: 1.45rem;
      letter-spacing: 0.12em;
      color: var(--neon);
      text-shadow: 0 0 10px rgba(184, 255, 134, 0.35);
    }

    .stat-value.small { font-size: 1.05rem; text-shadow: none; color: var(--cyan); }
    .stat-sub { font-size: 0.85rem; color: rgba(210, 225, 210, 0.65); }

    #score { color: var(--neon); }
    #lines { color: var(--cyan); }
    #level { color: var(--amber); }
    .state-running { color: var(--neon); }
    .state-over { color: var(--danger); }

    .row { display: flex; gap: 8px; align-items: center; flex-wrap: wrap; }

    button, input {
      border-radius: 10px;
      border: 1px solid rgba(184, 255, 134, 0.25);
      background: rgba(10, 16, 16, 0.85);
      color: var(--text);
      padding: 8px 12px;
      font-family: 'VT323', ui-monospace, monospace;
      font-size: 1.05rem;
      letter-spacing: 0.04em;
      transition: transform 160ms ease, box-shadow 160ms ease, border-color 160ms ease;
    }

    button { cursor: pointer; }
    button:hover { transform: translateY(-1px); box-shadow: 0 6px 16px rgba(184, 255, 134, 0.25); border-color: rgba(184, 255, 134, 0.55); }
    button:active { transform: translateY(0); box-shadow: none; }

    table { width: 100%; border-collapse: collapse; margin-top: 12px; background: rgba(7, 12, 18, 0.65); border: 1px solid rgba(122, 217, 255, 0.2); }
    th, td { text-align: left; padding: 8px; border-bottom: 1px solid rgba(122, 217, 255, 0.12); }
    th { text-transform: uppercase; letter-spacing: 0.12em; font-size: 0.65rem; color: var(--cyan); }
    tbody tr:hover { background: rgba(122, 217, 255, 0.08); }

    #score-form { display: none; margin-top: 12px; }
    .muted { opacity: 0.85; font-size: 1rem; color: var(--muted); }

    .next-preview { display: flex; align-items: center; justify-content: center; min-height: 148px; margin-top: 6px; }
    .mini-grid {
      display: grid;
      grid-template-columns: repeat(4, var(--cell-size));
      grid-template-rows: repeat(4, var(--cell-size));
      gap: var(--cell-gap);
      padding: 12px;
      background: rgba(8, 14, 12, 0.75);
      border: 1px solid rgba(184, 255, 134, 0.2);
      border-radius: 14px;
    }
    .cell {
      width: var(--cell-size);
      height: var(--cell-size);
      border-radius: 6px;
      background: #0b1411;
      border: 1px solid rgba(40, 55, 47, 0.9);
      box-shadow: inset 0 0 0 1px rgba(255, 255, 255, 0.04);
    }
    .cell.filled { box-shadow: 0 0 10px rgba(255, 255, 255, 0.18), inset 0 0 0 1px rgba(255, 255, 255, 0.2); }
    .next-raw { display: none; }

    .device-controls {
      margin-top: 18px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      padding: 0 18px;
    }

    .dpad {
      width: 68px;
      height: 68px;
      border-radius: 16px;
      background: linear-gradient(150deg, #2f3531, #1e2421);
      box-shadow: inset 0 2px 4px rgba(255, 255, 255, 0.2), inset 0 -4px 6px rgba(0, 0, 0, 0.45);
      position: relative;
    }

    .dpad::before,
    .dpad::after {
      content: '';
      position: absolute;
      background: #151a18;
      border-radius: 6px;
      box-shadow: inset 0 2px 3px rgba(255, 255, 255, 0.08);
    }

    .dpad::before {
      width: 42px;
      height: 14px;
      top: 27px;
      left: 13px;
    }

    .dpad::after {
      width: 14px;
      height: 42px;
      top: 13px;
      left: 27px;
    }

    .button-group {
      display: flex;
      gap: 14px;
      align-items: center;
    }

    .button {
      width: 38px;
      height: 38px;
      border-radius: 999px;
      display: grid;
      place-items: center;
      font-family: 'Press Start 2P', cursive;
      font-size: 0.55rem;
      color: rgba(255, 255, 255, 0.8);
      background: linear-gradient(150deg, #b25a7f, #8e3f64);
      box-shadow: inset 0 2px 4px rgba(255, 255, 255, 0.2), inset 0 -4px 6px rgba(0, 0, 0, 0.45), 0 6px 12px rgba(0, 0, 0, 0.35);
    }

    .button.b { background: linear-gradient(150deg, #a1486d, #7d3657); }
    .button.a { background: linear-gradient(150deg, #c06895, #98466f); }

    .scoreboard {
      margin-top: 22px;
      padding: 16px 16px 18px;
      border-radius: 16px;
      background: rgba(7, 12, 18, 0.75);
      border: 1px solid rgba(122, 217, 255, 0.2);
      box-shadow: 0 12px 26px rgba(0, 0, 0, 0.45);
    }

    .gameover-banner {
      display: none;
      align-items: center;
      justify-content: center;
      gap: 10px;
      padding: 10px 12px;
      border-radius: 12px;
      background: rgba(10, 14, 20, 0.85);
      border: 1px solid rgba(255, 123, 123, 0.4);
      color: var(--danger);
      font-family: 'Press Start 2P', cursive;
      font-size: 0.8rem;
      letter-spacing: 0.2em;
      margin-bottom: 12px;
      text-transform: uppercase;
    }

    .gameover-banner.is-visible { display: flex; }

    @keyframes card-in {
      from { opacity: 0; transform: translateY(8px); }
      to { opacity: 1; transform: translateY(0); }
    }

    @media (prefers-reduced-motion: reduce) {
      * { animation: none !important; transition: none !important; }
    }

    @media (max-width: 640px) {
      h1 { font-size: 0.95rem; }
      .stat-value { font-size: 1.2rem; }
      .wrap { padding: 18px 14px 26px; }
    }

    @media (max-width: 900px) {
      .screen-grid { grid-template-columns: 1fr; }
      .device-controls { padding: 0; }
      .device::after { display: none; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <h1>TETRIS FOR ESP32</h1>
    <p class="muted" id="network">Status: Laczenie...</p>

    <section class="device">
      <div class="device-top">
        <span class="device-title">TETRIS FOR ESP32</span>
        <span class="device-chip">DOT MATRIX</span>
      </div>
      <div class="device-layout">
        <div class="screen">
          <div class="screen-header">
            <span>DOT MATRIX DISPLAY</span>
            <span class="screen-led"></span>
          </div>
          <div class="screen-inner screen-grid">
            <div class="screen-left">
              <div class="stat-chip timer-chip">
                <div class="stat-label">Timer</div>
                <div class="timer-value" id="timer">00:00:00</div>
              </div>
            </div>
            <div class="screen-center">
              <div class="screen-panel">
                <div class="label">Teraz</div>
                <div class="next-preview" id="current-preview"></div>
                <div class="muted next-raw" id="current-raw">-</div>
              </div>
              <div class="screen-panel">
                <div class="label">Nastepny</div>
                <div class="next-preview" id="next-preview"></div>
                <div class="muted next-raw" id="next-raw">-</div>
              </div>
            </div>
            <div class="screen-right">
              <div class="stat-chip">
                <div class="stat-label">Wynik</div>
                <div class="stat-value" id="score">0</div>
              </div>
              <div class="stat-chip">
                <div class="stat-label">Wyczyszczone linie</div>
                <div class="stat-value" id="lines">0</div>
              </div>
              <div class="stat-chip">
                <div class="stat-label">Poziom</div>
                <div class="stat-value" id="level">1</div>
              </div>
              <div class="stat-chip">
                <div class="stat-label">Stan gry</div>
                <div class="stat-value state-running" id="state">GRA TRWA</div>
              </div>
              <div class="stat-chip">
                <div class="stat-label">Siec</div>
                <div class="stat-value small" id="network-mode">-</div>
                <div class="stat-sub" id="server-ip">-</div>
              </div>
            </div>
          </div>
        </div>
      </div>
      <div class="device-controls" aria-hidden="true">
        <div class="dpad"></div>
        <div class="button-group">
          <div class="button b">B</div>
          <div class="button a">A</div>
        </div>
      </div>
    </section>

    <form id="score-form" class="row">
      <span>Koniec gry. Zapisz wynik:</span>
      <input id="player-name" type="text" maxlength="20" placeholder="Imie gracza" />
      <button type="submit">Zapisz wynik</button>
    </form>

    <div class="row" style="margin-top:10px;">
      <button id="restart-btn" type="button">Restart gry</button>
      <span class="muted" id="pending"></span>
    </div>

    <section id="scoreboard" class="scoreboard">
      <div id="gameover-banner" class="gameover-banner" aria-hidden="true">
        <span>GAME OVER</span>
      </div>
      <h2>Tabela wynikow</h2>
      <table>
        <thead><tr><th>#</th><th>Nazwa</th><th>Wynik</th><th>Linie</th><th>Poziom</th><th>Czas (s)</th></tr></thead>
        <tbody id="scores-body"></tbody>
      </table>
    </section>
  </div>

  <script>
    const scoreEl = document.getElementById('score');
    const linesEl = document.getElementById('lines');
    const levelEl = document.getElementById('level');
    const currentPreviewEl = document.getElementById('current-preview');
    const currentRawEl = document.getElementById('current-raw');
    const nextPreviewEl = document.getElementById('next-preview');
    const nextRawEl = document.getElementById('next-raw');
    const timerEl = document.getElementById('timer');
    const stateEl = document.getElementById('state');
    const networkEl = document.getElementById('network');
    const networkModeEl = document.getElementById('network-mode');
    const serverIpEl = document.getElementById('server-ip');
    const pendingEl = document.getElementById('pending');
    const scoresBody = document.getElementById('scores-body');
    const scoreForm = document.getElementById('score-form');
    const playerName = document.getElementById('player-name');
    const restartBtn = document.getElementById('restart-btn');
    const scoreboardEl = document.getElementById('scoreboard');
    const gameoverBannerEl = document.getElementById('gameover-banner');

    let prevGameOver = false;

    const NEXT_SHAPES = {
      I: ['0000', '1111', '0000', '0000'],
      O: ['0110', '0110', '0000', '0000'],
      T: ['0100', '1110', '0000', '0000'],
      S: ['0110', '1100', '0000', '0000'],
      Z: ['1100', '0110', '0000', '0000'],
      J: ['1000', '1110', '0000', '0000'],
      L: ['0010', '1110', '0000', '0000']
    };
    const NEXT_COLORS = {
      I: '#46d8ff',
      O: '#ffd166',
      T: '#c77dff',
      S: '#80ed99',
      Z: '#ff6b6b',
      J: '#5aa9ff',
      L: '#ffa94d'
    };

    function formatTimer(ms) {
      if (!Number.isFinite(ms)) { return '--:--:--'; }
      const totalCs = Math.floor(ms / 10);
      const cs = totalCs % 100;
      const totalSec = Math.floor(totalCs / 100);
      const sec = totalSec % 60;
      const min = Math.floor(totalSec / 60);
      return String(min).padStart(2, '0') + ':' +
        String(sec).padStart(2, '0') + ':' +
        String(cs).padStart(2, '0');
    }

    function renderBlock(previewEl, rawEl, type) {
      const key = (type || '').toString().trim().toUpperCase();
      rawEl.textContent = key || '-';
      previewEl.textContent = '';

      const grid = document.createElement('div');
      grid.className = 'mini-grid';
      const matrix = NEXT_SHAPES[key];
      const color = NEXT_COLORS[key] || '#8b97b3';

      for (let row = 0; row < 4; row += 1) {
        for (let col = 0; col < 4; col += 1) {
          const cell = document.createElement('div');
          cell.className = 'cell';
          if (matrix && matrix[row][col] === '1') {
            cell.classList.add('filled');
            cell.style.background = color;
            cell.style.borderColor = 'rgba(255, 255, 255, 0.2)';
          }
          grid.appendChild(cell);
        }
      }

      previewEl.appendChild(grid);
    }

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
        const currentType = state.currentBlock || state.activeBlock || state.block || null;
        renderBlock(currentPreviewEl, currentRawEl, currentType);
        renderBlock(nextPreviewEl, nextRawEl, state.nextBlock);
        if (timerEl) {
          timerEl.textContent = formatTimer(state.uptimeMs);
        }
        const isOver = Boolean(state.gameOver);
        stateEl.textContent = isOver ? 'KONIEC GRY' : 'GRA TRWA';
        stateEl.classList.toggle('state-over', isOver);
        stateEl.classList.toggle('state-running', !isOver);
        networkModeEl.textContent = state.networkMode ? 'Tryb: ' + state.networkMode : '-';
        serverIpEl.textContent = state.serverIp ? 'IP: ' + state.serverIp : '-';
        networkEl.textContent = 'Status: Polaczono';

        if (state.awaitingName) {
          scoreForm.style.display = 'flex';
          pendingEl.textContent =
            'Do zapisania: ' + state.pending.score + ' (' +
            Math.floor(state.pending.durationMs / 1000) + ' s)';
        } else {
          scoreForm.style.display = 'none';
          pendingEl.textContent = '';
        }

        if (isOver && !prevGameOver && scoreboardEl) {
          scoreboardEl.scrollIntoView({ behavior: 'smooth', block: 'start' });
        }
        if (gameoverBannerEl) {
          gameoverBannerEl.classList.toggle('is-visible', isOver);
          gameoverBannerEl.setAttribute('aria-hidden', isOver ? 'false' : 'true');
        }
        prevGameOver = isOver;

        renderScores(state.scoreboard || []);
      } catch (error) {
        networkEl.textContent = 'Status: Brak polaczenia. Ponawiam...';
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

constexpr void
sort_and_trim_scoreboard()
{
  std::ranges::sort(
    scoreboard,
    [](const scoreboard_entry& lhs, const scoreboard_entry& rhs) -> bool
    {
      if (lhs.score != rhs.score) { return lhs.score > rhs.score; }
      if (lhs.lines != rhs.lines) { return lhs.lines > rhs.lines; }
      return lhs.ended_at_ms > rhs.ended_at_ms;
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
