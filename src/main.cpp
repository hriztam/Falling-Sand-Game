#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

#include "simulation.h"
#include "types.h"

namespace
{

    constexpr unsigned TARGET_FPS = 60;

    bool loadHudFont(sf::Font &font)
    {
        return font.openFromFile("/System/Library/Fonts/Supplemental/Menlo.ttc");
    }

    // Flat material colors for now.
    sf::Color getCellColor(const Cell &cell)
    {
        switch (cell.material)
        {
        case Material::Sand:
            return sf::Color(194, 160, 80);
        case Material::Water:
            return sf::Color(30, 120, 220);
        case Material::Wall:
            return sf::Color(110, 110, 115);
        default:
            return sf::Color::Black;
        }
    }

    const char *materialName(Material m)
    {
        switch (m)
        {
        case Material::Sand:
            return "Sand";
        case Material::Water:
            return "Water";
        case Material::Wall:
            return "Wall";
        default:
            return "Eraser";
        }
    }

} // namespace

int main()
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Falling Sand");
    window.setVerticalSyncEnabled(false);
    window.setFramerateLimit(TARGET_FPS);

    Simulation sim;
    Material currentMat = Material::Sand;
    int brushRadius = 3;

    sf::Vector2i prevMouse{-1, -1};

    auto stroke = [&](sf::Vector2i cur, Material mat)
    {
        if (prevMouse.x < 0)
        {
            sim.paint(cur.x / CELL_SIZE, cur.y / CELL_SIZE, brushRadius, mat);
        }
        else
        {
            const int dx = cur.x - prevMouse.x;
            const int dy = cur.y - prevMouse.y;
            const int steps = std::max(1, std::max(std::abs(dx), std::abs(dy)) / CELL_SIZE);
            for (int i = 0; i <= steps; ++i)
            {
                const int ix = prevMouse.x + dx * i / steps;
                const int iy = prevMouse.y + dy * i / steps;
                sim.paint(ix / CELL_SIZE, iy / CELL_SIZE, brushRadius, mat);
            }
        }
        prevMouse = cur;
    };

    sf::VertexArray verts(sf::PrimitiveType::Triangles,
                          static_cast<std::size_t>(GRID_WIDTH * GRID_HEIGHT * 6));
    sf::Clock fpsClock;
    float fps = 0.f;
    sf::CircleShape brushOutline;
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

    brushOutline.setFillColor(sf::Color::Transparent);
    brushOutline.setOutlineColor(sf::Color::White);
    brushOutline.setOutlineThickness(1.f);

    while (window.isOpen())
    {
        // ---- Events ----
        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (const auto *kp = event->getIf<sf::Event::KeyPressed>())
            {
                switch (kp->code)
                {
                case sf::Keyboard::Key::Num1:
                    currentMat = Material::Sand;
                    break;
                case sf::Keyboard::Key::Num2:
                    currentMat = Material::Wall;
                    break;
                case sf::Keyboard::Key::Num3:
                    currentMat = Material::Water;
                    break;
                case sf::Keyboard::Key::Num0:
                    currentMat = Material::Empty;
                    break;
                case sf::Keyboard::Key::C:
                    sim.clear();
                    break;
                default:
                    break;
                }
            }
        }

        const bool lmb = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);
        const bool rmb = sf::Mouse::isButtonPressed(sf::Mouse::Button::Right);

        if (lmb || rmb)
        {
            stroke(sf::Mouse::getPosition(window), lmb ? currentMat : Material::Empty);
        }
        else
        {
            prevMouse = {-1, -1}; // reset so next press starts fresh
        }

        // ---- Simulate ----
        sim.update();

        // ---- Build vertex array ----
        for (int y = 0; y < GRID_HEIGHT; ++y)
        {
            for (int x = 0; x < GRID_WIDTH; ++x)
            {
                const Cell cell = sim.getCell(x, y);
                const sf::Color color = getCellColor(cell);
                const std::size_t vi = static_cast<std::size_t>((y * GRID_WIDTH + x) * 6);

                const float px = static_cast<float>(x * CELL_SIZE);
                const float py = static_cast<float>(y * CELL_SIZE);
                const float pw = static_cast<float>(CELL_SIZE);
                const float ph = static_cast<float>(CELL_SIZE);

                // Triangle 1 (top-left half)
                verts[vi + 0] = {{px, py}, color};
                verts[vi + 1] = {{px + pw, py}, color};
                verts[vi + 2] = {{px, py + ph}, color};
                // Triangle 2 (bottom-right half)
                verts[vi + 3] = {{px + pw, py}, color};
                verts[vi + 4] = {{px + pw, py + ph}, color};
                verts[vi + 5] = {{px, py + ph}, color};
            }
        }

        // ---- Render ----
        const float dt = fpsClock.restart().asSeconds();
        if (dt > 0.f)
            fps = 1.f / dt;

        const sf::Vector2i mousePos = sf::Mouse::getPosition(window);
        const int brushCellX = std::clamp(mousePos.x / CELL_SIZE, 0, GRID_WIDTH - 1);
        const int brushCellY = std::clamp(mousePos.y / CELL_SIZE, 0, GRID_HEIGHT - 1);
        const float brushRadiusPixels = brushRadius * static_cast<float>(CELL_SIZE) + CELL_SIZE * 0.5f;
        brushOutline.setRadius(brushRadiusPixels);
        brushOutline.setOrigin({brushRadiusPixels, brushRadiusPixels});
        brushOutline.setPosition({brushCellX * static_cast<float>(CELL_SIZE) + CELL_SIZE * 0.5f,
                                  brushCellY * static_cast<float>(CELL_SIZE) + CELL_SIZE * 0.5f});

        if (hasHudFont)
        {
            hudText.setString(
                "Material: " + std::string(materialName(currentMat)) +
                "\nFPS: " + std::to_string(static_cast<int>(fps + 0.5f)) + "/" + std::to_string(TARGET_FPS) +
                "\nBrush: " + std::to_string(brushRadius) +
                "\nControls: 1 Sand  2 Wall  3 Water  0 Erase  C Clear  RMB Erase");

            const sf::FloatRect bounds = hudText.getLocalBounds();
            hudPanel.setSize({bounds.size.x + 20.f,
                              bounds.size.y + 22.f});
        }

        window.clear(sf::Color::Black);
        window.draw(verts);
        window.draw(brushOutline);
        if (hasHudFont)
        {
            window.draw(hudPanel);
            window.draw(hudText);
        }

        window.display();
    }

    return 0;
}
