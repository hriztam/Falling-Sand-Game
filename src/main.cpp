#include <SFML/Graphics.hpp>
#include <optional>
#include <vector>
#include <cstdlib>
#include <ctime>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int CELL_SIZE = 4;

const int GRID_WIDTH = WINDOW_WIDTH / CELL_SIZE;
const int GRID_HEIGHT = WINDOW_HEIGHT / CELL_SIZE;

int brushRadius = 2;

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
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    sf::RenderWindow window(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Falling Sand");
    window.setFramerateLimit(60);

    std::vector<CellType> grid(GRID_WIDTH * GRID_HEIGHT, CellType::Empty);

    CellType currentMaterial = CellType::Sand;

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
                int r = brushRadius;
                int r2 = r * r; // radius squared

                for (int dy = -r; dy <= r; dy++)
                {
                    for (int dx = -r; dx <= r; dx++)
                    {
                        // only inside circle: dx^2 + dy^2 <= r^2
                        if (dx * dx + dy * dy > r2)
                            continue;

                        int nx = gridX + dx;
                        int ny = gridY + dy;

                        if (inBounds(nx, ny))
                        {
                            grid[index(nx, ny)] = currentMaterial;
                        }
                    }
                }
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

                if (inBounds(x, y + 1) && grid[index(x, y + 1)] == CellType::Empty)
                {
                    std::swap(grid[index(x, y)], grid[index(x, y + 1)]);
                }
                else
                {
                    bool tryLeftFirst = (std::rand() % 2 == 0);

                    if (tryLeftFirst)
                    {
                        if (inBounds(x - 1, y + 1) && grid[index(x - 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x - 1, y + 1)]);
                        }
                        else if (inBounds(x + 1, y + 1) && grid[index(x + 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x + 1, y + 1)]);
                        }
                    }
                    else
                    {
                        if (inBounds(x + 1, y + 1) && grid[index(x + 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x + 1, y + 1)]);
                        }
                        else if (inBounds(x - 1, y + 1) && grid[index(x - 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x - 1, y + 1)]);
                        }
                    }
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
