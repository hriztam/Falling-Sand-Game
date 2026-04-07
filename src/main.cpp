#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

#include "simulation.h"
#include "types.h"

namespace {

// Per-cell color with subtle shade variation for visual depth.
sf::Color getCellColor(const Cell& cell)
{
    const int s = static_cast<int>(cell.shade) - 128; // roughly -32..+31
    switch (cell.material) {
    case Material::Sand:
        return sf::Color(
            static_cast<uint8_t>(std::clamp(194 + s,       140, 240)),
            static_cast<uint8_t>(std::clamp(160 + s / 2,   120, 200)),
            static_cast<uint8_t>(std::clamp( 80 + s / 4,    50, 130)));
    case Material::Water:
        return sf::Color(
            static_cast<uint8_t>(std::clamp( 30 + s / 3,   10,  80)),
            static_cast<uint8_t>(std::clamp(120 + s,        70, 180)),
            static_cast<uint8_t>(std::clamp(220 + s,       170, 255)));
    case Material::Wall:
        return sf::Color(
            static_cast<uint8_t>(std::clamp(110 + s,        70, 160)),
            static_cast<uint8_t>(std::clamp(110 + s,        70, 160)),
            static_cast<uint8_t>(std::clamp(115 + s,        75, 165)));
    default:
        return sf::Color::Black;
    }
}

const char* materialName(Material m)
{
    switch (m) {
    case Material::Sand:  return "Sand";
    case Material::Water: return "Water";
    case Material::Wall:  return "Wall";
    default:              return "Eraser";
    }
}

} // namespace

int main()
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Falling Sand");
    window.setFramerateLimit(60);

    Simulation sim;
    Material   currentMat = Material::Sand;
    int        brushRadius = 3;

    // Track previous mouse position to interpolate strokes between frames.
    // When the mouse moves faster than one frame, we draw a line of circles
    // so no gaps appear in the painted trail.
    sf::Vector2i prevMouse{-1, -1};

    // Helper: paint a continuous stroke from prevMouse to cur, then update prevMouse.
    auto stroke = [&](sf::Vector2i cur, Material mat) {
        if (prevMouse.x < 0) {
            // First frame of this press — just paint the current point
            sim.paint(cur.x / CELL_SIZE, cur.y / CELL_SIZE, brushRadius, mat);
        } else {
            // Interpolate in grid-cell steps so we never skip cells even at high speed
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

    // Pre-allocated vertex array: 2 triangles (6 vertices) per cell.
    // Updating a VertexArray in-place and drawing once is far faster than
    // individual RectangleShape draw calls.
    sf::VertexArray verts(sf::PrimitiveType::Triangles,
                          static_cast<std::size_t>(GRID_WIDTH * GRID_HEIGHT * 6));

    while (window.isOpen())
    {
        // ---- Events ----
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto* kp = event->getIf<sf::Event::KeyPressed>()) {
                switch (kp->code) {
                case sf::Keyboard::Key::Num1: currentMat = Material::Sand;  break;
                case sf::Keyboard::Key::Num2: currentMat = Material::Wall;  break;
                case sf::Keyboard::Key::Num3: currentMat = Material::Water; break;
                case sf::Keyboard::Key::Num0: currentMat = Material::Empty; break;
                case sf::Keyboard::Key::C:    sim.clear();                  break;
                default: break;
                }
            }

            if (const auto* sw = event->getIf<sf::Event::MouseWheelScrolled>())
                brushRadius = std::clamp(brushRadius + (sw->delta > 0.f ? 1 : -1), 1, 15);
        }

        const bool lmb = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        const bool rmb = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);

        if (lmb || rmb) {
            stroke(sf::Mouse::getPosition(window), lmb ? currentMat : Material::Empty);
        } else {
            prevMouse = {-1, -1}; // reset so next press starts fresh
        }

        // ---- Simulate ----
        sim.update();

        // ---- Build vertex array ----
        for (int y = 0; y < GRID_HEIGHT; ++y) {
            for (int x = 0; x < GRID_WIDTH; ++x) {
                const Cell        cell  = sim.getCell(x, y);
                const sf::Color   color = getCellColor(cell);
                const std::size_t vi    = static_cast<std::size_t>((y * GRID_WIDTH + x) * 6);

                const float px = static_cast<float>(x * CELL_SIZE);
                const float py = static_cast<float>(y * CELL_SIZE);
                const float pw = static_cast<float>(CELL_SIZE);
                const float ph = static_cast<float>(CELL_SIZE);

                // Triangle 1 (top-left half)
                verts[vi + 0] = {{px,      py     }, color};
                verts[vi + 1] = {{px + pw, py     }, color};
                verts[vi + 2] = {{px,      py + ph}, color};
                // Triangle 2 (bottom-right half)
                verts[vi + 3] = {{px + pw, py     }, color};
                verts[vi + 4] = {{px + pw, py + ph}, color};
                verts[vi + 5] = {{px,      py + ph}, color};
            }
        }

        // ---- Render ----
        window.clear(sf::Color::Black);
        window.draw(verts);

        window.setTitle(
            std::string("Falling Sand | ") + materialName(currentMat) +
            " | Brush: " + std::to_string(brushRadius) +
            " | 1=Sand  2=Wall  3=Water  0=Erase  C=Clear  Scroll=Brush Size  RMB=Erase");

        window.display();
    }

    return 0;
}
