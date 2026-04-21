#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>

#include "simulation.h"
#include "types.h"

// ---------------------------------------------------------------------------
// Pixel-buffer renderer helpers
// ---------------------------------------------------------------------------

static uint8_t scaledChannel(uint8_t channel, float factor)
{
    return static_cast<uint8_t>(std::clamp(int(channel * factor), 0, 255));
}

static ColorRGBA lerpColor(const ColorRGBA& a, const ColorRGBA& b, float t)
{
    const float clamped = std::clamp(t, 0.f, 1.f);
    auto lerpChannel = [&](uint8_t lhs, uint8_t rhs) -> uint8_t {
        return static_cast<uint8_t>(lhs + (rhs - lhs) * clamped);
    };

    return {
        lerpChannel(a.r, b.r),
        lerpChannel(a.g, b.g),
        lerpChannel(a.b, b.b),
        255
    };
}

static ColorRGBA heatOverlayColor(int8_t temperature)
{
    const float hotT  = std::clamp(static_cast<float>(temperature) / 100.f, 0.f, 1.f);
    const float coldT = std::clamp(static_cast<float>(-temperature) / 100.f, 0.f, 1.f);

    if (hotT > 0.f) {
        return lerpColor({255, 120, 20, 255}, {255, 245, 210, 255}, hotT);
    }

    if (coldT > 0.f) {
        return lerpColor({40, 120, 255, 255}, {180, 240, 255, 255}, coldT);
    }

    return {0, 0, 0, 255};
}

// Fill the RGBA pixel buffer from the current world state.
// One pixel per grid cell — the sf::Sprite is then scaled up by CELL_SIZE.
static void buildPixelBuffer(const World& world,
                             const MaterialRegistry& reg,
                             std::vector<ColorRGBA>& buf,
                             bool heatOverlayEnabled)
{
    auto cellAt = [&](int x, int y) -> const Cell* {
        if (x < 0 || x >= world.width || y < 0 || y >= world.height) return nullptr;
        return &world.cells[y * world.width + x];
    };

    for (int y = 0; y < world.height; ++y) {
        for (int x = 0; x < world.width; ++x) {
            const int i = y * world.width + x;
            const Cell& c = world.cells[i];
            const MaterialDef* def = reg.get(c.material);

            if (!def || c.material == MAT_EMPTY) {
                buf[i] = {0, 0, 0, 255};
                continue;
            }

            float factor = c.shade / 128.f;

            if (def->movementModel == MovementModel::Liquid) {
                // Liquids read better with calm fill shading and a brighter
                // exposed surface than with powder-like per-particle noise.
                factor = 0.98f + (static_cast<int>(c.shade) - 128) / 1024.f;

                const Cell* above = cellAt(x, y - 1);
                const Cell* left  = cellAt(x - 1, y);
                const Cell* right = cellAt(x + 1, y);

                if (!above || above->material != c.material) {
                    factor += 0.16f;
                }

                const bool edgeExposed =
                    (left  && left->material  == MAT_EMPTY) ||
                    (right && right->material == MAT_EMPTY);
                if (edgeExposed) {
                    factor += 0.04f;
                }
            }

            ColorRGBA baseColor;
            baseColor.r = scaledChannel(def->color.r, factor);
            baseColor.g = scaledChannel(def->color.g, factor);
            baseColor.b = scaledChannel(def->color.b, factor);
            baseColor.a = 255;

            if (!heatOverlayEnabled || c.temperature == 0) {
                buf[i] = baseColor;
                continue;
            }

            const float overlayStrength = std::clamp(std::abs(static_cast<int>(c.temperature)) / 90.f, 0.2f, 0.85f);
            buf[i] = lerpColor(baseColor, heatOverlayColor(c.temperature), overlayStrength);
        }
    }
}

