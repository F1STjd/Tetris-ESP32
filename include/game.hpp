#pragma once

#include "tetromino.hpp"

#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#elif defined(ESP_PLATFORM)
#include <esp_random.h>
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <type_traits>
#include <variant>

namespace tetris
{

static constexpr std::uint8_t bag_size_u8 { 7U };
static constexpr std::size_t bag_size { bag_size_u8 };
static constexpr std::uint8_t local_grid_size { 4 };

struct esp32_urbg
{
  using result_type = std::uint32_t;

  [[nodiscard]] static constexpr auto
  min() noexcept -> result_type
  { return 0U; }

  [[nodiscard]] static constexpr auto
  max() noexcept -> result_type
  { return std::numeric_limits<std::uint32_t>::max(); }

  auto
  operator()() noexcept -> result_type
  {
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP_PLATFORM)
    return esp_random();
#else
    // Deterministic fallback so host builds can compile and run.
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
#endif
  }

private:
  result_type state_ { 0xA341316CU };
};

struct game
{
public:
  static constexpr coord_t display_width_px { 240 };
  static constexpr coord_t display_height_px { 320 };
  static constexpr coord_t cell_size_px { 14 };
  static constexpr coord_t board_pixel_width_px { board_width * cell_size_px };
  static constexpr coord_t board_pixel_height_px { board_height *
    cell_size_px };
  static constexpr coord_t board_min_x_px {
    (display_width_px - board_pixel_width_px) / 2,
  };
  static constexpr coord_t board_min_y_px {
    (display_height_px - board_pixel_height_px) / 2,
  };
  static constexpr coord_t board_max_x_px {
    board_min_x_px + board_pixel_width_px - 1,
  };
  static constexpr coord_t board_max_y_px {
    board_min_y_px + board_pixel_height_px - 1,
  };

  game() noexcept
  {
    refill_bag_();
    next_ = draw_from_bag_();
    spawn_next_from_bag();
  }

  [[nodiscard]] constexpr auto
  can_place(
    const block_t& block, coord_t dx = 0, coord_t dy = 0, std::int8_t drot = 0
  ) const noexcept -> bool
  {
    return std::visit(
      [ this, dx, dy, drot ](const auto& piece) noexcept -> bool
      { return can_place_piece_(piece, dx, dy, drot); }, block
    );
  }

  constexpr auto
  try_move(coord_t dx, coord_t dy) noexcept -> bool
  {
    if (game_over_ || !can_place(active_, dx, dy, 0)) { return false; }

    std::visit(
      [ dx, dy ](auto& piece) noexcept -> void
      {
        piece.position_.x += dx;
        piece.position_.y += dy;
      },
      active_
    );
    return true;
  }

  // co to robi sie dowiedziec?
  static constexpr std::array<point_2d, 5> kick_table {
    point_2d { 0, 0 },
    point_2d { -1, 0 },
    point_2d { 1, 0 },
    point_2d { 0, 1 },
    point_2d { 0, -1 },
  };

  constexpr auto
  try_rotate(std::int8_t dir) noexcept -> bool
  {
    if (game_over_) { return false; }

    for (const auto kick : kick_table)
    {
      if (!can_place(active_, kick.x, kick.y, dir)) { continue; }

      std::visit(
        [ dir, kick ](auto& piece) noexcept -> void
        {
          using traits_t = block_traits<std::decay_t<decltype(piece)>>;
          piece.rotation =
            rotate_index_(piece.rotation, dir, traits_t::rotation_count);
          piece.position_.x += kick.x;
          piece.position_.y += kick.y;
        },
        active_
      );
      return true;
    }

    return false;
  }

  constexpr auto
  lock_active_into_floor() noexcept -> void
  {
    if (game_over_) { return; }

    std::visit(
      [ this ](const auto& piece) noexcept -> void
      { stamp_piece_to_floor_(piece); }, active_
    );

    const auto cleared_lines = clear_full_lines();
    update_score_(cleared_lines);
    hold_used_ = false;
    spawn_next_from_bag();
  }

