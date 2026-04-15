#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdlib>
#include <ctime>
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

// Fill the RGBA pixel buffer from the current world state.
// One pixel per grid cell — the sf::Sprite is then scaled up by CELL_SIZE.
static void buildPixelBuffer(const World& world,
                              const MaterialRegistry& reg,
                              std::vector<ColorRGBA>& buf)
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

            buf[i].r = scaledChannel(def->color.r, factor);
            buf[i].g = scaledChannel(def->color.g, factor);
            buf[i].b = scaledChannel(def->color.b, factor);
            buf[i].a = 255;
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

    // ---- Input state ---------------------------------------------------------
    MaterialId currentMat = MAT_SAND;
    int brushRadius = 3;
    sf::Vector2i prevMouse{-1, -1};

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
                case sf::Keyboard::Key::Num2: currentMat = MAT_WALL;  break;
                case sf::Keyboard::Key::Num3: currentMat = MAT_WATER; break;
                case sf::Keyboard::Key::Num4: currentMat = MAT_OIL;   break;
                case sf::Keyboard::Key::Num5: currentMat = MAT_SMOKE; break;
                case sf::Keyboard::Key::Num6: currentMat = MAT_FIRE;  break;
                case sf::Keyboard::Key::Num0: currentMat = MAT_EMPTY; break;
                case sf::Keyboard::Key::C:    sim.clear();             break;
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
            stroke(sf::Mouse::getPosition(window), lmb ? currentMat : MAT_EMPTY);
        } else {
            prevMouse = {-1, -1};
        }

        // ---- Simulate --------------------------------------------------------
        sim.update();

        // ---- Build pixel buffer and upload to GPU ----------------------------
        buildPixelBuffer(sim.world(), sim.materials(), pixelBuffer);
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

        if (hasHudFont) {
            const MaterialDef* matDef = sim.materials().get(currentMat);
            const char* matName = matDef ? matDef->name.c_str() : "Unknown";

            hudText.setString(
                std::string("Material: ") + matName +
                "\nFPS: " + std::to_string(static_cast<int>(fps + 0.5f)) +
                    "/" + std::to_string(TARGET_FPS) +
                "\nBrush: " + std::to_string(brushRadius) +
                "\nControls: 1 Sand  2 Wall  3 Water  4 Oil  5 Smoke  6 Fire  0 Erase  C Clear  RMB Erase  Scroll Brush");

            const sf::FloatRect bounds = hudText.getLocalBounds();
            hudPanel.setSize({bounds.size.x + 20.f, bounds.size.y + 22.f});
        }

        // ---- Render ----------------------------------------------------------
        window.clear(sf::Color::Black);
        window.draw(gridSprite);
        window.draw(brushOutline);
        if (hasHudFont) {
            window.draw(hudPanel);
            window.draw(hudText);
        }
        window.display();
    }

    return 0;
}
