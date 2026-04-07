#include <SFML/Graphics.hpp>
#include <optional>
#include <cstdlib>
#include <ctime>
#include <string>

#include "simulation.h"
#include "types.h"

namespace
{
const char *getMaterialName(CellType material)
{
    switch (material)
    {
    case CellType::Sand:
        return "Sand";
    case CellType::Wall:
        return "Wall";
    case CellType::Water:
        return "Water";
    case CellType::Empty:
    default:
        return "Eraser";
    }
}

sf::Color getCellColor(CellType cell)
{
    switch (cell)
    {
    case CellType::Sand:
        return sf::Color::Yellow;
    case CellType::Wall:
        return sf::Color(120, 120, 120);
    case CellType::Water:
        return sf::Color(50, 120, 255);
    case CellType::Empty:
    default:
        return sf::Color::Black;
    }
}
} // namespace

int main()
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Falling Sand");
    window.setFramerateLimit(60);

    Simulation simulation;
    CellType currentMaterial = CellType::Sand;
    int brushRadius = 2;

    sf::RectangleShape cellShape(sf::Vector2f(static_cast<float>(CELL_SIZE), static_cast<float>(CELL_SIZE)));

    while (window.isOpen())
    {
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            if (event->is<sf::Event::KeyPressed>())
            {
                const auto &k = event->getIf<sf::Event::KeyPressed>()->code;

                if (k == sf::Keyboard::Key::Num1)
                    currentMaterial = CellType::Sand;
                if (k == sf::Keyboard::Key::Num2)
                    currentMaterial = CellType::Wall;
                if (k == sf::Keyboard::Key::Num3)
                    currentMaterial = CellType::Water;
                if (k == sf::Keyboard::Key::Num0)
                    currentMaterial = CellType::Empty;
                if (k == sf::Keyboard::Key::C)
                    simulation.clear();
            }
        }

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            const sf::Vector2i mousePos = sf::Mouse::getPosition(window);
            const int gridX = mousePos.x / CELL_SIZE;
            const int gridY = mousePos.y / CELL_SIZE;

            simulation.paint(gridX, gridY, brushRadius, currentMaterial);
        }

        simulation.update();

        window.clear(sf::Color::Black);

        for (int y = 0; y < GRID_HEIGHT; ++y)
        {
            for (int x = 0; x < GRID_WIDTH; ++x)
            {
                const CellType cell = simulation.getCell(x, y);

                if (cell == CellType::Empty)
                {
                    continue;
                }

                cellShape.setFillColor(getCellColor(cell));
                cellShape.setPosition(sf::Vector2f(
                    static_cast<float>(x * CELL_SIZE),
                    static_cast<float>(y * CELL_SIZE)));
                window.draw(cellShape);
            }
        }

        window.setTitle(
            std::string("Falling Sand - Tool: ") + getMaterialName(currentMaterial) +
            " (1=Sand, 2=Wall, 3=Water, 0=Eraser, C=Clear)");

        window.display();
    }

    return 0;
}
