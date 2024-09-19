#include <arpa/inet.h>   // Include for inet_ntop
#include <netdb.h>       // Include for addrinfo
#include <netinet/in.h>  // Include for sockaddr_in and sockaddr_in6
#include <sys/socket.h>  // Include for sockaddr
#include <unistd.h>      // Include for close

#include <algorithm>
#include <cassert>
#include <cstring>  // Include for memset
#include <ctime>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <set>
#include <sstream>  // Include the necessary header for istringstream
#include <unordered_map>
#include <vector>

#include "Graph.cpp"
#include "GraphGUI.cpp"  // Include the GraphGUI header
#include "MST.hpp"
#include "union_find.hpp"

#define PORT "9034"  // port we're listening on
using namespace std;

Graph sharedGraph;  // Shared graph for all clients
mutex mtx;
MST mst;
// assume the input is in the form of "Newgraph n m u v w"
// and the input of edge u v with weight w but dont add the reverse edge with weight w'
vector<tuple<int, int, int, int>> Newgraph(istringstream &iss, int n, int num_of_Edge) {
    vector<tuple<int, int, int, int>> graph;
    cout << "Newgraph" << endl;
    int u, v, w;  // vertex and weight
    for (int i = 0; i < num_of_Edge; ++i) {
        iss >> u >> v >> w;
        graph.emplace_back(u, v, w, i);
        graph.emplace_back(v, u, w, i);  // Add the reverse edge for undirected graph
    }
    return graph;
}

