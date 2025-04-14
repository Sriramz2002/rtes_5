/*
 * GPIO 23 toggle implementation using sysfs interface
 * Build: g++ --std=c++23 -Wall -Werror -pedantic gpio_sysfs.cpp -o gpio_sysfs
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
 
 // Signal handler for Ctrl+C
 void sigHandler(int sig) {
     quit = true;
 }
 
 // GPIO number for pin 23
 const char* GPIO_NUM = "23";
 
 int setupGpio() {
     // Open export file
     // The export file is used to make a GPIO available in userspace
     int fd = open("/sys/class/gpio/export", O_WRONLY);
     if (fd == -1) {
         perror("Export failed");
         return -1;
     }
 
     // Write GPIO number to export file
     // This creates the /sys/class/gpio/gpioXX/ directory
     if (write(fd, GPIO_NUM, strlen(GPIO_NUM)) != (ssize_t)strlen(GPIO_NUM)) {
         // EBUSY means the GPIO is already exported - that's ok
         if (errno != EBUSY && errno != EINVAL) {
             perror("Export write failed");
             close(fd);
             return -1;
         }
     }
     close(fd);
 
     // Set direction to output
     // The direction file controls whether GPIO is input or output
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
 
     std::cout << "GPIO " << GPIO_NUM << " ready" << std::endl;
     return 0;
 }
 
 void cleanGpio() {
     // Unexport GPIO when done
     // This removes the GPIO from userspace control
     int fd = open("/sys/class/gpio/unexport", O_WRONLY);
     if (fd == -1) {
         perror("Unexport failed");
         return;
     }
     write(fd, GPIO_NUM, strlen(GPIO_NUM));
     close(fd);
     std::cout << "GPIO " << GPIO_NUM << " cleaned up" << std::endl;
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
     
     std::cout << "Starting GPIO toggle (sysfs method)" << std::endl;
     
     // Create sequencer
     Sequencer seq;
 
     // Add toggle service (100ms period)
     seq.addService("gpio23_toggle", toggleGpio, 97, 1, 100);
     seq.startServices(10);  // 10ms master interval
     
     // Main loop
     std::cout << "Press Ctrl+C to exit" << std::endl;
     while (!quit) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
     }
     
     // Cleanup
     seq.stopServices();
     cleanGpio();
     
     return 0;
 }
