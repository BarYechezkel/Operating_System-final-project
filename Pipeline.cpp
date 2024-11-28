#include <sys/socket.h>  // Include this header for the send function
#include <sys/types.h>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <vector>
#include "Graph.cpp"
#include "MST_graph.hpp"
#include "MST_stats.hpp"
#include "MST_strategy.hpp"
using namespace std;

// ActiveObject class that processes a task
struct ActiveObject {
    std::thread* workerThread;
    std::queue<void*> tasks;
    std::mutex acMutex;
    std::condition_variable cv;
    bool stopFlag;
    std::function<void(void*, mutex&)> function;  // The function that the worker will execute
    queue<void*>* nextQueueActiveObject;          // Queue for the next ActiveObject
    int id;

    ActiveObject(std::function<void(void*, mutex&)> func, int id) : stopFlag(false), function(func), workerThread(nullptr), nextQueueActiveObject(nullptr), id(id) {}

    // Copy constructor
    ActiveObject(const ActiveObject& other)
        : acMutex(), cv(), stopFlag(other.stopFlag), function(other.function), nextQueueActiveObject(other.nextQueueActiveObject), id(other.id) {}

    void addTask(void* task) {
        //  std::lock_guard<std::mutex> lock(acMutex);
        tasks.push(task);
        cv.notify_one();
        // cout << "Task added to ActiveObject. Queue size: " << tasks.size() << endl;
    }

    void runTasks(vector<ActiveObject>& activeObjects, mutex& send_mutex) {
        while (true) {
            void* task;
            {
                std::unique_lock<std::mutex> lock(acMutex);
                cv.wait(lock, [this] { return !tasks.empty() || stopFlag; });

                if (stopFlag && tasks.empty()) {
                    return;
                }

                if (!tasks.empty()) {
                    task = tasks.front();
                    tasks.pop();
                }

                // Release the current lock before processing
                lock.unlock();

                if (task) {
                    function(task, send_mutex);
                    // cout << "Task executed. Thread ID: " << std::this_thread::get_id() << endl;

                    if (nextQueueActiveObject) {
                        std::lock_guard<std::mutex> nextLock(activeObjects[id + 1].acMutex);  // Lock before modifying next queue
                        nextQueueActiveObject->push(task);
                        activeObjects[id + 1].cv.notify_one();
                        // cout << "Task passed to next ActiveObject. Thread ID: " << std::this_thread::get_id() << endl;
                    }
                }
            }
        }
    }
};

// Pipeline class that manages multiple ActiveObjects (workers)
class Pipeline {
    // std::mutex mtx_;
    // std::mutex& ansMutex;
    std::condition_variable cv_;
    std::vector<ActiveObject> activeObjects;
    //     // Task 1: Longest Distance
    static void taskLongestDistance(void* task, mutex& send_mutex) {
        
        auto* taskTuple = static_cast<std::tuple<MST_graph*, string*, int>*>(task);
        if (taskTuple != nullptr) {
            MST_stats stats;
            MST_graph* mst_graph = std::get<0>(*taskTuple);
            string* clientAns = std::get<1>(*taskTuple);
            if (mst_graph != nullptr && clientAns != nullptr) {
                {
                    lock_guard<mutex> lock(send_mutex);
                    *clientAns += "Longest Distance: " + std::to_string(stats.getLongestDistance(*mst_graph)) + "\n";
                }
            }
        } else {
            cout << "Error: Invalid task tuple" << endl;
        }
    }

    // Task 2: Shortest Distance
    static void taskShortestDistance(void* task, mutex& send_mutex) {
        // std::cout << "Thread ID: " << std::this_thread::get_id() << " Calculated shortest distance" << std::endl;

        auto* taskTuple = static_cast<tuple<MST_graph*, string*, int>*>(task);
        MST_graph* mst_graph = std::get<0>(*taskTuple);
        string* clientAns = std::get<1>(*taskTuple);

        MST_stats stats;
        {
            std::lock_guard<std::mutex> lock(send_mutex);
            *clientAns += "Shortest Distance: " + std::to_string(stats.getShortestDistance(*mst_graph)) + "\n";
        }
    }

