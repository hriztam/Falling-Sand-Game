#include <SFML/Graphics.hpp>
#include <optional>
#include <vector>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int CELL_SIZE = 4;

const int GRID_WIDTH = WINDOW_WIDTH / CELL_SIZE;
const int GRID_HEIGHT = WINDOW_HEIGHT / CELL_SIZE;

enum class CellType
{
    Empty,
    Sand
};

int index(int x, int y)
{
    return y * GRID_WIDTH + x;
}

bool inBounds(int x, int y)
{
    return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Falling Sand");
    window.setFramerateLimit(60);

    std::vector<CellType> grid(GRID_WIDTH * GRID_HEIGHT, CellType::Empty);

    while (window.isOpen())
    {
        while (const std::optional<sf::Event> event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

        // Paint sand with mouse
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);

            int gridX = mousePos.x / CELL_SIZE;
            int gridY = mousePos.y / CELL_SIZE;

            if (inBounds(gridX, gridY))
            {
                grid[index(gridX, gridY)] = CellType::Sand;
            }
        }

        // Sand falling logic
        for (int y = GRID_HEIGHT - 2; y >= 0; y--)
        {
            for (int x = 0; x < GRID_WIDTH; x++)
            {
                if (grid[index(x, y)] != CellType::Sand)
                {
                    continue;
                }

                // Try moving down
                if (inBounds(x, y + 1) && grid[index(x, y + 1)] == CellType::Empty)
                {
                    grid[index(x, y + 1)] = CellType::Sand;
                    grid[index(x, y)] = CellType::Empty;
                }
                // Try moving down-left
                else if (inBounds(x - 1, y + 1) && grid[index(x - 1, y + 1)] == CellType::Empty)
                {
                    grid[index(x - 1, y + 1)] = CellType::Sand;
                    grid[index(x, y)] = CellType::Empty;
                }
                // Try moving down-right
                else if (inBounds(x + 1, y + 1) && grid[index(x + 1, y + 1)] == CellType::Empty)
                {
                    grid[index(x + 1, y + 1)] = CellType::Sand;
                    grid[index(x, y)] = CellType::Empty;
                }
            }
        }

        window.clear(sf::Color::Black);

        sf::RectangleShape cellShape(sf::Vector2f(static_cast<float>(CELL_SIZE), static_cast<float>(CELL_SIZE)));

        for (int y = 0; y < GRID_HEIGHT; y++)
        {
            for (int x = 0; x < GRID_WIDTH; x++)
            {
                CellType cell = grid[index(x, y)];

                if (cell == CellType::Empty)
                {
                    continue;
                }

                if (cell == CellType::Sand)
                {
                    cellShape.setFillColor(sf::Color::Yellow);
                }

                cellShape.setPosition(sf::Vector2f(
                    static_cast<float>(x * CELL_SIZE),
                    static_cast<float>(y * CELL_SIZE)));
                window.draw(cellShape);
            }
        }

        window.display();
    }

    return 0;
}
