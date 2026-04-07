#include <SFML/Graphics.hpp>
#include <optional>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int CELL_SIZE = 4;

const int GRID_WIDTH = WINDOW_WIDTH / CELL_SIZE;
const int GRID_HEIGHT = WINDOW_HEIGHT / CELL_SIZE;

int brushRadius = 2;

enum class CellType
{
    Empty,
    Sand,
    Wall,
    Water
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

    sf::RectangleShape cellShape(sf::Vector2f(static_cast<float>(CELL_SIZE), static_cast<float>(CELL_SIZE)));

    bool scanLeftToRight = true;

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
            }
        }

        auto paintBrushAtMouse = [&](CellType writeType)
        {
            sf::Vector2i mousePos = sf::Mouse::getPosition(window);

            int gridX = mousePos.x / CELL_SIZE;
            int gridY = mousePos.y / CELL_SIZE;

            if (inBounds(gridX, gridY))
            {
                int r = brushRadius;

                for (int dy = -r; dy <= r; dy++)
                {
                    for (int dx = -r; dx <= r; dx++)
                    {
                        int nx = gridX + dx;
                        int ny = gridY + dy;

                        if (inBounds(nx, ny))
                        {
                            grid[index(nx, ny)] = writeType;
                        }
                    }
                }
            }
        };

        // Paint with mouse (Empty acts as the eraser).
        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left))
        {
            paintBrushAtMouse(currentMaterial);
        }

        const int xStart = scanLeftToRight ? 0 : (GRID_WIDTH - 1);
        const int xEndExclusive = scanLeftToRight ? GRID_WIDTH : -1;
        const int xStep = scanLeftToRight ? 1 : -1;

        // Sand falling logic
        for (int y = GRID_HEIGHT - 2; y >= 0; y--)
        {

            for (int x = xStart; x != xEndExclusive; x += xStep)
            {
                if (grid[index(x, y)] != CellType::Sand)
                {
                    continue;
                }

                if (grid[index(x, y + 1)] == CellType::Empty)
                {
                    std::swap(grid[index(x, y)], grid[index(x, y + 1)]);
                }
                else
                {
                    bool tryLeftFirst = (std::rand() % 2 == 0);

                    if (tryLeftFirst)
                    {
                        if (x > 0 && grid[index(x - 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x - 1, y + 1)]);
                        }
                        else if (x + 1 < GRID_WIDTH && grid[index(x + 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x + 1, y + 1)]);
                        }
                    }
                    else
                    {
                        if (x + 1 < GRID_WIDTH && grid[index(x + 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x + 1, y + 1)]);
                        }
                        else if (x > 0 && grid[index(x - 1, y + 1)] == CellType::Empty)
                        {
                            std::swap(grid[index(x, y)], grid[index(x - 1, y + 1)]);
                        }
                    }
                }
            }
        }

        // Water movement logic
        for (int y = GRID_HEIGHT - 2; y >= 0; y--)
        {
            for (int x = xStart; x != xEndExclusive; x += xStep)
            {
                if (grid[index(x, y)] != CellType::Water)
                {
                    continue;
                }

                // Try moving down
                if (inBounds(x, y + 1) && grid[index(x, y + 1)] == CellType::Empty)
                {
                    grid[index(x, y + 1)] = CellType::Water;
                    grid[index(x, y)] = CellType::Empty;
                }
                else
                {
                    bool tryLeftFirst = (std::rand() % 2 == 0);

                    // Try diagonal down-left / down-right
                    if (tryLeftFirst)
                    {
                        if (inBounds(x - 1, y + 1) && grid[index(x - 1, y + 1)] == CellType::Empty)
                        {
                            grid[index(x - 1, y + 1)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                        else if (inBounds(x + 1, y + 1) && grid[index(x + 1, y + 1)] == CellType::Empty)
                        {
                            grid[index(x + 1, y + 1)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                        // Try sideways
                        else if (inBounds(x - 1, y) && grid[index(x - 1, y)] == CellType::Empty)
                        {
                            grid[index(x - 1, y)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                        else if (inBounds(x + 1, y) && grid[index(x + 1, y)] == CellType::Empty)
                        {
                            grid[index(x + 1, y)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                    }
                    else
                    {
                        if (inBounds(x + 1, y + 1) && grid[index(x + 1, y + 1)] == CellType::Empty)
                        {
                            grid[index(x + 1, y + 1)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                        else if (inBounds(x - 1, y + 1) && grid[index(x - 1, y + 1)] == CellType::Empty)
                        {
                            grid[index(x - 1, y + 1)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                        else if (inBounds(x + 1, y) && grid[index(x + 1, y)] == CellType::Empty)
                        {
                            grid[index(x + 1, y)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                        else if (inBounds(x - 1, y) && grid[index(x - 1, y)] == CellType::Empty)
                        {
                            grid[index(x - 1, y)] = CellType::Water;
                            grid[index(x, y)] = CellType::Empty;
                        }
                    }
                }
            }
        }

        scanLeftToRight = !scanLeftToRight;

        window.clear(sf::Color::Black);

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
                else if (cell == CellType::Wall)
                {
                    cellShape.setFillColor(sf::Color(120, 120, 120));
                }
                else if (cell == CellType::Water)
                {
                    cellShape.setFillColor(sf::Color(50, 120, 255));
                }

                cellShape.setPosition(sf::Vector2f(
                    static_cast<float>(x * CELL_SIZE),
                    static_cast<float>(y * CELL_SIZE)));
                window.draw(cellShape);
            }
        }

        const char *materialName = (currentMaterial == CellType::Sand) ? "Sand" : "Eraser";
        window.setTitle(std::string("Falling Sand - Tool: ") + materialName + " (1=Sand, 2=Eraser)");

        window.display();
    }

    return 0;
}