    // Task 3: Average Distance
    static void taskAverageDistance(void* task, mutex& send_mutex) {

        // std::cout << "Thread ID: " << std::this_thread::get_id() << " Calculated average distance" << std::endl;

        auto* taskTuple = static_cast<tuple<MST_graph*, string*, int>*>(task);
        MST_graph* mst_graph = std::get<0>(*taskTuple);
        string* clientAns = std::get<1>(*taskTuple);

        MST_stats stats;
        {
            std::lock_guard<std::mutex> lock(send_mutex);
            *clientAns += "Average Distance: " + std::to_string(stats.getAverageDistance(*mst_graph)) + "\n";
        }
    }

    // Task 4: Total Weight
    static void taskTotalWeight(void* task, mutex& send_mutex) {

        // std::cout << "Thread ID: " << std::this_thread::get_id() << " Calculated total weight" << std::endl;

        auto* taskTuple = static_cast<tuple<MST_graph*, string*, int>*>(task);
        MST_graph* mst_graph = std::get<0>(*taskTuple);
        string* clientAns = std::get<1>(*taskTuple);
        int fdclient = std::get<2>(*taskTuple);

        MST_stats stats;
        {
            std::lock_guard<std::mutex> lock(send_mutex);
            *clientAns += "Total Weight: " + std::to_string(stats.getTotalWeight(*mst_graph)) + "\n";

            delete taskTuple;

            // send the response back to the client
            if (send(fdclient, clientAns->c_str(), clientAns->size(), 0) == -1) {
                perror("send");
            }
        }
    }

   public:
    Pipeline() {
        // Create ActiveObjects (workers) for each task
        activeObjects.push_back(ActiveObject(Pipeline::taskLongestDistance, 0));   // Task 1
        activeObjects.push_back(ActiveObject(Pipeline::taskShortestDistance, 1));  // Task 2
        activeObjects.push_back(ActiveObject(Pipeline::taskAverageDistance, 2));   // Task 3
        activeObjects.push_back(ActiveObject(Pipeline::taskTotalWeight, 3));       // Task 4

        for (int i = 0; i < activeObjects.size() - 1; ++i) {
            activeObjects[i].nextQueueActiveObject = &activeObjects[i + 1].tasks;
        }
    }

    ~Pipeline() {
        stop();  // Ensure all threads are stopped and joined

        for (auto& activeObject : activeObjects) {
            delete activeObject.workerThread;
        }
        // stop();
    }

    void addRequest(void* task) {
        std::lock_guard<std::mutex> lock(activeObjects[0].acMutex);
        activeObjects[0].addTask(task);
        activeObjects[0].cv.notify_one();
        // cout << "Task added to Pipeline (first worker). Initial ActiveObject notified." << endl;
    }

    void start(mutex& send_mutex) {
        // cout << "[DEBUG] Pipeline starting all worker threads." << endl;
        for (auto& activeObject : activeObjects) {
            activeObject.workerThread = new std::thread(&ActiveObject::runTasks, &activeObject, std::ref(activeObjects), std::ref(send_mutex));
            // cout << "[DEBUG] Worker thread started for ActiveObject ID: " << activeObject.id << endl;
            // cout << "[DEBUG] Worker thread started for ActiveObject." << endl;
        }
    }
    void stop() {
        for (auto& activeObject : activeObjects) {
            std::lock_guard<std::mutex> lock(activeObject.acMutex);
            activeObject.stopFlag = true;
            activeObject.cv.notify_one();
        }

        for (auto& activeObject : activeObjects) {
            if (activeObject.workerThread->joinable()) {
                activeObject.workerThread->join();
            }
        }
    }
};