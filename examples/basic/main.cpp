#include <Pure/Pure.h>

#include <iostream>

int main() {
    Pure::Window window("Pure", 1024, 768);
    if (!window.valid() || !window.load("web/index.html")) {
        std::cerr << "Failed to create Pure window.\n";
        return 1;
    }

    int count = 0;
    window.on_message([&window, &count](const std::string& message) {
        ++count;
        std::cout << "JavaScript: " << message << '\n';
        window.post_message("C++ acknowledged message #" + std::to_string(count) + ": " + message);
    });
    window.show();
    window.run();
}
