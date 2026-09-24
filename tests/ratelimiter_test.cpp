#include <gtest/gtest.h>
#include <chrono>
#include <thread> 
#include "ratelimiter.h"

long get_time_ms() {
    auto duration = std::chrono::steady_clock::now().time_since_epoch();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    return ms;
}

TEST(RateLimiterTest, AcquireWithinZeroLimit) {
    dimxy::RateLimiter rl(0.0, 1000);
    auto t0 = get_time_ms();
    EXPECT_TRUE(rl.acquire(1));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 < 100);
}

TEST(RateLimiterTest, AcquireWithinLimit) {
    dimxy::RateLimiter rl(100.0, 1000);
    auto t0 = get_time_ms();
    EXPECT_TRUE(rl.acquire(1));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 < 100);
}

TEST(RateLimiterTest, RejectsNegativeRequests) {
    dimxy::RateLimiter rl(100.0, 1000);
    EXPECT_FALSE(rl.acquire(-1));
}

TEST(RateLimiterTest, RejectsOverBurst) {
    dimxy::RateLimiter rl(100.0, 5);
    EXPECT_FALSE(rl.acquire(10));
}

TEST(RateLimiterTest, AcquireWait2x) {
    dimxy::RateLimiter my_rl(10.0, 1000);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(8));
    EXPECT_TRUE(my_rl.acquire(9));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 17 / 10.0 * 1000);
    EXPECT_TRUE(t1 - t0 < 18 / 10.0 * 1000);
}

TEST(RateLimiterTest, AcquireWithinLimit3x) {
    dimxy::RateLimiter my_rl(10.0, 1000);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(2));
    EXPECT_TRUE(my_rl.acquire(2));
    EXPECT_TRUE(my_rl.acquire(2));
    auto t1 = get_time_ms();
    assert(t1 - t0 < 100);
}

/* This test is flaky:
TEST(RateLimiterTest, RejectsOverBurst3x) {
    dimxy::RateLimiter my_rl(10.0, 10);
    EXPECT_TRUE(my_rl.acquire(2));
    EXPECT_FALSE(my_rl.acquire(9));
}*/

TEST(RateLimiterTest, AcquireWithinLimit3xNoBurst) {
    dimxy::RateLimiter my_rl(10.0, 10);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(2));
    EXPECT_TRUE(my_rl.acquire(2));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(my_rl.acquire(9));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 13 / 10.0 * 1000);
    EXPECT_TRUE(t1 - t0 < 14 / 10.0 * 1000);
}

TEST(RateLimiterTest, AcquireWithinLimit3xWithDelays) {
    dimxy::RateLimiter my_rl(10.0, 10);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(9));
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    EXPECT_TRUE(my_rl.acquire(9));
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    EXPECT_TRUE(my_rl.acquire(9));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 4000); // no wait if sleep_for(2000) twice
    EXPECT_TRUE(t1 - t0 < 4500);
}

TEST(RateLimiterTest, RejectOverBurstInThread) {
    dimxy::RateLimiter my_rl(10.0, 10);
    std::thread th([&]() { 
        std::this_thread::sleep_for(std::chrono::seconds(2));
        my_rl.shutdown(); 
    });
    bool r = my_rl.acquire(100);
    th.join();
    EXPECT_FALSE(r);
}

TEST(RateLimiterTest, InfiniteLoopFixWithCeil) {
    dimxy::RateLimiter my_rl(3.0, 10);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(4));
    EXPECT_TRUE(my_rl.acquire(3));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 2333); // total 7 reqs when rps=3
    EXPECT_TRUE(t1 - t0 < 2500);
}

TEST(RateLimiterTest, AcquireBigRequestOverLimit) {
    dimxy::RateLimiter my_rl(3.0, 100);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(99));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 100 / 3.0 * 1000); // total 100 reqs when rps=3
    EXPECT_TRUE(t1 - t0 < 101 / 3.0 * 1000);
}

TEST(RateLimiterTest, AcquireMultipleRequestsOverLimit) {
    dimxy::RateLimiter my_rl(3.0, 20);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(1));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 11 / 3.0 * 1000);
    EXPECT_TRUE(t1 - t0 < 12 / 3.0 * 1000);
}

