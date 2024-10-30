#include <arpa/inet.h>   // Include for inet_ntop
#include <netdb.h>       // Include for addrinfo
#include <netinet/in.h>  // Include for sockaddr_in and sockaddr_in6
#include <sys/socket.h>  // Include for sockaddr
#include <unistd.h>      // Include for close

#include <algorithm>
#include <cassert>
#include <csignal>  // For signal handling (e.g., SIGINT)
#include <cstring>  // Include for memset
#include <ctime>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <set>
#include <sstream>  // Include the necessary header for istringstream
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <future>
#include <memory>


#include "Graph.cpp"
#include "GraphGUI.cpp"  // Include the GraphGUI header
#include "MST_graph.hpp"
#include "MST_stats.hpp"
#include "MST_strategy.hpp"
#include "Pipeline.cpp"
#include "ThreadPool.hpp"
#include "union_find.hpp"
#define PORT "9034"    // port we're listening on
#define NUM_THREADS 4  // Number of threads in the thread pool
using namespace std;
unordered_map<int, tuple<Graph, MST_graph, string>> map_clients;  // Each client gets its own graph
    ThreadPool threadPool(NUM_THREADS);  // Create a thread pool object

mutex mtx;
MST_strategy mst;
MST_stats stats;
bool isMST = false;
// std::ostringstream sharedAns;

// Declare the ThreadPool instance here
int listener;                        // Global listener for shutdown handling

// Signal handler for graceful shutdown
void shutdown_handler(int signum) {
    cout << "\nShutting down the server..." << endl;
    close(listener);  // Close the listener socket
    exit(0);          // Exit the program
}

