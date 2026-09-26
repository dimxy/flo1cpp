# Flo1cpp library
My free lib with a few C++ tools.

Currently there is only one tool in the lib: rate limiter.

### Rate limiter
Multithreaded black box rate limiter, to protect your API from overload.

### Installation
Set this repo as a dependency in your project (or, simply clone the repo and `#include "ratelimiter.h"`).<br>
Check RateLimiter class doc comment and the examples dir for usage.

### C++ version
CPP version: C++11 and up.<br>
CPP version for google tests: C++14 and up.<br>

### How to build and run tests
Building tests:
```
mkdir build
cmake -B build --fresh  && cmake --build build 
```

Running tests:
```
./build/unit_tests
./build/unit_tests --gtest_filter=RateLimiterTest.AcquireWithinZeroLimit
```

---
[**LICENSE**](./LICENSE)
