#include <iostream>

#include "agent.hpp"
#include "path_utils.hpp"

int main() {
    try {
        SCCA app(pathutil::currentPath());
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "SCCA fatal error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
