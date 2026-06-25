#include <zenoh.h>
#include <iostream>

int main() {
    std::cout << "Testing Zenoh C link..." << std::endl;
    z_owned_config_t config;
    z_result_t res = z_config_default(&config);
    if (res != Z_OK) {
        std::cerr << "Failed to get default Zenoh config: " << res << std::endl;
        return 1;
    }
    std::cout << "Zenoh config initialized successfully." << std::endl;
    z_config_drop(z_move(config));
    return 0;
}
