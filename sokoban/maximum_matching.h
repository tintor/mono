// C++ implementation of Hopcroft Karp algorithm for maximum matching
#pragma once
#include "sokoban/common.h"

// A class to represent Bipartite graph for Hopcroft
// Karp implementation
class BipartiteGraph {
    // m and n are number of vertices on left
    // and right sides of Bipartite Graph
    int m, n;

    // adj[u] stores adjacents of left side
    // vertex 'u'. The value of u ranges from 1 to m.
    // 0 is used for dummy vertex
    vector<vector<int>> adj;

    mutable vector<int> pairU, pairV, dist;

    mutable array_deque<int> Q; // used by bfs()

    // Returns true if there is an augmenting path
    bool bfs() const;

    // Adds augmenting path if there is one beginning
    // with u
    bool dfs(int u) const;

public:
    void reset(int m, int n);

    // Add edge from u to v and v to u
    void add_edge(int u, int v) { adj[u].push_back(v); }

    // Returns size of maximum matcing
    int maximum_matching() const;
};
