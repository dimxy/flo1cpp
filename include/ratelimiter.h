#pragma once

#include <iostream>
#include <vector>
#include <deque>
#include <list>
#include <utility>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <assert.h>
#include <thread> 
#include <cmath>

#ifdef RATELIMITER_DEBUG
  #define RL_DEBUG(x) do { \
      std::cerr << "[RateLimiter tid=" << std::this_thread::get_id() << "] " << x << std::endl; \
  } while (0)
#else
  #define RL_DEBUG(x) do {} while (0)
#endif

namespace dimxy {
    /// Multithreaded rate limiter protecting a service or API from overloading, with support of rps and burst limits.
    /// Returns true or waits until the current rps meets the limit.
    /// Returns false immediately if total requests in one ms exceeds the burst limit.
    ///
    /// Usage:
    /// ```
    /// dimxy::RateLimiter rl(3.0, 1000);
    /// if (rl.acquire(100)) {
    ///     <call your system>
    /// }
    /// ```
    class RateLimiter {
    public:
        RateLimiter(double rps, int burst) : 
            m_total_reqs(0L), m_burst_reqs(0L), m_rps(rps), m_burst_cap(burst), m_tlast_ms(0L), m_enters(0), init_shutdown(false) {        
            m_t0 = std::chrono::steady_clock::now();
            m_tlast = m_t0;
            m_win_size = 1000;
        }
        ~RateLimiter() {
            shutdown();
        }

        bool acquire(int n_reqs) {
            std::unique_lock<std::mutex> lck(m_mtx);
            if (init_shutdown || n_reqs < 0) return false;
            if (m_rps <= 0) return true;
            ++ m_enters;
            RL_DEBUG(__func__ << " enterred" );
            bool ret = false;
            try {
                ret = acquire_impl(n_reqs, lck);
            } 
            catch(...) {
                // pass
            }
            RL_DEBUG(__func__ << " returns=" << std::boolalpha << ret);
            -- m_enters;
            cv.notify_all();
            cv_shutdown.notify_all();
            return ret;
        }

        void shutdown() {
            std::unique_lock<std::mutex> lck(m_mtx);
            init_shutdown = true;
            cv.notify_all();

            cv_shutdown.wait(lck, [this](){ return this->all_finished(); } );
            RL_DEBUG("Shutdown complete.");
        }

        bool test_no_pending() {
            return true;
        }

    private:
        bool acquire_impl(int n_reqs, std::unique_lock<std::mutex> &lck) {
            std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();
            
            if (std::chrono::duration_cast<std::chrono::milliseconds> (t1 - m_t0).count() > m_win_size) {
                // Reset counters, if they are stalled
                std::chrono::duration<long, std::milli> d_win_size(m_win_size);
                std::chrono::steady_clock::time_point t_win_left = t1 - d_win_size;
                long elapsed = std::chrono::duration_cast<std::chrono::milliseconds> (t_win_left - m_t0).count();
                long reqs_to_decay = (long)((double)elapsed * m_rps / 1000.0);
                
                #ifdef RATELIMITER_DEBUG
                long m_total_reqs_prev = m_total_reqs;
                #endif
                // When m_t0 is reset to the beginning of the window, we must also decrease the m_total_reqs (if it is greater than rps), 
                // proportionally to the elapsed time.
                // This is backwards to the case when acquire call results m_total_reqs over rps pushes the deadline and drops m_total_reqs to 0.
                // So here we pull m_t0 and decrease m_total_reqs in a similar way, and not necessary to zero, because:
                // another thread may call while the first thread is waiting for deadline;
                // when a call comes right after deadline in the same thread, m_total_reqs can't go to 0 as m_t0 would lag from now by m_window_size 
                // (so it would make to 1 sec * m_rps).
                m_total_reqs = reqs_to_decay <= m_total_reqs ? m_total_reqs - reqs_to_decay : 0L;
                #ifdef RATELIMITER_DEBUG
                RL_DEBUG(__func__ << " m_total_reqs changed from " << m_total_reqs_prev << " to " << m_total_reqs);
                #endif

                m_t0 = t_win_left;
            }

            m_tlast = t1;

            // Calculate burst with one millisec resolution:
            long t1_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1.time_since_epoch()).count();
            if (t1_ms == m_tlast_ms) {
                m_burst_reqs += n_reqs;
            } else {
                m_burst_reqs = n_reqs;
                m_tlast_ms = t1_ms;
            }
                    
            if (m_burst_reqs > m_burst_cap) {
                return false;
            }
            m_total_reqs += n_reqs;
            cv.notify_all(); // ask others to recacl t deadline and wait

            bool ret = true;
            auto now = t1;
            if (calc_deadline(now, t1)) {
                ret = false;
                while(!init_shutdown) {
                    if (cv.wait_until(lck, t1) == std::cv_status::timeout) {

                        #ifdef RATELIMITER_DEBUG
                        auto now = std::chrono::steady_clock::now();
                        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()
                        ).count();
                        RL_DEBUG(__func__ << " deadline reached at=" << now_ms);
                        #endif

                        m_tlast = t1;
                        ret = true;
                        break;
                    } else if (!init_shutdown) {
                        auto now = std::chrono::steady_clock::now();

                        #ifdef RATELIMITER_DEBUG
                        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()
                        ).count();
                        RL_DEBUG(__func__ << " wait interrupted at=" << now_ms);
                        #endif

                        calc_deadline(now, t1);
                    }
                }
            }
            return ret;
        }


        // Return true if we need to continue waiting for allowed rps 
        // NOTE: we must pass 'now' as wait_until may be interrupted by another thread (before deadline)
        // so we need to recalculate deadline for the updated total_reqs and t=now 
        bool calc_deadline(std::chrono::steady_clock::time_point now, std::chrono::steady_clock::time_point &deadline) {
            double cur_rps = 0.0;
            auto cur_span = std::chrono::duration_cast<std::chrono::milliseconds> (now - m_t0).count();
            if (cur_span < m_win_size) {
                cur_span = m_win_size;
            }
            if (cur_span > 0)
                cur_rps = (double)m_total_reqs / cur_span * 1000.0;

            // Check current rps:
            if ((double)m_total_reqs / cur_span * 1000.0 <= m_rps) 
                return false;
            // 'ceil' prevents infinite loop due truncations
            long span_ms = (long)ceil((double)m_total_reqs / m_rps * 1000.0);
            if (span_ms <= 0)
                return false;
            std::chrono::duration<int, std::milli> span(span_ms);
            deadline = m_t0 + span;

            #ifdef RATELIMITER_DEBUG
            auto ms0 = std::chrono::duration_cast<std::chrono::milliseconds>(
                m_t0.time_since_epoch()
            ).count();
            auto ms1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline.time_since_epoch()
            ).count();
            RL_DEBUG(__func__ << " cur_rps=" << cur_rps << " m_rps=" << m_rps << " total_reqs=" 
                << m_total_reqs << " cur_span=" << cur_span << " span_ms=" << span_ms << " m_t0=" << ms0 << " deadline=" << ms1);
            #endif // RATELIMITER_DEBUG
            return deadline > now;
        }

        // caller must hold m_mtx 
        bool all_finished() {
            return m_enters == 0;
        }

        double m_rps;
        int m_burst_cap;
        long m_win_size;
        long m_total_reqs;
        long m_burst_reqs;
        std::chrono::steady_clock::time_point m_t0;
        std::chrono::steady_clock::time_point m_tlast;
        long m_tlast_ms;
        int m_enters;
        std::condition_variable cv;
        std::condition_variable cv_shutdown;
        std::mutex m_mtx;
        bool init_shutdown;
    };
}
