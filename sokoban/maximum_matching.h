#include "sokoban/common.h"
// C++ implementation of Hopcroft Karp algorithm for
// maximum matching
#define NIL 0
#define INF INT_MAX

// A class to represent Bipartite graph for Hopcroft
// Karp implementation
class BipGraph
{
    // m and n are number of vertices on left
    // and right sides of Bipartite Graph
    int m, n;

    // adj[u] stores adjacents of left side
    // vertex 'u'. The value of u ranges from 1 to m.
    // 0 is used for dummy vertex
    vector<list<int>> adj;

    vector<int> pairU, pairV, dist;

public:
    BipGraph(int m, int n) {
        this->m = m;
        this->n = n;
        adj.resize(m+1);

        // pairU[u] stores pair of u in matching where u
        // is a vertex on left side of Bipartite Graph.
        // If u doesn't have any pair, then pairU[u] is NIL
        pairU.resize(m+1);

        // pairV[v] stores pair of v in matching. If v
        // doesn't have any pair, then pairU[v] is NIL
        pairV.resize(n+1);

        // dist[u] stores distance of left side vertices
        // dist[u] is one more than dist[u'] if u is next
        // to u'in augmenting path
        dist.resize(m+1);
    }

    // Add edge from u to v and v to u
    void addEdge(int u, int v) { adj[u].push_back(v); }

    // Returns true if there is an augmenting path
    bool bfs();

    // Adds augmenting path if there is one beginning
    // with u
    bool dfs(int u);

    // Returns size of maximum matcing
    int hopcroftKarp();
};

// Returns size of maximum matching
int BipGraph::hopcroftKarp() {
    // Initialize NIL as pair of all vertices
    for (int u = 0; u <= m; u++) pairU[u] = NIL;
    for (int v = 0; v <= n; v++) pairV[v] = NIL;

    // Initialize result
    int result = 0;

    // Keep updating the result while there is an
    // augmenting path.
    while (bfs()) {
        // Find a free vertex
        for (int u = 1; u <= m; u++) {
            // If current vertex is free and there is
            // an augmenting path from current vertex
            if (pairU[u] == NIL && dfs(u)) result++;
        }
    }
    return result;
}

// Returns true if there is an augmenting path, else returns
// false
bool BipGraph::bfs() {
    array_deque<int> Q; //an integer queue

    // First layer of vertices (set distance as 0)
    for (int u = 1; u <= m; u++) {
        // If this is a free vertex, add it to queue
        if (pairU[u] == NIL) {
            // u is not matched
            dist[u] = 0;
            Q.push_back(u);
            continue;
        }
        // Else set distance as infinite so that this vertex
        // is considered next time
        dist[u] = INF;
    }

    // Initialize distance to NIL as infinite
    dist[NIL] = INF;

    // Q is going to contain vertices of left side only.
    while (!Q.empty()) {
        // Dequeue a vertex
        int u = Q.front();
        Q.pop_front();
        // If this node is not NIL and can provide a shorter path to NIL
        if (dist[u] < dist[NIL]) {
            // Get all adjacent vertices of the dequeued vertex u
            for (int v : adj[u]) {
                // If pair of v is not considered so far
                // (v, pairV[V]) is not yet explored edge.
                if (dist[pairV[v]] == INF) {
                    // Consider the pair and add it to queue
                    dist[pairV[v]] = dist[u] + 1;
                    Q.push_back(pairV[v]);
                }
            }
        }
    }

    // If we could come back to NIL using alternating path of distinct
    // vertices then there is an augmenting path
    return dist[NIL] != INF;
}

// Returns true if there is an augmenting path beginning with free vertex u
bool BipGraph::dfs(int u) {
    if (u != NIL) {
        for (int v : adj[u]) {
            // Follow the distances set by BFS
            if (dist[pairV[v]] == dist[u] + 1) {
                // If dfs for pair of v also returns
                // true
                if (dfs(pairV[v])) {
                    pairV[v] = u;
                    pairU[u] = v;
                    return true;
                }
            }
        }
        // If there is no augmenting path beginning with u.
        dist[u] = INF;
        return false;
    }
    return true;
}
