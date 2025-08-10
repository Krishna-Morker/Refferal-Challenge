Referral Graph System
Overview
This C++ program models a referral system where users can refer others to join a network. It implements a directed acyclic graph (DAG) to represent referral relationships and provides various analytical tools to measure and optimize referral program performance. Key features include user management, referral relationship tracking, shortest path analysis, top referrer identification, growth simulation, and bonus optimization.

Features
Core Functionality
User Management: Add users with deterministic token generation

Referral Tracking: Create referral relationships with cycle prevention

Referral Analytics:

Get direct referrals for any user

Calculate total referrals (direct + indirect)

Identify top referrers

Path Analysis: Determine if a user lies on the shortest referral path between two others

Advanced Capabilities
Growth Simulation: Model expected referral growth over time

Target Projection: Calculate days needed to reach hiring targets

Bonus Optimization: Determine minimum bonus required to meet hiring goals within time constraints

Time Complexity Analysis
Operation	Time Complexity	Description
addUser()	O(L)	L = email length (token generation)
addReferral()	O(h + log n)	h = tree height, n = users
getDirectReferrals()	O(1) + O(d)	d = direct referrals
topKReferrers()	O(n)	n = total users
isOnShortestPath()	O(V + E)	V = vertices, E = edges
simulate()	O(days²)	days = simulation period
min_bonus_for_target()	O(log(maxBonus) * days²)	days = target period
AI/ML Techniques
Probabilistic Modeling
math
E[New Referrals] = Σ (cohortₛ × p × P(Binomial(t, p) < capacity)
Models user adoption probability (p) and referral limits

Uses binomial distribution with capacity constraints

Precomputes CDFs for efficient calculation

Optimization Algorithms
Exponential + binary search for bonus optimization

Finds minimum bonus where:

math
days\_to\_target(p(bonus), target) ≤ deadline
Uses adoption probability function: p = f(bonus)