string graph_user_commands(string input_user) {
    string ans;
    string command_of_user;
    istringstream iss(input_user);
    iss >> command_of_user;

    int n, m;

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
            sharedGraph.setEdges(newEdges);       // Pass the local variable to setEdges
            sharedGraph.setnumVertices(n);
            ans += "Graph created:\n";
            for (int i = 0; i < m; i++) {
                int u, v, w, id;
                tie(u, v, w, id) = sharedGraph.getEdge(i);
                ans += "Edge " + to_string(i) + ": " + to_string(u) + " " + to_string(v) + " " + to_string(w) + "\n";
            }
        }
    } else if (command_of_user == "MST-P") {
        if (sharedGraph.getSize() != 0) {
            cout << "MST-P n: " << n << endl;
            cout << "Shared graph size: " << sharedGraph.getSize() << endl;

            // Debug print edges
            for (int i = 0; i < sharedGraph.getSize(); i++) {
                int u, v, w, id;
                tie(u, v, w, id) = sharedGraph.getEdge(i);
                ans += "Edge " + to_string(i) + ": " + to_string(u) + " " + to_string(v) + " " + to_string(w) + "\n";
            }
            cout << "In MST-P, sharedGraph.getnumVertices(): " << sharedGraph.getnumVertices() << endl;
            ans += mst.prim(sharedGraph.getEdges(), sharedGraph.getnumVertices());
            cout << "MST-P completed" << endl;
        }
    } else if (command_of_user == "MST-K") {
        cout << "MST-K n: " << n << endl;
        if (sharedGraph.getSize() != 0) {
            ans += mst.kruskal(sharedGraph.getEdges(), sharedGraph.getnumVertices());
        }
    } else if (command_of_user == "Newedge") {
        int from, to, weight;
        iss >> from >> to >> weight;
        cout << "Newedge n: " << n << endl;
        if (sharedGraph.getSize() != 0) {
            sharedGraph.addEdge(from, to, weight, sharedGraph.getSize());
            ans += "Edge added from " + to_string(from) + " to " + to_string(to) + "\n";
        } else {
            ans += "No graph found for adding edge.\n";
        }
    } else if (command_of_user == "Removeedge") {
        int from, to;
        iss >> from >> to;
        cout << "Removeedge n: " << n << endl;
        if (sharedGraph.getSize() != 0) {
            sharedGraph.removeEdge(from, to);
            ans += "Edge removed from " + to_string(from) + " to " + to_string(to) + "\n";
        } else {
            ans += "No graph found for removing edge.\n";
        }
    } else if (command_of_user == "Reduceedge") {
        int id, newWeight;
        iss >> id >> newWeight;
        cout << "Reduceedge n: " << n << endl;
        if (sharedGraph.getSize() != 0) {
            sharedGraph.reduceEdges(id, newWeight);
            ans += "Edge reduced with id " + to_string(id) + " to weight " + to_string(newWeight) + "\n";
        } else {
            ans += "No graph found for reducing edge.\n";
        }
    } else {
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

vector<tuple<int, int, int, int>>
build_random_connected_graph(int n, int m, unsigned int seed) {
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

// int main(int argc, char *argv[]) {
//     if (argc != 4) {
//         std::cerr << "Usage: " << argv[0] << " <n> <m> <seed>" << std::endl;
//         return 1;
//     }

//     int n = std::stoi(argv[1]);
//     int m = std::stoi(argv[2]);
//     unsigned int seed = std::stoul(argv[3]);

//     MST mst;
//     Graph graph(n);
//     vector<tuple<int, int, int, int>> mst_result = mst.prim(graph);
//     mst.printMST(mst_result);

//     // Define vertices (for example, positions of points)
//     std::vector<sf::Vector2f> vertices;
//     for (int i = 0; i < n; ++i) {
//         vertices.emplace_back(100.f + (i * 200.f), 100.f + ((i % 2) * 200.f));
//     }

//     // Create the Graph GUI and run it
//     GraphGUI graphGUI(vertices, graph, mst_result);
//     graphGUI.run();

//     return 0;
// }

int longestdistance(Graph graph, int u, int v) {
    int n = graph.getnumVertices();  // Get the number of vertices in the graph
    vector<int> dist(n, INT_MIN);    // Initialize distance vector with maximum integer values
    dist[u] = 0;                     // Set the distance from the source vertex to itself as 0
    queue<int> q;                    // Create a queue for BFS
    q.push(u);                       // Push the source vertex into the queue

    while (!q.empty()) {             // Continue until the queue is empty
        int x = q.front();           // Get the front element of the queue
        q.pop();                     // Remove the front element from the queue

        for (int i = 0; i < graph.getSize(); i++) {  // Iterate over all edges in the graph
            int u, v, w, id;                         // Variables to store edge details
            tie(u, v, w, id) = graph.getEdge(i);     // Get the edge details

            // Check if the current vertex is one of the endpoints of the edge
            if ((u == x || v == x)) {
                int neighbor = (u == x) ? v : u;     // Determine the neighbor vertex
                // If the new distance to the neighbor is shorter, update it
                if (dist[neighbor] < dist[x] + w) {
                    dist[neighbor] = dist[x] + w;    // Update the distance to the neighbor
                    q.push(neighbor);                // Push the neighbor into the queue
                }
            }
        }
    }
    return dist[v];  // Return the distance to the destination vertex
}
int main() {
    int listener = -1;                      // listening socket descriptor
    struct sockaddr_storage clientAddress;  // client address
    socklen_t addrlen;
    char remoteIP[INET6_ADDRSTRLEN];
    int yes = 1;  // for setsockopt() SO_REUSEADDR, below
    int rv;
    struct addrinfo hints, *ai, *p;

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
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        cerr << "selectserver: failed to bind" << endl;
        exit(2);
    }

    freeaddrinfo(ai);  // all done with this

    // listen
    if (listen(listener, 10) == -1) {
        perror("listen");
        close(listener);
        exit(3);
    }

    cout << "Server is listening on port " << PORT << endl;

    while (true) {
        addrlen = sizeof clientAddress;
        int newfd = accept(listener, (struct sockaddr *)&clientAddress, &addrlen);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(clientAddress.ss_family, get_in_addr((struct sockaddr *)&clientAddress), remoteIP, INET6_ADDRSTRLEN);
        cout << "selectserver: new connection from " << remoteIP << endl;

        char buf[2048];
        int nbytes;
        while ((nbytes = recv(newfd, buf, sizeof(buf), 0)) > 0) {
            buf[nbytes] = '\0';
            string client_input = string(buf);
            string ans = graph_user_commands(client_input);

            cout << "Response to client: " << ans << endl;
            if (send(newfd, ans.c_str(), ans.size(), 0) == -1) {
                perror("send");
            }
        }

        if (nbytes == 0) {
            cout << "selectserver: socket " << newfd << " hung up" << endl;
        } else if (nbytes < 0) {
            perror("recv");
        }

        close(newfd);
    }

    close(listener);
    return 0;
}