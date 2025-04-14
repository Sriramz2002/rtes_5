/*
 * GPIO 23 toggle using ioctl interface (Method 3)
 * Build: g++ --std=c++23 -Wall -Werror -pedantic gpio_ioctl.cpp -o gpio_ioctl
 */

 #include "Sequencer.hpp"
 #include <csignal>
 #include <chrono>
 #include <thread>
 #include <atomic>
 #include <cstdio>
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/ioctl.h>
 #include <linux/gpio.h>
 #include <cstring>
 #include <iostream>
 
 // Quit signal flag
 std::atomic<bool> stop(false);
 
 // Signal handler
 void sigCatch(int sig) {
     stop = true;
 }
 
 // File descriptors for GPIO access
 int chip = -1;
 int line = -1;
 
 bool setupGpio() {
     // Open GPIO chip device file
     // This uses the character device interface introduced in Linux 4.8+
     chip = open("/dev/gpiochip0", O_RDWR);
     if (chip < 0) {
         perror("Open GPIO chip failed");
         return false;
     }
 
     // Set up GPIO request structure
     // IOCTL is used to get a handle to the specific GPIO line
     struct gpiohandle_request req = {};
     req.lineoffsets[0] = 23;  // Using GPIO 23
     req.flags = GPIOHANDLE_REQUEST_OUTPUT;  // Set as output
     req.lines = 1;  // Request 1 line
     req.default_values[0] = 0;  // Initial value low
     std::strcpy(req.consumer_label, "gpio_toggle");  // Name our controller
 
     // Request GPIO line handle via ioctl
     // This gives us a file descriptor that controls the specific GPIO line
     if (ioctl(chip, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
         perror("GPIO line request failed");
         close(chip);
         chip = -1;
         return false;
     }
 
     // Store the line file descriptor
     line = req.fd;
     std::cout << "GPIO 23 initialized via ioctl" << std::endl;
     return true;
 }
 
 void cleanGpio() {
     // Close file descriptors
     if (line >= 0) close(line);
     if (chip >= 0) close(chip);
     line = -1;
     chip = -1;
     std::cout << "GPIO 23 resources released" << std::endl;
 }
 
 void toggleGpio() {
     static bool state = false;
     if (line < 0) return;  // Safety check
 
     // Data structure for setting GPIO values
     struct gpiohandle_data data = {};
     data.values[0] = state ? 1 : 0;
     
     // Set GPIO value using ioctl
     // This directly communicates with the kernel's GPIO subsystem
     ioctl(line, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
 
     // Toggle state for next call
     state = !state;
 }
 
 int main() {
     // Register signal handler
     std::signal(SIGINT, sigCatch);
     
     std::cout << "Starting GPIO 23 toggle (Method 3: ioctl)" << std::endl;
     
     // Initialize GPIO with ioctl
     if (!setupGpio()) {
         std::cerr << "Failed to set up GPIO 23" << std::endl;
         return 1;
     }
     
     // Create sequencer and add service
     Sequencer seq;
     seq.addService(toggleGpio, 1, 97, 100);
     seq.startServices();
     
     std::cout << "Toggling GPIO 23 every 100ms... Press Ctrl+C to exit" << std::endl;
     
     // Wait for termination signal
     while (!stop) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
     }
     
     // Clean up
     seq.stopServices();
     cleanGpio();
     
     std::cout << "Program terminated" << std::endl;
     return 0;
 }