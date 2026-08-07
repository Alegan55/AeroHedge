#pragma once

#include <pthread.h>
#include <thread>
#include <iostream>
#include <cstring>

namespace aerohedge {

inline void pin_thread_to_core(std::thread& th, int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    int rc = pthread_setaffinity_np(th.native_handle(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cerr << "Warning: Failed to pin thread to core " << core_id 
                  << " (" << std::strerror(rc) << ")\n";
    } else {
        std::cout << "Successfully pinned thread to CPU Core " << core_id << "\n";
    }
}

} // namespace aerohedge
