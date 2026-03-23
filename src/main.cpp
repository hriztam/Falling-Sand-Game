#include <SFML/Graphics.hpp>
#include <optional>

int main()
{
    // Create window
    sf::RenderWindow window(sf::VideoMode({800, 600}), "Falling Sand");

    // Main loop
    while (window.isOpen())
    {
        // Event handling
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
        }

        // Clear screen
        window.clear(sf::Color::Black);

        // Green Ball
        sf::CircleShape shape(50);
        shape.setFillColor(sf::Color::Green);

        window.draw(shape);

        // Display frame
        window.display();
    }

    return 0;
}
