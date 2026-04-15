#include "material_registry.h"
#include <cassert>

// ---------------------------------------------------------------------------
// MaterialRegistry
// ---------------------------------------------------------------------------

void MaterialRegistry::registerMaterial(MaterialDef def)
{
    // Reject duplicate names (startup-only path, linear scan is fine).
    for (const auto &existing : m_defs)
    {
        if (!existing.name.empty() && existing.name == def.name)
        {
            assert(false && "MaterialRegistry: duplicate material name");
            return;
        }
    }

    // Grow the vector to accommodate def.id, filling gaps with default defs.
    if (def.id >= m_defs.size())
    {
        m_defs.resize(static_cast<std::size_t>(def.id) + 1);
    }

    // Reject duplicate IDs (a gap slot has empty name and id == MAT_EMPTY by default).
    if (!m_defs[def.id].name.empty())
    {
        assert(false && "MaterialRegistry: duplicate material id");
        return;
    }

    m_defs[def.id] = std::move(def);
}

const MaterialDef *MaterialRegistry::get(MaterialId id) const
{
    if (id >= m_defs.size())
        return nullptr;
    if (m_defs[id].name.empty())
        return nullptr; // gap slot
    return &m_defs[id];
}

const MaterialDef *MaterialRegistry::getByName(const std::string &name) const
{
    for (const auto &def : m_defs)
    {
        if (def.name == name)
            return &def;
    }
    return nullptr;
}

bool MaterialRegistry::has(MaterialId id) const
{
    return id < m_defs.size() && !m_defs[id].name.empty();
}

// ---------------------------------------------------------------------------
// buildDefaults — registers Empty, Sand, Water, Wall, Oil.
// Written with explicit field assignment for C++17 compatibility
// (designated initialisers are C++20).
// ---------------------------------------------------------------------------
MaterialRegistry MaterialRegistry::buildDefaults()
{
    MaterialRegistry reg;

    // --- Empty (MAT_EMPTY = 0) -------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_EMPTY;
        d.name = "Empty";
        d.movementModel = MovementModel::Empty;
        d.traits = 0;
        d.density = 0.f;
        d.spreadFactor = 0;
        d.shadeMin = 128;
        d.shadeMax = 128;
        d.color = {0, 0, 0, 255};
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Sand (MAT_SAND = 1) ---------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_SAND;
        d.name = "Sand";
        d.movementModel = MovementModel::Powder;
        d.traits = Trait::Movable | Trait::AffectedByGravity | Trait::Displaceable;
        d.density = 1.5f;
        d.spreadFactor = 0; // powders don't spread laterally
        d.shadeMin = 96;
        d.shadeMax = 159;
        d.color = {194, 160, 80, 255};
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Water (MAT_WATER = 2) -------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_WATER;
        d.name = "Water";
        d.movementModel = MovementModel::Liquid;
        d.traits = Trait::Movable | Trait::AffectedByGravity | Trait::Displaceable | Trait::LiquidLike;
        d.density = 1.0f;
        d.spreadFactor = 6;
        d.shadeMin = 110;
        d.shadeMax = 140;
        d.color = {30, 120, 220, 255};
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Wall (MAT_WALL = 3) ---------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_WALL;
        d.name = "Wall";
        d.movementModel = MovementModel::Static;
        d.traits = Trait::SolidLike;
        d.density = 999.f;
        d.spreadFactor = 0;
        d.shadeMin = 100;
        d.shadeMax = 120;
        d.color = {110, 110, 115, 255};
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Oil (MAT_OIL = 4) -----------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_OIL;
        d.name = "Oil";
        d.movementModel = MovementModel::Liquid;
        d.traits = Trait::Movable | Trait::AffectedByGravity | Trait::Displaceable | Trait::LiquidLike | Trait::Flammable;
        d.density = 0.8f;
        d.spreadFactor = 3;
        d.shadeMin = 40;
        d.shadeMax = 60;
        d.color = {70, 50, 15, 255};
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    return reg;
}
