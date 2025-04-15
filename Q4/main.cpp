#include "Sequencer.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>

// Method 1: Toggle GPIO using shell command with pinctrl
void toggleGpio23ShellCmd()
{
    static bool toggle = false;
    if (toggle) {
        std::system("pinctrl set 23 op dh");  // Set GPIO 23 High
    } else {
        std::system("pinctrl set 23 op dl");  // Set GPIO 23 Low
    }
    toggle = !toggle;
}

int main()
{
    std::cout << "Starting GPIO Toggling Demo with Method 1 (shell command)\n";

    Sequencer seq;

    // Add our GPIO toggle service with 100ms period
    // Using priority 99 (high) and CPU affinity 0
    seq.addService("gpio23Toggle", toggleGpio23ShellCmd, /*priority=*/99, /*cpuAffinity=*/0, /*periodMs=*/100);

    // Master alarm ticks every 10 ms (provides good resolution for our 100ms service)
    seq.startServices(/*masterIntervalMs=*/10);

    // Main thread just waits for SIGINT (Ctrl+C)
    std::cout << "Press Ctrl+C to stop and view statistics...\n";
    while(true) { 
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
    }
    
    return 0;
}
