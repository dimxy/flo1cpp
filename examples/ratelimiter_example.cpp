#include <iostream>
#include "../include/ratelimiter.h"

void print_time_ms(const char * pref) {
    auto duration = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    std::cout << pref << ms << std::endl;
}

int main() {
    dimxy::RateLimiter rl(3.0, 1000);
    print_time_ms("starting at:");
    rl.acquire(10);
    print_time_ms("acquired 10 at:");
}
