#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Grid / window constants
// ---------------------------------------------------------------------------
constexpr int WINDOW_WIDTH  = 1200;
constexpr int WINDOW_HEIGHT = 800;
constexpr int CELL_SIZE     = 4;
constexpr int GRID_WIDTH    = WINDOW_WIDTH  / CELL_SIZE;
constexpr int GRID_HEIGHT   = WINDOW_HEIGHT / CELL_SIZE;

// ---------------------------------------------------------------------------
// MaterialId — replaces the old enum class Material.
// Well-known built-in IDs are reserved here; additional materials are
// registered at startup in MaterialRegistry.
// ---------------------------------------------------------------------------
using MaterialId = uint16_t;

constexpr MaterialId MAT_EMPTY = 0;
constexpr MaterialId MAT_SAND  = 1;
constexpr MaterialId MAT_WATER = 2;
constexpr MaterialId MAT_WALL  = 3;
constexpr MaterialId MAT_OIL   = 4;
constexpr MaterialId MAT_SMOKE = 5;
constexpr MaterialId MAT_FIRE  = 6;
constexpr MaterialId MAT_STEAM = 7;

// ---------------------------------------------------------------------------
// MovementModel — determines which update family handles a material.
// ---------------------------------------------------------------------------
enum class MovementModel : uint8_t {
    Empty   = 0,  // void / air — never processed
    Static  = 1,  // immovable solid (wall)
    Powder  = 2,  // falls and slides diagonally (sand)
    Liquid  = 3,  // falls and spreads laterally (water)
    Gas     = 4,  // rises and spreads laterally (smoke)
    Organic = 5,  // custom specialHook drives all behaviour (plants, fire)
};

// ---------------------------------------------------------------------------
// Trait — bitfield constants describing cross-cutting material properties.
// Store as uint16_t in MaterialDef.
// ---------------------------------------------------------------------------
namespace Trait {
    constexpr uint16_t Movable           = 1u << 0;
    constexpr uint16_t AffectedByGravity = 1u << 1;
    constexpr uint16_t Displaceable      = 1u << 2;  // can be pushed aside by denser materials
    constexpr uint16_t Flammable         = 1u << 3;
    constexpr uint16_t LiquidLike        = 1u << 4;
    constexpr uint16_t GasLike           = 1u << 5;
    constexpr uint16_t SolidLike         = 1u << 6;
    constexpr uint16_t SupportsGrowth    = 1u << 7;
    constexpr uint16_t ConductsHeat      = 1u << 8;
}

// ---------------------------------------------------------------------------
// ColorRGBA — backend-agnostic pixel color. Used by MaterialDef and the
// pixel-buffer renderer; has no SFML dependency.
// ---------------------------------------------------------------------------
struct ColorRGBA {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// ---------------------------------------------------------------------------
// Cell — compact POD stored for every grid position. Kept to 8 bytes so the
// full 300×200 grid fits comfortably in ~480 KB.
// ---------------------------------------------------------------------------
struct Cell {
    MaterialId material    = MAT_EMPTY; // 2 bytes
    uint8_t    shade       = 128;       // 1 byte — visual brightness [shadeMin, shadeMax]
    int8_t     temperature = 0;         // 1 byte — relative temperature offset
    uint8_t    life        = 0;         // 1 byte — countdown timer (fire burnout, etc.)
    uint8_t    aux         = 0;         // 1 byte — material-specific state
    uint8_t    _pad[2]     = {};        // 2 bytes — explicit padding to 8 bytes
};