// assume the input is in the form of "Newgraph n m u v w"
// and the input of edge u v with weight w but dont add the reverse edge with weight w'
vector<tuple<int, int, int, int>> Newgraph(istringstream &iss, int n, int num_of_Edge) {
    vector<tuple<int, int, int, int>> graph;
    cout << "Newgraph" << endl;
    int u, v, w;  // vertex and weight
    for (int i = 0; i < num_of_Edge; ++i) {
        iss >> u >> v >> w;
        // dibug print
        cout << "Newgraph u: " << u << endl;
        cout << "Newgraph v: " << v << endl;
        cout << "Newgraph w: " << w << endl;
        graph.emplace_back(u, v, w, i);
        graph.emplace_back(v, u, w, i);  // Add the reverse edge for undirected graph
    }
    return graph;
}
string MST_to_string(const MST_graph &mst) {
    string ans;
    vector<tuple<int, int, int, int>> edges = mst.getEdges();

    ans += "MST created:\n";
    for (const auto &e : edges) {
        int u, v, w, id;
        tie(u, v, w, id) = e;
        ans += "Edge " + to_string(id) + ": " + to_string(u) + " " + to_string(v) + " " + to_string(w) + "\n";
    }

    return ans;
}
string graph_user_commands(string input_user, Graph &clientGraph, MST_graph &clientMST, string &clientAns) {
    // threadPool.start();  // Start the thread pool
    // Shared string to accumulate results
    std::ostringstream sharedAns_;
    //  Pipeline Pipeline(sharedAns, mtxAns);  // Create a pipeline object
    Pipeline pipeline(sharedAns_);
    string ans;
    string command_of_user;
    istringstream iss(input_user);
    iss >> command_of_user;

    int n, m;
    string strategy;

    if (command_of_user.empty()) {
        ans += "No command received.\n";
    } else if (command_of_user == "Newgraph") {
        // Get the number of vertices and edges
        iss >> n >> m;
        cout << "Newgraph n: " << n << endl;
        cout << "Newgraph m: " << m << endl;

        if (n <= 0 || m < 0) {
            ans += "Invalid graph parameters. \n";
        } else {
            auto newEdges = Newgraph(iss, n, m);  // Store the result in a local variable
            clientGraph.setEdges(newEdges);       // Pass the local variable to setEdges
            clientGraph.setnumVertices(n);
            ans += "Graph created:\n";
            for (int i = 0; i < 2 * m; i = i + 2) {
                int u, v, w, id;
                tie(u, v, w, id) = clientGraph.getEdge(i);
                ans += "Edge " + to_string(i) + ": " + to_string(u) + " " + to_string(v) + " " + to_string(w) + "\n";
            }
            iss >> strategy;
            if (strategy == "prim") {
                clientMST = mst.prim(clientGraph.getEdges(), clientGraph.getnumVertices());
                isMST = true;
                ans += MST_to_string(clientMST);
            } else if (strategy == "kruskal") {
                clientMST = mst.kruskal(clientGraph.getEdges(), clientGraph.getnumVertices());
                isMST = true;
                ans += MST_to_string(clientMST);
            } else {
                ans += "Invalid strategy.\n";
            }
        }
    } else if (command_of_user == "Newedge") {
        int from, to, weight;
        iss >> from >> to >> weight;
        cout << "Newedge n: " << n << endl;
        if (clientGraph.getSize() != 0) {
            clientGraph.addEdge(from, to, weight, clientGraph.getSize());
            ans += "Edge added from " + to_string(from) + " to " + to_string(to) + "\n";
            // update the mst graph
            if (clientMST.getStrategy() == "prim") {
                clientMST = mst.prim(clientGraph.getEdges(), clientGraph.getnumVertices());
            } else if (clientMST.getStrategy() == "kruskal") {
                clientMST = mst.kruskal(clientGraph.getEdges(), clientGraph.getnumVertices());
            }

        }

        else {
            ans += "No graph found for adding edge.\n";
        }
    } else if (command_of_user == "Removeedge") {
        int from, to;
        iss >> from >> to;
        cout << "Removeedge n: " << n << endl;
        if (clientGraph.getSize() != 0) {
            clientGraph.removeEdge(from, to);
            ans += "Edge removed from " + to_string(from) + " to " + to_string(to) + "\n";

            // update the mst graph
            if (clientMST.getStrategy() == "prim") {
                clientMST = mst.prim(clientGraph.getEdges(), clientGraph.getnumVertices());
            } else if (clientMST.getStrategy() == "kruskal") {
                clientMST = mst.kruskal(clientGraph.getEdges(), clientGraph.getnumVertices());
            }

        } else {
            ans += "No graph found for removing edge.\n";
        }
    } else if (command_of_user == "Reduceedge") {
        int id, newWeight;
        iss >> id >> newWeight;
        cout << "Reduceedge n: " << n << endl;
        if (clientGraph.getSize() != 0) {
            clientGraph.reduceEdges(id, newWeight);
            ans += "Edge reduced with id " + to_string(id) + " to weight " + to_string(newWeight) + "\n";

            // update the mst graph
            if (clientMST.getStrategy() == "prim") {
                clientMST = mst.prim(clientGraph.getEdges(), clientGraph.getnumVertices());
            } else if (clientMST.getStrategy() == "kruskal") {
                clientMST = mst.kruskal(clientGraph.getEdges(), clientGraph.getnumVertices());
            }

        } else {
            ans += "No graph found for reducing edge.\n";
        }
    }  // Leader-Follower with shared answer and mutex
    else if (command_of_user == "Leader_Follower") {
        if (!isMST) {
            ans += "No MST found.\n";
        } else {
            // Use a promise to get the result
        auto promise = std::make_shared<std::promise<std::string>>();
            auto future = promise->get_future();

            threadPool.addEventHandler([&clientMST, promise]() {
                MST_stats mst_stats;
                std::ostringstream localAns;
                localAns << "Thread " << std::this_thread::get_id() << "\n";
                localAns << " Longest path: " << mst_stats.getLongestDistance(clientMST) << "\n";
                localAns << " Shortest path: " << mst_stats.getShortestDistance(clientMST) << "\n";
                localAns << " Average path: " << mst_stats.getAverageDistance(clientMST) << "\n";
                localAns << " Total weight: " << mst_stats.getTotalWeight(clientMST) << "\n";
                promise->set_value(localAns.str());
            });

            // Wait for result
            try {
                ans += future.get();  // Get the result when ready
            } catch (const exception &e) {
                ans += "Error processing request: " + string(e.what()) + "\n";
            }
        }
    }
else if (command_of_user == "Pipeline") {
    if (!isMST) {
        ans += "No MST found.\n";
    } else {
        // Simulate clients sending graphs
        pipeline.processGraph(clientGraph);
        // Wait for the pipeline to finish processing
        string ans_pipeline = pipeline.waitForCompletion();

        // Append the processed result to ans
        ans += ans_pipeline;
    }
}
else {
    ans += "Unknown command.\n";
}

return ans;
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

vector<tuple<int, int, int, int>> build_random_connected_graph(int n, int m, unsigned int seed) {
    srand(seed);
    assert(m >= (n - 1));
    assert(1ll * m <= (1ll * n * (n - 1)) / 2);

    vector<int> W(m, 0);
    iota(W.begin(), W.end(), 0);

    random_device rd;
    mt19937 g(seed);
    shuffle(W.begin(), W.end(), g);

    vector<tuple<int, int, int, int>> random_graph;
    int nxt = 0;
    set<pair<int, int>> edges;

    for (int i = 0; i < n - 1; ++i) {
        random_graph.emplace_back(i, i + 1, W[nxt], nxt);
        ++nxt;
        edges.emplace(i, i + 1);
    }

    int remaining_edges = m - (n - 1);
    for (int e = 0; e < remaining_edges; ++e) {
        int a, b;
        a = (rand() % n);
        b = (rand() % n);
        if (a > b) swap(a, b);
        while (b == a || edges.find(make_pair(a, b)) != edges.end()) {
            a = (rand() % n);
            b = (rand() % n);
            if (a > b) swap(a, b);
        }
        random_graph.emplace_back(a, b, W[nxt], nxt);
        ++nxt;
        edges.emplace(a, b);
    }
    assert(static_cast<int>(random_graph.size()) == m);
    return random_graph;
}
int main() {


    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    // Initialize the ThreadPool once
    threadPool.start();                  // Start the ThreadPool once during server initialization

    int newfd;                              // newly accept()ed socket descriptor
    struct sockaddr_storage clientAddress;  // client address
    socklen_t addrlen;

    char buf[4096];  // buffer for client data (increased size)
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];
    int yes = 1;  // for setsockopt() SO_REUSEADDR, below
    int i, rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);  // clear the master and temp sets
    FD_ZERO(&read_fds);

    // Set up shutdown signal handler
    signal(SIGINT, shutdown_handler);

    // get us a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        cerr << "selectserver: " << gai_strerror(rv) << endl;
        exit(1);
    }

    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }

        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }

    if (p == NULL) {
        cerr << "selectserver: failed to bind" << endl;
        exit(2);
    }

    freeaddrinfo(ai);  // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        exit(3);
    }

    // add the listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest file descriptor
    fdmax = listener;  // so far, it's this one

    cout << "Server is listening on port " << PORT << endl;

    // main loop
    for (;;) {
        read_fds = master;  // copy it
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {  // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof clientAddress;
                    newfd = accept(listener, (struct sockaddr *)&clientAddress, &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master);  // add to master set
                        if (newfd > fdmax) {     // keep track of the max
                            fdmax = newfd;
                        }
                        cout << "selectserver: new connection from "
                             << inet_ntop(clientAddress.ss_family,
                                          get_in_addr((struct sockaddr *)&clientAddress),
                                          remoteIP, INET6_ADDRSTRLEN)
                             << " on socket " << newfd << endl;

                        // Create a new graph for this client
                        lock_guard<mutex> lock(mtx);
                        std::get<0>(map_clients[newfd]) = Graph();  // Create an empty graph for the new client

                        // map_clients[newfd].first = Graph();  // Create an empty graph for the new client
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            cout << "selectserver: socket " << i << " hung up" << endl;
                        } else {
                            perror("recv");
                        }
                        close(i);            // bye!
                        FD_CLR(i, &master);  // remove from master set
                        lock_guard<mutex> lock(mtx);
                        map_clients.erase(i);  // Remove the client's graph
                    } else {
                        // we got some data from a client
                        buf[nbytes] = '\0';  // null-terminate the buffer

                        // Process the command for this client
                        lock_guard<mutex> lock(mtx);
                        string response = graph_user_commands(string(buf), get<0>(map_clients[i]), get<1>(map_clients[i]), get<2>(map_clients[i]));

                        // string response = graph_user_commands(string(buf), map_clients[i].first, map_clients[i].second,);

                        // Send the response back to the client
                        if (send(i, response.c_str(), response.length(), 0) == -1) {
                            perror("send");
                        }
                    }
                }
            }
        }
    }

    return 0;
}
