#include "include/game.hpp"

#if defined(ARDUINO)

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

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

  game.set_gravity_interval_ms(gravity_interval_ms);
  game.reset_gravity_timer(millis());
  game.draw_board(tft);
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
  game.draw_board(tft);

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