#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>  // Include this header for the send function


#include "MST_graph.hpp"
#include "MST_stats.hpp"

class LeaderFollowerPool {
   public:
    LeaderFollowerPool(int numThreads, std::mutex& ansMutex);

    ~LeaderFollowerPool();

    // Add an event to the queue
    void addEventHandler(void* task);

    // Stop the thread pool
    void stop();
    void start();

   private:
    void leaderRole();    // Function for the leader thread
    void followerRole();  // Function for the followers
    void mainFunction(void* task);

    int numThreads_;                    // Total number of threads in the pool
    std::queue<void*> eventQueue_;      // Queue of events to process
    std::vector<std::thread> workers_;  // List of thread objects
    std::mutex mutexstop;               // Mutex for synchronization
    std::mutex mutexqueue;              // Mutex for synchronization

    std::mutex &ansMutex;                // Mutex for synchronization
    std::condition_variable cv_;        // Condition variable for waiting threads
    bool stopFlag_;                     // Flag to stop the thread pool
    bool leader_exists;;
};

#endif  // THREAD_POOL_HPP