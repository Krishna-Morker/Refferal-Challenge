Key operations and their time complexities:

User Management

addUser(): O(1) average case (hash operations)

Token generation: O(L) where L = email length

Referral Operations

addReferral(): O(h + log n)

h = height of referral tree

n = number of users

Involves DSU union (O(α(n))) and referral count updates

Graph Queries

getDirectReferrals(): O(1) + O(d) where d = direct referrals

topKReferrers(): O(n) worst-case (n = total users)

Path Analysis

isOnShortestPath(): O(V + E)

V = number of vertices

E = number of edges

Uses BFS for shortest path calculations

Simulations

simulate(): O(days²)

days_to_target(): O(max_days_limit²)

min_bonus_for_target(): O(log(maxBonus) * days²)

AI/ML Techniques Used
Probabilistic Modeling

Binomial distribution with capacity constraints

Models user adoption probability (p) and referral limits

Optimization Algorithms

Exponential + binary search for bonus optimization

Finds minimum bonus to hit hiring targets

Statistical Computations

Cumulative distribution functions (CDF)

Expected value calculations for growth projections

Key Components
Graph Representation

Adjacency lists for referral relationships

DSU (Disjoint Set Union) for cycle detection

Bidirectional email-token mapping

Referral Analytics

Shortest path analysis with BFS

Centrality metrics (path fraction calculation)

Referral count propagation through chains

Growth Simulation

math
E[New Referrals] = Σ (cohortₛ × p × P(Binomial(t, p) < capacity)
Models viral growth with capacity constraints

Precomputes CDFs for efficient calculation

Bonus Optimization

Finds minimum bonus where:

math
days\_to\_target(p(bonus), target) ≤ deadline
Uses adoption probability function: p = f(bonus)
