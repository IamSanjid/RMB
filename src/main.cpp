#include <iostream>
#include "ClayApplication.hpp"
#include "Config.h"

int main(int, char**) {
    ClayApplication app{};
    if (!app.Initialize(Config::Current()->NAME, Config::Current()->WIDTH,
                        Config::Current()->HEIGHT)) {
        std::cerr << "Couldn't initialize the app" << std::endl;
        std::abort();
    }
    app.Run();
    return 0;
}
