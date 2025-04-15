/*
 * GPIO 23 toggle implementation using libgpiod (Method 3)
 * Build: g++ --std=c++20 -Wall -Werror -pedantic main.cpp Sequencer.cpp -o gpio_method3 -pthread -lrt -lgpiod
 */

#include "Sequencer.hpp"
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <gpiod.h>   // libgpiod



// RAII class for GPIO handling with libgpiod
class GpioHandler {
private:
    gpiod_chip* chip;
    gpiod_line* line;
    bool initialized;
    int pin_number;
    
public:
    // Constructor
    explicit GpioHandler(int pin) : 
        chip(nullptr), 
        line(nullptr), 
        initialized(false),
        pin_number(pin) {}
    
    // Initialize GPIO
    bool init() {
        // Open the default GPIO chip
        chip = gpiod_chip_open_by_name("gpiochip0");
        if (!chip) {
            std::cerr << "Failed to open gpiochip0" << std::endl;
            return false;
        }

        // Get the specified GPIO line
        line = gpiod_chip_get_line(chip, pin_number);
        if (!line) {
            std::cerr << "Failed to get GPIO line " << pin_number << std::endl;
            gpiod_chip_close(chip);
            chip = nullptr;
            return false;
        }

        // Request the line as output with initial value 0
        int req = gpiod_line_request_output(line, "gpio_toggle", 0);
        if (req < 0) {
            std::cerr << "Failed to request GPIO line as output" << std::endl;
            gpiod_chip_close(chip);
            chip = nullptr;
            line = nullptr;
            return false;
        }

        initialized = true;
        std::cout << "GPIO " << pin_number << " initialized using libgpiod" << std::endl;
        return true;
    }
    
    // Toggle GPIO state
    void toggle() {
        if (!initialized) {
            std::cerr << "GPIO not initialized" << std::endl;
            return;
        }
        
        static bool state = false;
        state = !state;
        
        // Set the GPIO value
        int ret = gpiod_line_set_value(line, state ? 1 : 0);
        if (ret < 0) {
            std::cerr << "Error setting GPIO value" << std::endl;
        }
    }
    
    // Release GPIO resources
    void cleanup() {
        if (!initialized) return;
        
        std::cout << "Cleaning up GPIO " << pin_number << "..." << std::endl;
        
        // Set to input mode first (safer default state)
        gpiod_line_release(line);
        
        // Close the GPIO chip
        gpiod_chip_close(chip);
        
        line = nullptr;
        chip = nullptr;
        initialized = false;
        std::cout << "GPIO " << pin_number << " cleaned up" << std::endl;
    }
    
    // Destructor
    ~GpioHandler() {
        cleanup();
    }
};

// Global GPIO handler
GpioHandler gpio(23);  // Using GPIO 23

// Function to toggle GPIO that will be called by sequencer
void toggleGpio() {
    gpio.toggle();
}

// GPIO cleanup function that will be called before exit
void cleanGpio() {
    gpio.cleanup();
}

int main()
 {
    // Initialize GPIO
    if (!gpio.init()) {
        std::cerr << "GPIO setup failed" << std::endl;
        return 1;
    }
    
    std::cout << "Starting GPIO 23 toggle (Method 3: libgpiod)" << std::endl;
    
    // Create sequencer
    Sequencer seq;

    // Add toggle service with name, function, priority, CPU affinity, and period (ms)
    seq.addService("gpio23_toggle", toggleGpio, 97, 1, 100);
    
    // Start sequencer with 10ms master interval
    seq.startServices(10);
    
    // Main loop
    std::cout << "Toggling GPIO 23 every 100ms... Press Ctrl+C to exit" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Stop sequencer
    seq.stopServices();
    
    return 0;
}