  constexpr auto
  clear_full_lines() noexcept -> std::uint8_t
  {
    std::uint8_t cleared_lines { 0U };
    std::size_t write_y { 0U };

    for (std::size_t read_y = 0U; read_y < row_count; ++read_y)
    {
      if (floor_[ read_y ].all())
      {
        ++cleared_lines;
        continue;
      }

      if (write_y != read_y) { floor_[ write_y ] = floor_[ read_y ]; }
      ++write_y;
    }

    for (; write_y < row_count; ++write_y)
    {
      floor_[ write_y ].reset();
    }

    return cleared_lines;
  }

  constexpr auto
  spawn_next_from_bag() noexcept -> void
  {
    active_ = next_;
    next_ = draw_from_bag_();
    hold_used_ = false;
    if (!can_place(active_)) { game_over_ = true; }
  }

  auto
  set_gravity_interval_ms(std::uint32_t interval_ms) noexcept -> void
  {
    if (interval_ms == 0U) { return; }
    gravity_interval_ms_ = interval_ms;
  }

  auto
  reset_gravity_timer(std::uint32_t now_ms) noexcept -> void
  { last_gravity_ms_ = now_ms; }

  auto
  tick(std::uint32_t now_ms) noexcept -> void
  {
    if (game_over_) { return; }
    if ((now_ms - last_gravity_ms_) < gravity_interval_ms_) { return; }

    last_gravity_ms_ = now_ms;
    if (!try_move(0, -1)) { lock_active_into_floor(); }
  }

  auto
  soft_drop() noexcept -> void
  {
    if (game_over_) { return; }
    if (!try_move(0, -1)) { lock_active_into_floor(); }
  }

  auto
  hard_drop() noexcept -> void
  {
    if (game_over_) { return; }
    while (try_move(0, -1)) {}
    lock_active_into_floor();
  }

  [[nodiscard]] constexpr auto
  active_block() const noexcept -> const block_t&
  { return active_; }

  [[nodiscard]] constexpr auto
  next_block() const noexcept -> const block_t&
  { return next_; }

  [[nodiscard]] constexpr auto
  floor() const noexcept -> const floor_t&
  { return floor_; }

  [[nodiscard]] constexpr auto
  game_over() const noexcept -> bool
  { return game_over_; }

  template<typename Display>
  auto
  draw_board(Display& display) const -> void
  {
    draw_board_frame(display);
    draw_floor(display);
    draw_block(display, active_);
  }

  template<typename Display>
  auto
  draw_board_frame(Display& display) const -> void
  {
    display.fillRect(
      board_min_x_px, board_min_y_px, board_pixel_width_px,
      board_pixel_height_px, color_board_background_
    );
    display.drawRect(
      board_min_x_px - 1, board_min_y_px - 1, board_pixel_width_px + 2,
      board_pixel_height_px + 2, color_border_
    );

    for (coord_t x = 1; x < board_width; ++x)
    {
      const auto pixel_x = board_min_x_px + (x * cell_size_px);
      display.drawFastVLine(
        pixel_x, board_min_y_px, board_pixel_height_px, color_grid_
      );
    }

    for (coord_t y = 1; y < board_height; ++y)
    {
      const auto pixel_y = board_min_y_px + (y * cell_size_px);
      display.drawFastHLine(
        board_min_x_px, pixel_y, board_pixel_width_px, color_grid_
      );
    }
  }

  template<typename Display>
  auto
  draw_floor(Display& display) const -> void
  {
    for (std::size_t y = 0U; y < row_count; ++y)
    {
      for (std::size_t x = 0U; x < column_count; ++x)
      {
        if (!floor_[ y ].test(x)) { continue; }
        draw_cell_(
          display, static_cast<coord_t>(x), static_cast<coord_t>(y),
          color_locked_
        );
      }
    }
  }

