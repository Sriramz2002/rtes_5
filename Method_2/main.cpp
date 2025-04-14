/*
 * GPIO 23 toggle implementation using sysfs interface
 * Build: g++ --std=c++20 -Wall -Werror -pedantic gpio_sysfs.cpp Sequencer.cpp -o gpio_sysfs -pthread -lrt
 */
#include "Sequencer.hpp"
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

// Global termination flag
std::atomic<bool> quit(false);

// GPIO number for pin 23
const char* GPIO_NUM = "535";  // Adjusted for GPIO base 512

// Forward declaration for use in signal handler
void cleanGpio();

// Signal handler for Ctrl+C
void sigHandler(int sig) {
    std::cout << "\nReceived signal " << sig << ", cleaning up..." << std::endl;
    quit = true;
    // Directly call cleanup to ensure GPIO is unexported
    cleanGpio();
}

int setupGpio() {
    // Open export file
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        perror("Export failed");
        return -1;
    }

    // Write GPIO number to export file
    if (write(fd, GPIO_NUM, strlen(GPIO_NUM)) != (ssize_t)strlen(GPIO_NUM)) {
        // EBUSY means the GPIO is already exported - that's ok
        if (errno != EBUSY && errno != EINVAL) {
            perror("Export write failed");
            close(fd);
            return -1;
        }
    }
    close(fd);
    
    // Add a brief delay to allow the system to set up the GPIO
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Set direction to output
    std::string dirPath = "/sys/class/gpio/gpio" + std::string(GPIO_NUM) + "/direction";
    fd = open(dirPath.c_str(), O_WRONLY);
    if (fd == -1) {
        perror("Direction open failed");
        return -1;
    }

    // Write "out" to set as output
    if (write(fd, "out", 3) != 3) {
        perror("Direction write failed");
        close(fd);
        return -1;
    }
    close(fd);

    std::cout << "GPIO 23 (sysfs: " << GPIO_NUM << ") ready" << std::endl;
    return 0;
}

void cleanGpio() {
    // Unexport GPIO when done
    std::cout << "Unexporting GPIO " << GPIO_NUM << "..." << std::endl;
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        perror("Unexport failed");
        return;
    }
    if (write(fd, GPIO_NUM, strlen(GPIO_NUM)) != (ssize_t)strlen(GPIO_NUM)) {
        perror("Unexport write failed");
    } else {
        std::cout << "GPIO 23 (sysfs: " << GPIO_NUM << ") unexported successfully" << std::endl;
    }
    close(fd);
}

void toggleGpio() {
    static int state = 0;
    
    // Open the value file which directly controls the GPIO state
    std::string valPath = "/sys/class/gpio/gpio" + std::string(GPIO_NUM) + "/value";
    int fd = open(valPath.c_str(), O_WRONLY);
    if (fd == -1) {
        perror("Value open failed");
        return;
    }

    // Write 1 or 0 to set the GPIO high or low
    const char* val = state ? "1" : "0";
    if (write(fd, val, 1) != 1) {
        perror("Value write failed");
    }
    close(fd);
    
    // Toggle for next call
    state = !state;
}

int main() {
    // Setup signal handler
    std::signal(SIGINT, sigHandler);
    
    // Initialize GPIO
    if (setupGpio() != 0) {
        std::cerr << "GPIO setup failed" << std::endl;
        return 1;
    }
    
    std::cout << "Starting GPIO 23 toggle using sysfs (Method 2)" << std::endl;
    
    // Create sequencer
    Sequencer seq;

    // Add toggle service with name, function, priority, CPU affinity, and period (ms)
    seq.addService("gpio23_toggle", toggleGpio, 97, 1, 100);
    
    // Start sequencer with 10ms master interval
    seq.startServices(10);
    
    // Main loop
    std::cout << "Toggling every 100ms... Press Ctrl+C to exit" << std::endl;
    while (!quit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Cleanup - may not be reached if signal handler exits program
    seq.stopServices();
    // Not calling cleanGpio() here since it's already called in the signal handler
    
    std::cout << "Program exiting" << std::endl;
    return 0;
}
