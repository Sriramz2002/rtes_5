/*
 * GPIO 23 toggle using direct memory mapping (Method 4)
 * Build: g++ --std=c++23 -Wall -Werror -pedantic gpio_mmap.cpp -o method4
 * 
 * This implementation directly maps GPIO registers into memory space
 * for the fastest possible GPIO control with no system call overhead.
 */

#include "Sequencer.hpp"
#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>

// Quit flag
std::atomic<bool> quit(false);

// Signal handler
void sigHandler(int sig) {
    quit = true;
}

// For Raspberry Pi 4, the GPIO base address
// Adjust this value depending on your hardware
#define GPIO_BASE_ADDR  0xFE200000
#define BLOCK_SIZE      (4 * 1024)

// Register offsets (each is 4 bytes)
#define GPFSEL0  0  // 0x00 /4  - Function select registers
#define GPSET0   7  // 0x1C /4  - Pin output set registers
#define GPCLR0   10 // 0x28 /4  - Pin output clear registers

// Global pointer to mapped GPIO registers
static volatile uint32_t* gpio = nullptr;
static bool initialized = false;

// Initialize GPIO23 as output via direct memory mapping
bool setupGpio() {
    // Open /dev/mem for direct memory access (requires root privileges)
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        std::cerr << "Failed to open /dev/mem (need sudo?)" << std::endl;
        return false;
    }

    // Map GPIO registers into our address space
    void* map = mmap(
        nullptr,                // Let kernel choose the address
        BLOCK_SIZE,             // Size to map
        PROT_READ | PROT_WRITE, // Allow read/write access
        MAP_SHARED,             // Share with other processes
        fd,                     // File descriptor
        GPIO_BASE_ADDR          // Physical address to map
    );
    
    // Close fd after mapping (no longer needed)
    close(fd);

    if (map == MAP_FAILED) {
        std::cerr << "mmap failed: " << strerror(errno) << std::endl;
        return false;
    }

    // Set global pointer to mapped memory
    gpio = reinterpret_cast<volatile uint32_t*>(map);

    // Configure GPIO23 as output
    // Calculate which GPFSEL register controls pin 23
    int reg = GPFSEL0 + (23 / 10);  // For GPIO23, this is GPFSEL2
    int shift = (23 % 10) * 3;      // Bit position within register
    
    // Clear the 3 bits for this pin
    uint32_t mask = 0b111 << shift;
    uint32_t value = gpio[reg];
    value &= ~mask;
    
    // Set as output (001)
    value |= (0b001 << shift);
    gpio[reg] = value;

    initialized = true;
    std::cout << "GPIO 23 initialized via direct memory mapping" << std::endl;
    return true;
}

// Toggle GPIO23 using direct register access
void toggleGpio() {
    if (!initialized) return;
    
    static bool state = false;
    state = !state;
    
    if (state) {
        // Set GPIO23 high by setting bit in GPSET0
        gpio[GPSET0] = (1 << 23);
    } else {
        // Set GPIO23 low by setting bit in GPCLR0
        gpio[GPCLR0] = (1 << 23);
    }
}

// Clean up mapped memory
void cleanupGpio() {
    if (gpio) {
        munmap((void*)gpio, BLOCK_SIZE);
        gpio = nullptr;
        initialized = false;
        std::cout << "GPIO mapping released" << std::endl;
    }
}

int main() {
    // Register signal handler
    std::signal(SIGINT, sigHandler);
    
    std::cout << "Starting GPIO 23 toggle (Method 4: direct memory mapping)" << std::endl;
    
    // Set up memory-mapped GPIO
    if (!setupGpio()) {
        std::cerr << "Failed to set up GPIO" << std::endl;
        return 1;
    }
    
    // Create sequencer
    Sequencer seq;
    
    // Add toggle service (100ms period)
    seq.addService("tglGpio",toggleGpio, 1, 97, 100);
    
    // Start sequencer
    seq.startServices(10);
    
    std::cout << "Toggling GPIO 23 every 100ms... Press Ctrl+C to exit" << std::endl;
    
    // Wait for Ctrl+C
    while (!quit) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Clean up
    seq.stopServices();
    cleanupGpio();
    
    std::cout << "Program terminated" << std::endl;
    return 0;
}