  template<typename Display>
  auto
  draw_block(Display& display, const block_t& block) const -> void
  {
    std::visit(
      [ this, &display ](const auto& piece) -> void
      { draw_piece_(display, piece, block_color_(piece)); }, block
    );
  }

private:
  static constexpr std::uint16_t color_board_background_ { 0x0000U };
  static constexpr std::uint16_t color_border_ { 0xFFFFU };
  static constexpr std::uint16_t color_grid_ { 0x2124U };
  static constexpr std::uint16_t color_cell_outline_ { 0x7BEFU };
  static constexpr std::uint16_t color_locked_ { 0x39E7U };

  [[nodiscard]] static constexpr auto
  inside_board_(coord_t x, coord_t y) noexcept -> bool
  { return x >= 0 && x < board_width && y >= 0 && y < board_height; }

  [[nodiscard]] static constexpr auto
  board_to_screen_x_(coord_t board_x) noexcept -> coord_t
  { return board_min_x_px + (board_x * cell_size_px); }

  [[nodiscard]] static constexpr auto
  board_to_screen_y_(coord_t board_y) noexcept -> coord_t
  { return board_min_y_px + ((board_height - 1 - board_y) * cell_size_px); }

  template<typename Display>
  static auto
  draw_cell_(
    Display& display, coord_t board_x, coord_t board_y, std::uint16_t fill_color
  ) -> void
  {
    if (!inside_board_(board_x, board_y)) { return; }

    const auto pixel_x = board_to_screen_x_(board_x);
    const auto pixel_y = board_to_screen_y_(board_y);
    display.fillRect(
      pixel_x + 1, pixel_y + 1, cell_size_px - 2, cell_size_px - 2, fill_color
    );
    display.drawRect(
      pixel_x, pixel_y, cell_size_px, cell_size_px, color_cell_outline_
    );
  }

  [[nodiscard]] static constexpr auto
  block_color_(const block::O& /*unused*/) noexcept -> std::uint16_t
  { return 0xFFE0U; }

  [[nodiscard]] static constexpr auto
  block_color_(const block::I& /*unused*/) noexcept -> std::uint16_t
  { return 0x07FFU; }

  [[nodiscard]] static constexpr auto
  block_color_(const block::S& /*unused*/) noexcept -> std::uint16_t
  { return 0x07E0U; }

  [[nodiscard]] static constexpr auto
  block_color_(const block::Z& /*unused*/) noexcept -> std::uint16_t
  { return 0xF800U; }

  [[nodiscard]] static constexpr auto
  block_color_(const block::J& /*unused*/) noexcept -> std::uint16_t
  { return 0x001FU; }

  [[nodiscard]] static constexpr auto
  block_color_(const block::L& /*unused*/) noexcept -> std::uint16_t
  { return 0xFD20U; }

  [[nodiscard]] static constexpr auto
  block_color_(const block::T& /*unused*/) noexcept -> std::uint16_t
  { return 0xF81FU; }

  template<typename Display, typename Piece>
  auto
  draw_piece_(
    Display& display, const Piece& piece, std::uint16_t fill_color
  ) const -> void
  {
    using traits_t = block_traits<Piece>;
    const auto mask =
      traits_t::masks[ piece.rotation % traits_t::rotation_count ];

    for (std::uint8_t local_y = 0U; local_y < local_grid_size; ++local_y)
    {
      for (std::uint8_t local_x = 0U; local_x < local_grid_size; ++local_x)
      {
        if (!mask_cell(mask, local_x, local_y)) { continue; }
        draw_cell_(
          display, piece.position_.x + local_x, piece.position_.y + local_y,
          fill_color
        );
      }
    }
  }

  static constexpr auto
  rotate_index_(
    rotation_t current, std::int8_t delta, rotation_t rotation_count
  ) noexcept -> rotation_t
  {
    std::int32_t index { current };
    index += static_cast<std::int32_t>(delta);
    const auto count { static_cast<std::int32_t>(rotation_count) };
    index %= count;
    if (index < 0) { index += count; }
    return static_cast<rotation_t>(index);
  }