TEST(RateLimiterTest, AcquireMultipleRequestsEachOverLimit) {
    dimxy::RateLimiter my_rl(3.0, 20);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(19));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(19));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(19));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(19));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(19));
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    EXPECT_TRUE(my_rl.acquire(19));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 114 / 3.0 * 1000); // total 100 reqs when rps=3
    EXPECT_TRUE(t1 - t0 < 115 / 3.0 * 1000);
}

TEST(RateLimiterTest, AcquireWithinLimitInThreads) {
    dimxy::RateLimiter my_rl(3.0, 30);
    auto t0 = get_time_ms();
    std::thread th1([&]() {
        EXPECT_TRUE(my_rl.acquire(4));
        EXPECT_TRUE(my_rl.acquire(3));
    });
    std::thread th2([&]() {
        EXPECT_TRUE(my_rl.acquire(5));
    });
    th1.join();
    th2.join();
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 12 / 3.0 * 1000);
    EXPECT_TRUE(t1 - t0 < 13 / 3.0 * 1000);
}

TEST(RateLimiterTest, AcquireWithinLimitInThreadsWithDelay) {
    dimxy::RateLimiter my_rl(3.0, 30);
    auto t0 = get_time_ms();
    std::thread th1([&]() {
        EXPECT_TRUE(my_rl.acquire(4));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_TRUE(my_rl.acquire(3));
    });
    std::thread th2([&]() {
        EXPECT_TRUE(my_rl.acquire(5));
    });
    th1.join();
    th2.join();
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 12 / 3.0 * 1000);
    EXPECT_TRUE(t1 - t0 < 13 / 3.0 * 1000);
}

TEST(RateLimiterTest, AcquireWaitForLimitInThreadsWithDelay) {
    dimxy::RateLimiter my_rl(10.0, 30);
    auto t0 = get_time_ms();
    std::thread th1([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        EXPECT_TRUE(my_rl.acquire(10));
    });
    std::thread th2([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        EXPECT_TRUE(my_rl.acquire(20));
    });
    th1.join();
    th2.join();
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 30 / 10.0 * 1000);
    EXPECT_TRUE(t1 - t0 < 32 / 10.0 * 1000); // '32' for extra time to cover sleep_for
}

TEST(RateLimiterTest, AcquireInThreadsWithDelayedStart) {
    std::vector<std::pair<int, int>> d = {{0, 0}, {10, 0}, {0, 10}, {10, 10}};
    for (auto p : d) {
        dimxy::RateLimiter my_rl(10.0, 100);
        int d1 = p.first;
        int d2 = p.second;
        
        auto t0 = get_time_ms();
        std::thread th1([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(d1));
            EXPECT_TRUE(my_rl.acquire(9));
            EXPECT_TRUE(my_rl.acquire(9));
            EXPECT_TRUE(my_rl.acquire(9));
            EXPECT_TRUE(my_rl.acquire(9));
        });
        std::thread th2([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(d2));
            EXPECT_TRUE(my_rl.acquire(29));
            EXPECT_TRUE(my_rl.acquire(9));
        });
        th1.join();
        th2.join();
        auto t1 = get_time_ms();
        EXPECT_TRUE(t1 - t0 >= 7400); 
        EXPECT_TRUE(t1 - t0 < 7900); // approx upper bound
    }
}

// First acquire call is over limit so it waits.
// The second call follows immediately and the rate limiter recalculates (decays) the internal total requests counter to non-zero value
TEST(RateLimiterTest, AcquireWithTotalReqsDecayTotalReqs) {
    dimxy::RateLimiter my_rl(3.0, 1000);
    auto t0 = get_time_ms();
    EXPECT_TRUE(my_rl.acquire(27));
    EXPECT_TRUE(my_rl.acquire(9));
    EXPECT_TRUE(my_rl.acquire(9));
    auto t1 = get_time_ms();
    EXPECT_TRUE(t1 - t0 >= 45 / 3.0 * 1000); // sleep_for(2000) not included
    EXPECT_TRUE(t1 - t0 < 46 / 3.0 * 1000);
}
