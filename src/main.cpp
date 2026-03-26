#include <iostream>
#include "Application.h"
#include "Config.h"

int main(int, char**) {
    Application app{};
    if (!app.Initialize(Config::Current()->NAME, Config::Current()->WIDTH,
                        Config::Current()->HEIGHT)) {
        std::cerr << "Couldn't initialize the app" << std::endl;
        std::abort();
    }
    app.Run();
    return 0;
}