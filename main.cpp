#include "include/game.hpp"

#if defined(ARDUINO)

#include "include/server.hpp"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>


#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite board_sprite(&tft);

namespace
{

tetris::game game {};

constexpr std::uint8_t button_left_pin { 32U };
constexpr std::uint8_t button_right_pin { 33U };
constexpr std::uint8_t button_rotate_pin { 25U };
constexpr std::uint8_t button_down_pin { 26U };
constexpr std::uint8_t button_drop_pin { 27U };

constexpr std::uint8_t display_rotation { 0U };
constexpr std::uint32_t gravity_interval_ms { 650U };
constexpr std::uint32_t move_repeat_ms { 120U };
constexpr std::uint32_t soft_drop_repeat_ms { 70U };
constexpr std::uint32_t frame_delay_ms { 16U };
constexpr std::int8_t preferred_board_sprite_color_depth { 16 };
constexpr std::int8_t fallback_board_sprite_color_depth { 8 };
constexpr tetris::coord_t board_sprite_origin_x_px { 1 };
constexpr tetris::coord_t board_sprite_origin_y_px { 1 };

bool board_sprite_ready { false };

struct button_repeat_state
{
  bool was_pressed { false };
  std::uint32_t last_trigger_ms { 0U };
};

button_repeat_state left_state {};
button_repeat_state right_state {};
button_repeat_state down_state {};

bool rotate_was_pressed { false };
bool drop_was_pressed { false };

[[nodiscard]] auto
is_pressed(std::uint8_t pin) -> bool
{ return digitalRead(pin) == HIGH; }

template<typename Action>
void
handle_repeat_button(
  std::uint8_t pin, std::uint32_t now_ms, std::uint32_t repeat_ms,
  button_repeat_state& state, Action action
)
{
  if (!is_pressed(pin))
  {
    state.was_pressed = false;
    return;
  }

  if (!state.was_pressed || (now_ms - state.last_trigger_ms) >= repeat_ms)
  {
    action();
    state.last_trigger_ms = now_ms;
  }
  state.was_pressed = true;
}

template<typename Action>
void
handle_edge_button(std::uint8_t pin, bool& was_pressed, Action action)
{
  const auto pressed = is_pressed(pin);
  if (pressed && !was_pressed) { action(); }
  was_pressed = pressed;
}

void
configure_button(std::uint8_t pin)
{ pinMode(pin, INPUT_PULLUP); }

void
draw_board_frame()
{
  if (!board_sprite_ready)
  {
    game.draw_board(tft);
    if (game.game_over())
    {
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawString(
        "GAME OVER", tetris::game::display_width_px / 2,
        tetris::game::display_height_px / 2, 4
      );
    }
    return;
  }

  game.draw_board(
    board_sprite, board_sprite_origin_x_px, board_sprite_origin_y_px
  );

  if (game.game_over())
  {
    board_sprite.setTextDatum(MC_DATUM);
    board_sprite.setTextColor(TFT_WHITE, TFT_RED);
    board_sprite.drawString(
      "GAME OVER", board_sprite.width() / 2, board_sprite.height() / 2, 4
    );
  }

  board_sprite.pushSprite(
    tetris::game::board_buffer_screen_x_px,
    tetris::game::board_buffer_screen_y_px
  );
}

} // namespace

void
setup()
{
  Serial.begin(115200);

  configure_button(button_left_pin);
  configure_button(button_right_pin);
  configure_button(button_rotate_pin);
  configure_button(button_down_pin);
  configure_button(button_drop_pin);

  tft.init();
  tft.setRotation(display_rotation);
  tft.fillScreen(TFT_BLACK);

  tetris_server::connect_network();
  tetris_server::mount_spiffs();
  tetris_server::load_scoreboard();
  board_sprite.setColorDepth(preferred_board_sprite_color_depth);
  board_sprite_ready =
    board_sprite.createSprite(
      static_cast<std::int16_t>(tetris::game::board_buffer_width_px),
      static_cast<std::int16_t>(tetris::game::board_buffer_height_px)
    ) != nullptr;
  if (!board_sprite_ready)
  {
    board_sprite.setColorDepth(fallback_board_sprite_color_depth);
    board_sprite_ready =
      board_sprite.createSprite(
        static_cast<std::int16_t>(tetris::game::board_buffer_width_px),
        static_cast<std::int16_t>(tetris::game::board_buffer_height_px)
      ) != nullptr;
  }

  game.set_gravity_interval_ms(gravity_interval_ms);
  tetris_server::game_started_ms = millis();
  game.reset_gravity_timer(tetris_server::game_started_ms);
  tetris_server::tetris_server::previous_game_over = game.game_over();
  draw_board_frame();

  tetris_server::configure_server(game, gravity_interval_ms);
  Serial.print("Dashboard ready at: http://");
  Serial.println(tetris_server::server_ip_text());
}

void
loop()
{
  const auto now_ms = millis();

  handle_repeat_button(
    button_left_pin, now_ms, move_repeat_ms, left_state,
    []() -> void { game.try_move(-1, 0); }
  );

  handle_repeat_button(
    button_right_pin, now_ms, move_repeat_ms, right_state,
    []() -> void { game.try_move(1, 0); }
  );

  handle_edge_button(
    button_rotate_pin, rotate_was_pressed, []() -> void { game.try_rotate(1); }
  );

  handle_repeat_button(
    button_down_pin, now_ms, soft_drop_repeat_ms, down_state,
    []() -> void { game.soft_drop(); }
  );

  handle_edge_button(
    button_drop_pin, drop_was_pressed, []() -> void { game.hard_drop(); }
  );

  game.tick(now_ms);
  const auto is_now_game_over = game.game_over();
  if (is_now_game_over && !tetris_server::previous_game_over)
  {
    tetris_server::capture_pending_score(game, now_ms);
  }
  tetris_server::previous_game_over = is_now_game_over;
  draw_board_frame();

  delay(frame_delay_ms);
}

#else

#include <cstdint>

auto
main() -> std::int32_t
{
  tetris::game game {};
  volatile std::int32_t exit_code { game.game_over() ? 1 : 0 };
  return exit_code;
}

#endif
