#include "material_registry.h"
#include "simulation.h"
#include <cassert>
#include <cstdlib>

namespace
{
    constexpr uint8_t FIRE_AUX_NONE = 0;
    constexpr uint8_t FIRE_AUX_OIL = 1;
    constexpr uint8_t FIRE_AUX_WOOD = 2;

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

    HeatReaction makeHeatReaction(std::optional<int8_t> minTemperature,
                                  std::optional<int8_t> maxTemperature,
                                  std::optional<CellSpawnDesc> selfResult,
                                  uint8_t chancePercent = 100,
                                  bool stopAfterApply = true)
    {
        HeatReaction reaction;
        reaction.minTemperature = minTemperature;
        reaction.maxTemperature = maxTemperature;
        reaction.selfResult = std::move(selfResult);
        reaction.chancePercent = chancePercent;
        reaction.stopAfterApply = stopAfterApply;
        return reaction;
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
// buildDefaults — registers Empty, Sand, Water, Stone, Oil, Smoke, Fire, Steam,
// Wood, Lava, and Ice.
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
        d.coolingRate = 1;
        d.heatConductivity = 1;
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
        d.coolingRate = 4;
        d.heatConductivity = 4;
        d.spawnState = makeSpawn(MAT_WATER);
        d.heatReactions.push_back(
            makeHeatReaction(std::nullopt,
                             int8_t(-20),
                             makeSpawn(MAT_ICE, 0, 0, -40),
                             35));
        d.heatReactions.push_back(
            makeHeatReaction(int8_t(45),
                             std::nullopt,
                             makeSpawn(MAT_STEAM, 0, 0, 70),
                             30));
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Stone (MAT_STONE = 3) -------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_STONE;
        d.name = "Stone";
        d.movementModel = MovementModel::Static;
        d.traits = Trait::SolidLike;
        d.density = 999.f;
        d.spreadFactor = 0;
        d.shadeMin = 100;
        d.shadeMax = 120;
        d.color = {110, 110, 115, 255};
        d.coolingRate = 1;
        d.heatConductivity = 3;
        d.spawnState = makeSpawn(MAT_STONE);
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
        d.coolingRate = 1;
        d.heatConductivity = 1;
        d.spawnState = makeSpawn(MAT_OIL);

        d.heatReactions.push_back(
            makeHeatReaction(int8_t(35),
                             std::nullopt,
                             makeSpawn(MAT_FIRE, 18, 36, 100, FIRE_AUX_OIL),
                             35));

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
        d.coolingRate = 2;
        d.spawnState = makeSpawn(MAT_SMOKE, 20, 40, 25);
        d.specialHook = [](Simulation &sim, int x, int y)
        {
            const Cell smoke = sim.getCell(x, y);
            if (smoke.material != MAT_SMOKE)
                return;

            if (smoke.life <= 1)
            {
                sim.spawnInto(x, y, MAT_EMPTY);
                return;
            }

            sim.spawnInto(x, y, makeSpawn(MAT_SMOKE, static_cast<uint8_t>(smoke.life - 1), static_cast<uint8_t>(smoke.life - 1), smoke.temperature, smoke.aux, ShadeMode::Preserve));
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
        d.heatEmission = 12;
        d.spawnState = makeSpawn(MAT_FIRE, 24, 42, 100);

        // Water contact quenches fire into steam.
        {
            InteractionRule rule;
            rule.neighbor.material = MAT_WATER;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 100;
            rule.stopAfterApply = true;
            rule.selfResult = makeSpawn(MAT_STEAM, 0, 0, 55);
            d.interactionRules.push_back(rule);
        }

        // Oil keeps its denser smoke-producing fire variant when ignited by
        // contact, while other flammables fall through to the generic rule.
        {
            InteractionRule rule;
            rule.neighbor.material = MAT_OIL;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 30;
            rule.stopAfterApply = true;
            rule.neighborResult = makeSpawn(MAT_FIRE, 18, 36, 100, FIRE_AUX_OIL);
            d.interactionRules.push_back(rule);
        }

        {
            InteractionRule rule;
            rule.neighbor.material = MAT_WOOD;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 20;
            rule.stopAfterApply = true;
            rule.neighborResult = makeSpawn(MAT_FIRE, 90, 150, 100, FIRE_AUX_WOOD);
            d.interactionRules.push_back(rule);
        }

        // Any flammable neighbor can catch fire through contact instead of
        // relying on a per-material heat threshold.
        {
            InteractionRule rule;
            rule.neighbor.requiredTraits = Trait::Flammable;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 30;
            rule.stopAfterApply = true;
            rule.neighborResult = makeSpawn(MAT_FIRE, 18, 36, 100);
            d.interactionRules.push_back(rule);
        }

        d.specialHook = [](Simulation &sim, int x, int y)
        {
            const Cell fire = sim.getCell(x, y);
            if (fire.material != MAT_FIRE)
                return;

            const bool isOilFire = fire.aux == FIRE_AUX_OIL;
            const bool isWoodFire = fire.aux == FIRE_AUX_WOOD;

            if ((isOilFire || isWoodFire) &&
                sim.inBounds(x, y - 1) &&
                sim.getCell(x, y - 1).material == MAT_EMPTY)
            {
                const int smokeChance = isWoodFire ? 18 : 35;
                if ((std::rand() % 100) < smokeChance)
                {
                    const uint8_t smokeLifeMin = isWoodFire ? 18 : 12;
                    const uint8_t smokeLifeMax = isWoodFire ? 32 : 24;
                    sim.spawnInto(x, y - 1, makeSpawn(MAT_SMOKE, smokeLifeMin, smokeLifeMax, 35), true);
                }
            }

            if (fire.life <= 1)
            {
                sim.spawnInto(x, y, MAT_EMPTY);
                return;
            }

            sim.spawnInto(x, y, makeSpawn(MAT_FIRE, static_cast<uint8_t>(fire.life - 1), static_cast<uint8_t>(fire.life - 1), fire.temperature, fire.aux, ShadeMode::Preserve));
        };

        reg.registerMaterial(std::move(d));
    }

    // --- Steam (MAT_STEAM = 7) -------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_STEAM;
        d.name = "Steam";
        d.movementModel = MovementModel::Gas;
        d.traits = Trait::Movable | Trait::Displaceable | Trait::GasLike;
        d.density = 0.04f;
        d.spreadFactor = 6;
        d.shadeMin = 185;
        d.shadeMax = 225;
        d.color = {205, 205, 205, 255};
        d.coolingRate = 1;
        d.heatConductivity = 2;
        d.spawnState = makeSpawn(MAT_STEAM, 0, 0, 70);
        d.heatReactions.push_back(
            makeHeatReaction(std::nullopt,
                             int8_t(12),
                             makeSpawn(MAT_WATER, 0, 0, 0),
                             45));
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Wood (MAT_WOOD = 8) ---------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_WOOD;
        d.name = "Wood";
        d.movementModel = MovementModel::Static;
        d.traits = Trait::Flammable | Trait::SolidLike | Trait::ConductsHeat;
        d.density = 2.2f;
        d.spreadFactor = 0;
        d.shadeMin = 82;
        d.shadeMax = 116;
        d.color = {120, 78, 38, 255};
        d.coolingRate = 1;
        d.heatConductivity = 1;
        d.spawnState = makeSpawn(MAT_WOOD);
        d.heatReactions.push_back(
            makeHeatReaction(int8_t(32),
                             std::nullopt,
                             makeSpawn(MAT_FIRE, 90, 150, 100, FIRE_AUX_WOOD),
                             20));
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    // --- Lava (MAT_LAVA = 9) ---------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_LAVA;
        d.name = "Lava";
        d.movementModel = MovementModel::Liquid;
        d.traits = Trait::Movable | Trait::AffectedByGravity | Trait::Displaceable | Trait::LiquidLike | Trait::ConductsHeat;
        d.density = 2.8f;
        d.spreadFactor = 2;
        d.shadeMin = 150;
        d.shadeMax = 220;
        d.color = {255, 92, 18, 255};
        d.coolingRate = 0;
        d.heatEmission = 18;
        d.heatConductivity = 5;
        d.spawnState = makeSpawn(MAT_LAVA, 0, 0, 120);

        {
            InteractionRule rule;
            rule.neighbor.material = MAT_WATER;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 100;
            rule.stopAfterApply = true;
            rule.selfResult = makeSpawn(MAT_STONE, 0, 0, 0);
            rule.neighborResult = makeSpawn(MAT_STEAM, 0, 0, 95);
            d.interactionRules.push_back(rule);
        }

        {
            InteractionRule rule;
            rule.neighbor.material = MAT_OIL;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 80;
            rule.stopAfterApply = true;
            rule.neighborResult = makeSpawn(MAT_FIRE, 18, 36, 100, FIRE_AUX_OIL);
            d.interactionRules.push_back(rule);
        }

        {
            InteractionRule rule;
            rule.neighbor.material = MAT_WOOD;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 65;
            rule.stopAfterApply = true;
            rule.neighborResult = makeSpawn(MAT_FIRE, 90, 150, 100, FIRE_AUX_WOOD);
            d.interactionRules.push_back(rule);
        }

        {
            InteractionRule rule;
            rule.neighbor.requiredTraits = Trait::Flammable;
            rule.neighborhood = InteractionNeighborhood::Cardinal;
            rule.chancePercent = 70;
            rule.stopAfterApply = true;
            rule.neighborResult = makeSpawn(MAT_FIRE, 18, 36, 100);
            d.interactionRules.push_back(rule);
        }

        d.specialHook = [](Simulation &sim, int x, int y)
        {
            const Cell lava = sim.getCell(x, y);
            if (lava.material != MAT_LAVA)
                return;

            static constexpr int kCardinalOffsets[4][2] = {
                {0, -1},
                {1, 0},
                {0, 1},
                {-1, 0},
            };

            bool touchesStone = false;
            for (const auto &offset : kCardinalOffsets)
            {
                const Cell neighbor = sim.getCell(x + offset[0], y + offset[1]);
                if (neighbor.material == MAT_STONE)
                {
                    touchesStone = true;
                    break;
                }
            }

            // Water contact nucleates a stone crust through the interaction
            // rule above; once a crust exists, adjacent cool lava solidifies
            // inward as a front rather than as random speckles.
            if (touchesStone && lava.temperature <= 55)
            {
                sim.spawnInto(x, y, makeSpawn(MAT_STONE, 0, 0, 0), true);
                return;
            }
        };
        reg.registerMaterial(std::move(d));
    }

    // --- Ice (MAT_ICE = 10) ----------------------------------------------
    {
        MaterialDef d;
        d.id = MAT_ICE;
        d.name = "Ice";
        d.movementModel = MovementModel::Static;
        d.traits = Trait::SolidLike | Trait::ConductsHeat;
        d.density = 1.1f;
        d.spreadFactor = 0;
        d.shadeMin = 170;
        d.shadeMax = 220;
        d.color = {170, 225, 255, 255};
        d.coolingRate = 0;
        d.heatEmission = -8;
        d.heatConductivity = 4;
        d.spawnState = makeSpawn(MAT_ICE, 0, 0, -100);
        d.heatReactions.push_back(
            makeHeatReaction(int8_t(10),
                             std::nullopt,
                             makeSpawn(MAT_WATER, 0, 0, 0),
                             100));
        d.specialHook = nullptr;
        reg.registerMaterial(std::move(d));
    }

    return reg;
}
