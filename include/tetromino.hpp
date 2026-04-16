#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace tetris
{

using coord_t = std::int32_t;
using rotation_t = std::uint8_t;

static constexpr std::size_t column_count { 10U };
static constexpr std::size_t row_count { 20U };
static constexpr coord_t board_width { 10 };
static constexpr coord_t board_height { 20 };
static constexpr coord_t spawn_x { 3 };
static constexpr coord_t spawn_y { 16 };

struct point_2d
{
  coord_t x;
  coord_t y;
};

namespace block
{

struct O
{
  point_2d position_ { spawn_x, spawn_y };
  rotation_t rotation { 0U };
};

struct I
{
  point_2d position_ { spawn_x, spawn_y };
  rotation_t rotation { 0U };
};

struct S
{
  point_2d position_ { spawn_x, spawn_y };
  rotation_t rotation { 0U };
};

struct Z
{
  point_2d position_ { spawn_x, spawn_y };
  rotation_t rotation { 0U };
};

struct J
{
  point_2d position_ { spawn_x, spawn_y };
  rotation_t rotation { 0U };
};

struct L
{
  point_2d position_ { spawn_x, spawn_y };
  rotation_t rotation { 0U };
};

struct T
{
  point_2d position_ { spawn_x, spawn_y };
  rotation_t rotation { 0U };
};

} // namespace block

template<typename T>
struct block_traits;

template<>
struct block_traits<block::O>
{
  static constexpr rotation_t rotation_count { 1U };
  static constexpr std::array<std::uint16_t, rotation_count> masks {
    0b0000'0110'0110'0000,
  };
};

template<>
struct block_traits<block::I>
{
  static constexpr rotation_t rotation_count { 2U };
  static constexpr std::array<std::uint16_t, rotation_count> masks {
    0b0100'0100'0100'0100,
    0b0000'1111'0000'0000,
  };
};

template<>
struct block_traits<block::S>
{
  static constexpr rotation_t rotation_count { 2U };
  static constexpr std::array<std::uint16_t, rotation_count> masks {
    0b0000'0011'0110'0000,
    0b0100'0110'0010'0000,
  };
};

template<>
struct block_traits<block::Z>
{
  static constexpr rotation_t rotation_count { 2U };
  static constexpr std::array<std::uint16_t, rotation_count> masks {
    0b0000'1100'0110'0000,
    0b0010'0110'0100'0000,
  };
};

template<>
struct block_traits<block::J>
{
  static constexpr rotation_t rotation_count { 4U };
  static constexpr std::array<std::uint16_t, rotation_count> masks {
    0b0010'0010'0110'0000,
    0b0000'0100'0111'0000,
    0b0110'0100'0100'0000,
    0b0000'1110'0010'0000,
  };
};

template<>
struct block_traits<block::L>
{
  static constexpr rotation_t rotation_count { 4U };
  static constexpr std::array<std::uint16_t, rotation_count> masks {
    0b0100'0100'0110'0000,
    0b0000'0111'0100'0000,
    0b0110'0010'0010'0000,
    0b0000'0010'1110'0000,
  };
};

template<>
struct block_traits<block::T>
{
  static constexpr rotation_t rotation_count { 4U };
  static constexpr std::array<std::uint16_t, rotation_count> masks {
    0b0000'1110'0100'0000,
    0b0000'0010'0110'0010,
    0b0000'0100'1110'0000,
    0b0000'0100'0110'0100,
  };
};

constexpr auto
mask_cell(std::uint16_t mask, std::uint8_t local_x, std::uint8_t local_y)
  -> bool
{ return ((mask >> ((local_y * 4) + local_x)) & 1U) != 0; }

using block_t = std::
  variant<block::O, block::I, block::S, block::Z, block::J, block::L, block::T>;
using floor_t = std::array<std::bitset<column_count>, row_count>;

template<typename... Ts>
struct match : Ts...
{
  using Ts::operator()...;
};

template<typename... Ts>
match(Ts...) -> match<Ts...>;

} // namespace tetris