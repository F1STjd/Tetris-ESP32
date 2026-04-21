#include "include/game.hpp"

#if defined(ARDUINO)

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

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
bool has_last_render_signature { false };
std::uint32_t last_render_signature { 0U };

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
mix_signature(std::uint32_t& signature, std::uint32_t value)
{ signature ^= value + 0x9E3779B9U + (signature << 6U) + (signature >> 2U); }

[[nodiscard]] auto
board_state_signature() -> std::uint32_t
{
  std::uint32_t signature { 2166136261U };

  mix_signature(signature, static_cast<std::uint32_t>(game.game_over()));
  mix_signature(
    signature, static_cast<std::uint32_t>(game.active_block().index())
  );

  std::visit(
    [ &signature ](const auto& piece) -> void
    {
      mix_signature(signature, static_cast<std::uint32_t>(piece.position_.x));
      mix_signature(signature, static_cast<std::uint32_t>(piece.position_.y));
      mix_signature(signature, static_cast<std::uint32_t>(piece.rotation));
    },
    game.active_block()
  );

  for (const auto& row : game.floor())
  {
    mix_signature(signature, static_cast<std::uint32_t>(row.to_ulong()));
  }

  return signature;
}

void
draw_board_frame()
{
  if (!board_sprite_ready)
  {
    game.draw_board(tft);
    return;
  }

  game.draw_board(
    board_sprite, board_sprite_origin_x_px, board_sprite_origin_y_px
  );
  board_sprite.pushSprite(
    tetris::game::board_buffer_screen_x_px,
    tetris::game::board_buffer_screen_y_px
  );
}

void
draw_board_frame_if_changed()
{
  const auto signature = board_state_signature();
  if (has_last_render_signature && signature == last_render_signature)
  {
    return;
  }

  draw_board_frame();
  last_render_signature = signature;
  has_last_render_signature = true;
}

} // namespace

void
setup()
{
  configure_button(button_left_pin);
  configure_button(button_right_pin);
  configure_button(button_rotate_pin);
  configure_button(button_down_pin);
  configure_button(button_drop_pin);

  tft.init();
  tft.setRotation(display_rotation);
  tft.fillScreen(TFT_BLACK);

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
  game.reset_gravity_timer(millis());
  draw_board_frame_if_changed();
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
  draw_board_frame_if_changed();

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