// ---------------------------------------------------------------------------
// HUD font
// ---------------------------------------------------------------------------
static bool loadHudFont(sf::Font& font)
{
    return font.openFromFile("/System/Library/Fonts/Menlo.ttc");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    enum class DebugScene {
        None = 0,
        OilBurn,
        Boiler,
        Condensation,
        Freezing,
        Corrosion,
    };

    std::srand(static_cast<unsigned>(std::time(nullptr)));

    // Build the material registry and hand ownership to the simulation.
    MaterialRegistry registry = MaterialRegistry::buildDefaults();

    // Keep a raw pointer to the registry inside the simulation for HUD queries
    // after the move. We call sim.materials() for that instead.
    Simulation sim(std::move(registry));

    // ---- Window ----
    constexpr unsigned TARGET_FPS = 60;
    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Falling Sand");
    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(TARGET_FPS);

    // ---- Pixel-buffer texture ------------------------------------------------
    // The grid is GRID_WIDTH×GRID_HEIGHT pixels. The sprite is scaled up by
    // CELL_SIZE to fill the window, replacing the old six-vertices-per-cell path.
    std::vector<ColorRGBA> pixelBuffer(GRID_WIDTH * GRID_HEIGHT);

    sf::Texture gridTexture(sf::Vector2u{static_cast<unsigned>(GRID_WIDTH),
                                         static_cast<unsigned>(GRID_HEIGHT)});
    sf::Sprite gridSprite(gridTexture);
    gridSprite.setScale({static_cast<float>(CELL_SIZE), static_cast<float>(CELL_SIZE)});

    // ---- HUD -----------------------------------------------------------------
    sf::Font hudFont;
    const bool hasHudFont = loadHudFont(hudFont);
    sf::Text hudText(hudFont);
    hudText.setCharacterSize(15);
    hudText.setFillColor(sf::Color::White);
    hudText.setPosition({14.f, 10.f});

    sf::RectangleShape hudPanel;
    hudPanel.setPosition({8.f, 8.f});
    hudPanel.setFillColor(sf::Color(0, 0, 0, 170));
    hudPanel.setOutlineThickness(1.f);
    hudPanel.setOutlineColor(sf::Color(255, 255, 255, 60));

    sf::CircleShape brushOutline;
    brushOutline.setFillColor(sf::Color::Transparent);
    brushOutline.setOutlineColor(sf::Color::White);
    brushOutline.setOutlineThickness(1.f);

    sf::RectangleShape inspectCellOutline;
    inspectCellOutline.setSize({static_cast<float>(CELL_SIZE), static_cast<float>(CELL_SIZE)});
    inspectCellOutline.setFillColor(sf::Color::Transparent);
    inspectCellOutline.setOutlineThickness(1.f);
    inspectCellOutline.setOutlineColor(sf::Color(255, 255, 255, 180));

    // ---- Input state ---------------------------------------------------------
    MaterialId currentMat = MAT_SAND;
    int brushRadius = 3;
    sf::Vector2i prevMouse{-1, -1};
    bool debugHudEnabled = true;
    bool heatOverlayEnabled = false;
    bool paused = false;
    bool stepOnce = false;
    DebugScene activeScene = DebugScene::None;

    // Interpolate the brush stroke between previous and current mouse positions
    // so fast mouse movement doesn't leave gaps.
    auto stroke = [&](sf::Vector2i cur, MaterialId mat) {
        if (prevMouse.x < 0) {
            sim.paint(cur.x / CELL_SIZE, cur.y / CELL_SIZE, brushRadius, mat);
        } else {
            const int dx    = cur.x - prevMouse.x;
            const int dy    = cur.y - prevMouse.y;
            const int steps = std::max(1, std::max(std::abs(dx), std::abs(dy)) / CELL_SIZE);
            for (int i = 0; i <= steps; ++i) {
                const int ix = prevMouse.x + dx * i / steps;
                const int iy = prevMouse.y + dy * i / steps;
                sim.paint(ix / CELL_SIZE, iy / CELL_SIZE, brushRadius, mat);
            }
        }
        prevMouse = cur;
    };

    auto sceneName = [&](DebugScene scene) -> const char* {
        switch (scene) {
        case DebugScene::OilBurn:     return "Oil Burn";
        case DebugScene::Boiler:      return "Boiler";
        case DebugScene::Condensation:return "Condensation";
        case DebugScene::Freezing:    return "Freezing";
        case DebugScene::Corrosion:   return "Corrosion";
        default:                      return "Custom";
        }
    };

    auto loadScene = [&](DebugScene scene) {
        sim.clear();
        activeScene = scene;
        currentMat = MAT_SAND;
        prevMouse = {-1, -1};

        auto placeRect = [&](int x0, int y0, int w, int h, MaterialId mat) {
            for (int y = y0; y < y0 + h; ++y) {
                for (int x = x0; x < x0 + w; ++x) {
                    sim.spawnInto(x, y, mat);
                }
            }
        };

        switch (scene) {
        case DebugScene::OilBurn:
            placeRect(40, 165, 220, 6, MAT_STONE);
            placeRect(45, 145, 95, 18, MAT_OIL);
            placeRect(150, 125, 35, 38, MAT_WATER);
            placeRect(90, 142, 8, 3, MAT_FIRE);
            currentMat = MAT_FIRE;
            break;

        case DebugScene::Boiler:
            placeRect(95, 40, 4, 120, MAT_STONE);
            placeRect(200, 40, 4, 120, MAT_STONE);
            placeRect(95, 160, 109, 4, MAT_STONE);
            placeRect(99, 85, 101, 75, MAT_WATER);
            placeRect(135, 150, 28, 5, MAT_FIRE);
            currentMat = MAT_FIRE;
            break;

        case DebugScene::Condensation:
            placeRect(60, 32, 4, 140, MAT_STONE);
            placeRect(235, 32, 4, 140, MAT_STONE);
            placeRect(60, 168, 179, 4, MAT_STONE);
            placeRect(64, 32, 171, 4, MAT_STONE);
            placeRect(90, 120, 120, 30, MAT_WATER);
            placeRect(110, 46, 80, 28, MAT_STEAM);
            currentMat = MAT_STEAM;
            break;

        case DebugScene::Freezing:
            placeRect(70, 90, 4, 70, MAT_STONE);
            placeRect(226, 90, 4, 70, MAT_STONE);
            placeRect(70, 160, 160, 4, MAT_STONE);
            placeRect(74, 104, 152, 56, MAT_WATER);
            placeRect(138, 96, 24, 8, MAT_ICE);
            currentMat = MAT_ICE;
            break;

        case DebugScene::Corrosion:
            placeRect(60, 165, 180, 5, MAT_STONE);
            placeRect(85, 105, 20, 60, MAT_STONE);
            placeRect(110, 125, 35, 18, MAT_WOOD);
            placeRect(150, 116, 28, 10, MAT_ICE);
            placeRect(185, 90, 18, 75, MAT_STONE);
            placeRect(100, 78, 95, 18, MAT_ACID);
            currentMat = MAT_ACID;
            break;

        default:
            break;
        }
    };

    sf::Clock fpsClock;
    float fps = 0.f;

    // =========================================================================
    // Main loop
    // =========================================================================
    while (window.isOpen()) {
        // ---- Events ----------------------------------------------------------
        while (const auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                switch (kp->code) {
                case sf::Keyboard::Key::Num1: currentMat = MAT_SAND;  break;
                case sf::Keyboard::Key::Num2: currentMat = MAT_STONE; break;
                case sf::Keyboard::Key::Num3: currentMat = MAT_WATER; break;
                case sf::Keyboard::Key::Num4: currentMat = MAT_OIL;   break;
                case sf::Keyboard::Key::Num5: currentMat = MAT_SMOKE; break;
                case sf::Keyboard::Key::Num6: currentMat = MAT_FIRE;  break;
                case sf::Keyboard::Key::Num7: currentMat = MAT_STEAM; break;
                case sf::Keyboard::Key::Num8: currentMat = MAT_WOOD;  break;
                case sf::Keyboard::Key::Num9: currentMat = MAT_LAVA;  break;
                case sf::Keyboard::Key::Num0: currentMat = MAT_EMPTY; break;
                case sf::Keyboard::Key::A:    currentMat = MAT_ACID;  break;
                case sf::Keyboard::Key::I:    currentMat = MAT_ICE;   break;
                case sf::Keyboard::Key::C:    sim.clear(); activeScene = DebugScene::None; break;
                case sf::Keyboard::Key::F1:   debugHudEnabled = !debugHudEnabled; break;
                case sf::Keyboard::Key::F2:   heatOverlayEnabled = !heatOverlayEnabled; break;
                case sf::Keyboard::Key::Space: paused = !paused; break;
                case sf::Keyboard::Key::Period:
                case sf::Keyboard::Key::N:    stepOnce = true; break;
                case sf::Keyboard::Key::F5:   loadScene(DebugScene::OilBurn); break;
                case sf::Keyboard::Key::F6:   loadScene(DebugScene::Boiler); break;
                case sf::Keyboard::Key::F7:   loadScene(DebugScene::Condensation); break;
                case sf::Keyboard::Key::F8:   loadScene(DebugScene::Freezing); break;
                case sf::Keyboard::Key::F9:   loadScene(DebugScene::Corrosion); break;
                default: break;
                }
            }

            if (const auto* ws = event->getIf<sf::Event::MouseWheelScrolled>()) {
                brushRadius = std::clamp(brushRadius + (ws->delta > 0 ? 1 : -1), 1, 20);
            }
        }

        const bool lmb = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        const bool rmb = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);

        if (lmb || rmb) {
            activeScene = DebugScene::None;
            stroke(sf::Mouse::getPosition(window), lmb ? currentMat : MAT_EMPTY);
        } else {
            prevMouse = {-1, -1};
        }

        // ---- Simulate --------------------------------------------------------
        if (!paused || stepOnce) {
            sim.update();
            stepOnce = false;
        }

        // ---- Build pixel buffer and upload to GPU ----------------------------
        buildPixelBuffer(sim.world(), sim.materials(), pixelBuffer, heatOverlayEnabled);
        gridTexture.update(reinterpret_cast<const uint8_t*>(pixelBuffer.data()));

        // ---- FPS / HUD -------------------------------------------------------
        const float dt = fpsClock.restart().asSeconds();
        if (dt > 0.f) fps = 1.f / dt;

        const sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        const int brushCellX = std::clamp(mousePos.x / CELL_SIZE, 0, GRID_WIDTH  - 1);
        const int brushCellY = std::clamp(mousePos.y / CELL_SIZE, 0, GRID_HEIGHT - 1);
        const float brushRadiusPx = brushRadius * static_cast<float>(CELL_SIZE)
                                  + CELL_SIZE * 0.5f;
        brushOutline.setRadius(brushRadiusPx);
        brushOutline.setOrigin({brushRadiusPx, brushRadiusPx});
        brushOutline.setPosition({brushCellX * static_cast<float>(CELL_SIZE) + CELL_SIZE * 0.5f,
                                   brushCellY * static_cast<float>(CELL_SIZE) + CELL_SIZE * 0.5f});

        inspectCellOutline.setPosition({
            brushCellX * static_cast<float>(CELL_SIZE),
            brushCellY * static_cast<float>(CELL_SIZE)
        });

        if (hasHudFont && debugHudEnabled) {
            const MaterialDef* matDef = sim.materials().get(currentMat);
            const char* matName = matDef ? matDef->name.c_str() : "Unknown";
            const Cell hoveredCell = sim.getCell(brushCellX, brushCellY);
            const MaterialDef* hoveredDef = sim.materials().get(hoveredCell.material);
            const char* hoveredName = hoveredDef ? hoveredDef->name.c_str() : "Unknown";

            std::ostringstream hud;
            hud << "Material: " << matName
                << "\nFPS: " << static_cast<int>(fps + 0.5f) << "/" << TARGET_FPS
                << "\nBrush: " << brushRadius
                << "\nSim: " << (paused ? "Paused" : "Running")
                << "\nOverlay: " << (heatOverlayEnabled ? "Heat" : "Off")
                << "\nScene: " << sceneName(activeScene)
                << "\nCursor: (" << brushCellX << ", " << brushCellY << ")"
                << "\nHover: " << hoveredName
                << "\nTemp/Life/Aux: "
                << static_cast<int>(hoveredCell.temperature) << " / "
                << static_cast<int>(hoveredCell.life) << " / "
                << static_cast<int>(hoveredCell.aux)
                << "\nControls: 1 Sand 2 Stone 3 Water 4 Oil 5 Smoke 6 Fire 7 Steam 8 Wood 9 Lava I Ice A Acid 0 Erase"
                << "\nDebug: F1 HUD  F2 Heat  Space Pause  N/. Step  F5 Oil  F6 Boiler  F7 Condense  F8 Freeze  F9 Acid  C Clear";

            hudText.setString(hud.str());

            const sf::FloatRect bounds = hudText.getLocalBounds();
            hudPanel.setSize({bounds.size.x + 20.f, bounds.size.y + 22.f});
        }

        // ---- Render ----------------------------------------------------------
        window.clear(sf::Color::Black);
        window.draw(gridSprite);
        window.draw(brushOutline);
        if (hasHudFont && debugHudEnabled) {
            window.draw(hudPanel);
            window.draw(hudText);
        }
        window.draw(inspectCellOutline);
        window.display();
    }

    return 0;
}