  template<typename Piece>
  [[nodiscard]] constexpr auto
  can_place_piece_(
    const Piece& piece, coord_t dx, coord_t dy, std::int8_t drot
  ) const noexcept -> bool
  {
    using traits_t = block_traits<Piece>;
    const auto rotated =
      rotate_index_(piece.rotation, drot, traits_t::rotation_count);
    const auto mask = traits_t::masks[ rotated ];

    for (std::uint8_t local_y = 0; local_y < local_grid_size; ++local_y)
    {
      for (std::uint8_t local_x = 0; local_x < local_grid_size; ++local_x)
      {
        if (!mask_cell(mask, local_x, local_y)) { continue; }

        const auto world_x = piece.position_.x + dx + local_x;
        const auto world_y = piece.position_.y + dy + local_y;

        if (
          world_x < 0 || world_x >= board_width || world_y < 0 ||
          world_y >= board_height
        )
        {
          return false;
        }

        const auto x_index = static_cast<std::size_t>(world_x);
        const auto y_index = static_cast<std::size_t>(world_y);
        if (floor_[ y_index ].test(x_index)) { return false; }
      }
    }

    return true;
  }

  template<typename Piece>
  constexpr auto
  stamp_piece_to_floor_(const Piece& piece) noexcept -> void
  {
    using traits_t = block_traits<Piece>;
    const auto mask =
      traits_t::masks[ piece.rotation % traits_t::rotation_count ];

    for (std::uint8_t local_y = 0; local_y < local_grid_size; ++local_y)
    {
      for (std::uint8_t local_x = 0; local_x < local_grid_size; ++local_x)
      {
        if (!mask_cell(mask, local_x, local_y)) { continue; }

        const auto world_x = piece.position_.x + local_x;
        const auto world_y = piece.position_.y + local_y;

        if (
          world_x >= 0 && world_x < board_width && world_y >= 0 &&
          world_y < board_height
        )
        {
          const auto x_index = static_cast<std::size_t>(world_x);
          const auto y_index = static_cast<std::size_t>(world_y);
          floor_[ y_index ].set(x_index);
        }
      }
    }
  }

  static constexpr auto
  make_piece_from_id_(std::uint8_t id) noexcept -> block_t
  {
    switch (id)
    {
    case 0:
      return block::O {};
    case 1:
      return block::I {};
    case 2:
      return block::S {};
    case 3:
      return block::Z {};
    case 4:
      return block::J {};
    case 5:
      return block::L {};
    case 6:
    default:
      return block::T {};
    }
  }

  auto
  refill_bag_() noexcept -> void
  {
    std::iota(bag_.begin(), bag_.end(), std::uint8_t {});
    std::shuffle(bag_.begin(), bag_.end(), rng_);
    bag_index_ = 0;
  }

  constexpr auto
  draw_from_bag_() noexcept -> block_t
  {
    if (bag_index_ >= bag_size_u8) { refill_bag_(); }
    const auto id = bag_[ bag_index_++ ];
    return make_piece_from_id_(id);
  }

  constexpr auto
  update_score_(std::uint8_t cleared_lines) noexcept -> void
  {
    lines_ += cleared_lines;

    switch (cleared_lines)
    {
    case 1:
      score_ += 100U * level_;
      break;
    case 2:
      score_ += 300U * level_;
      break;
    case 3:
      score_ += 500U * level_;
      break;
    case 4:
      score_ += 800U * level_;
      break;
    default:
      break;
    }

    level_ = 1U + (lines_ / 10U);
  }

  tetris::floor_t floor_ {};
  std::array<std::uint8_t, bag_size> bag_ {};
  std::optional<tetris::block_t> hold_ { std::nullopt };
  tetris::block_t active_;
  tetris::block_t next_;
  std::uint32_t last_gravity_ms_ { 0 };
  std::uint32_t gravity_interval_ms_ { 700 };
  std::uint32_t score_ {};
  std::uint32_t lines_ {};
  std::uint32_t level_ { 1 };
  std::uint8_t bag_index_ { bag_size_u8 };
  esp32_urbg rng_ {};
  bool hold_used_ { false };
  bool game_over_ { false };
};

} // namespace tetris