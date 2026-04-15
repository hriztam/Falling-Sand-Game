#include "material_registry.h"
#include "simulation.h"
#include <cassert>
#include <cstdlib>

namespace {
CellSpawnDesc makeSpawn(MaterialId material,
                        uint8_t lifeMin = 0,
                        uint8_t lifeMax = 0,
                        int8_t temperature = 0,
                        uint8_t aux = 0,
                        ShadeMode shadeMode = ShadeMode::Randomized)
{
    CellSpawnDesc spawn;
    spawn.material = material;
    spawn.lifeMin = lifeMin;
    spawn.lifeMax = lifeMax;
    spawn.temperature = temperature;
    spawn.aux = aux;
    spawn.shadeMode = shadeMode;
    return spawn;
}
} // namespace

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
// buildDefaults — registers Empty, Sand, Water, Wall, Oil, Smoke, Fire.
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
        d.spawnState = makeSpawn(MAT_EMPTY);
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
        d.spawnState = makeSpawn(MAT_SAND);
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
        d.spawnState = makeSpawn(MAT_WATER);
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
        d.spawnState = makeSpawn(MAT_WALL);
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
        d.spawnState = makeSpawn(MAT_OIL);
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Smoke (MAT_SMOKE = 5) -------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_SMOKE;
        d.name = "Smoke";
        d.movementModel = MovementModel::Gas;
        d.traits = Trait::Movable | Trait::Displaceable | Trait::GasLike;
        d.density = 0.05f;
        d.spreadFactor = 5;
        d.shadeMin = 80;
        d.shadeMax = 120;
        d.color = {95, 95, 95, 255};
        d.spawnState = makeSpawn(MAT_SMOKE, 20, 40);
        d.specialHook = [](Simulation& sim, int x, int y) {
            const Cell smoke = sim.getCell(x, y);
            if (smoke.material != MAT_SMOKE)
                return;

            if (smoke.life <= 1) {
                sim.spawnInto(x, y, MAT_EMPTY);
                return;
            }

            sim.spawnInto(x, y, makeSpawn(MAT_SMOKE,
                                          static_cast<uint8_t>(smoke.life - 1),
                                          static_cast<uint8_t>(smoke.life - 1),
                                          smoke.temperature,
                                          smoke.aux,
                                          ShadeMode::Preserve));
        };
        reg.registerMaterial(std::move(d));
    }

    // --- Fire (MAT_FIRE = 6) ---------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_FIRE;
        d.name = "Fire";
        d.movementModel = MovementModel::Organic;
        d.traits = 0;
        d.density = 0.f;
        d.spreadFactor = 0;
        d.shadeMin = 180;
        d.shadeMax = 255;
        d.color = {255, 110, 25, 255};
        d.spawnState = makeSpawn(MAT_FIRE, 24, 42);

        // Water contact extinguishes fire into a small puff of smoke.
        {
            InteractionRule rule;
            rule.neighbor.material = MAT_WATER;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 100;
            rule.stopAfterApply = true;
            rule.selfResult = makeSpawn(MAT_SMOKE, 12, 24);
            d.interactionRules.push_back(rule);
        }

        // Fire spreads to any flammable neighbour without caring which
        // concrete material provided the trait.
        {
            InteractionRule rule;
            rule.neighbor.requiredTraits = Trait::Flammable;
            rule.neighborhood = InteractionNeighborhood::Moore;
            rule.chancePercent = 20;
            rule.stopAfterApply = false;
            rule.neighborResult = makeSpawn(MAT_FIRE, 18, 36);
            d.interactionRules.push_back(rule);
        }

        d.specialHook = [](Simulation& sim, int x, int y) {
            const Cell fire = sim.getCell(x, y);
            if (fire.material != MAT_FIRE)
                return;

            if (sim.inBounds(x, y - 1) &&
                sim.getCell(x, y - 1).material == MAT_EMPTY &&
                (std::rand() % 100) < 35) {
                sim.spawnInto(x, y - 1, makeSpawn(MAT_SMOKE, 10, 20), true);
            }

            if (fire.life <= 1) {
                if ((std::rand() % 100) < 80) {
                    sim.spawnInto(x, y, makeSpawn(MAT_SMOKE, 16, 28), true);
                } else {
                    sim.spawnInto(x, y, MAT_EMPTY);
                }
                return;
            }

            sim.spawnInto(x, y, makeSpawn(MAT_FIRE,
                                          static_cast<uint8_t>(fire.life - 1),
                                          static_cast<uint8_t>(fire.life - 1),
                                          fire.temperature,
                                          fire.aux,
                                          ShadeMode::Preserve));
        };

        reg.registerMaterial(std::move(d));
    }

    return reg;
}
