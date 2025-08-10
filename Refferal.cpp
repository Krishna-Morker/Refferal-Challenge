#include <iostream>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <stdexcept>
#include <queue>
#include <string>
#include <iomanip>
#include <cmath>       // powl, expl
#include <functional>  // std::function

using namespace std;

class ReferralGraph {
private:
    // --- core graph storage ---
    unordered_map<string, vector<string>> graph;      // token -> list of direct referrals (tokens)
    unordered_map<string, string> referredBy;         // candidate token -> referrer token

    // token <-> email mappings
    unordered_map<string, string> tokenToEmail;
    unordered_map<string, string> emailToToken;

    // DSU (used only to detect cycles robustly in unionSet)
    unordered_map<string, string> parent;
    unordered_map<string, int> compSize;

    // Referral counts (direct + indirect descendants) stored per token
    unordered_map<string, int> referralCount;
    // reverse index: referralCount -> set of tokens (keeps sorted order by count descending)
    map<int, unordered_set<string>, greater<int>> referaltotoken;

    // set of nodes currently with indegree 0 (useful for some algorithms)
    unordered_set<string> indegreeZero;


    // find with path compression
    string dsu_find(const string &token) {
        if (parent[token] != token) parent[token] = dsu_find(parent[token]);
        return parent[token];
    }

    // union two components; returns false if they're already in same component
    bool dsu_union(const string &a, const string &b) {
        string ra = dsu_find(a);
        string rb = dsu_find(b);
        if (ra == rb) return false;
        parent[rb] = ra;
        compSize[ra] += compSize[rb];
        return true;
    }

    // FNV-1a based deterministic token generation from an email string.
    unsigned long long fnv1aHash(const string &s) const {
        const unsigned long long FNV_offset_basis = 14695981039346656037ULL;
        const unsigned long long FNV_prime = 1099511628211ULL;
        unsigned long long h = FNV_offset_basis;
        for (char c : s) {
            h ^= static_cast<unsigned long long>(c);
            h *= FNV_prime;
        }
        return h;
    }

    string makeToken(const string &email) const {
        return string("token_") + to_string(fnv1aHash(email));
    }

    // small utility: remove token from referaltotoken bucket if present
    void removeFromReferalBucket(int oldCount, const string &token) {
        auto it = referaltotoken.find(oldCount);
        if (it == referaltotoken.end()) return;
        it->second.erase(token);
        if (it->second.empty()) referaltotoken.erase(it);
    }

public:
    ReferralGraph() = default;

    // Return list of all tokens
    vector<string> getAllTokens() const {
        vector<string> nodes;
        nodes.reserve(tokenToEmail.size());
        for (const auto &p : tokenToEmail) nodes.push_back(p.first);
        return nodes;
    }

    // Build reversed adjacency: for each node v, list of parents u where u->v
    unordered_map<string, vector<string>> buildReverseGraph() const {
        unordered_map<string, vector<string>> rev;
        for (const auto &p : tokenToEmail) rev[p.first] = {};
        for (const auto &kv : graph) {
            const string &u = kv.first;
            for (const string &v : kv.second) rev[v].push_back(u);
        }
        return rev;
    }

    // BFS that also counts number of shortest paths (sigma) from source.
    pair<unordered_map<string,int>, unordered_map<string,double>>
    bfs_count_paths(const string &source,
                    const unordered_map<string, vector<string>> &adj) {
        unordered_map<string,int> dist;
        unordered_map<string,double> sigma;

        // initialize for all nodes
        for (const auto &p : tokenToEmail) {
            dist[p.first] = -1;
            sigma[p.first] = 0.0;
        }

        if (dist.find(source) == dist.end()) {
            return {dist, sigma}; // unknown source
        }

        queue<string> q;
        dist[source] = 0;
        sigma[source] = 1.0;
        q.push(source);

        while (!q.empty()) {
            string u = q.front(); q.pop();
            auto it = adj.find(u);
            if (it == adj.end()) continue;
            for (const string &w : it->second) {
                if (dist[w] < 0) {
                    dist[w] = dist[u] + 1;
                    q.push(w);
                }
                if (dist[w] == dist[u] + 1) {
                    sigma[w] += sigma[u];
                }
            }
        }
        return {dist, sigma};
    }

    // Check if v is on shortest paths from s to t; returns (onPath, fractionOfShortestPathsThroughV)
    pair<bool,double> isOnShortestPath(const string &s, const string &t, const string &v,
                                       const unordered_map<string, vector<string>> &adj,
                                       const unordered_map<string, vector<string>> &revAdj) {
        auto ds = bfs_count_paths(s, adj);
        auto dtRev = bfs_count_paths(t, revAdj);

        auto &distS = ds.first;
        auto &sigmaS = ds.second;
        auto &distTrev = dtRev.first;
        auto &sigmaRev = dtRev.second;

        if (distS.find(t) == distS.end() || distS[t] < 0) return {false, 0.0};
        if (distS.find(v) == distS.end() || distTrev.find(v) == distTrev.end()) return {false, 0.0};
        if (distS[v] < 0 || distTrev[v] < 0) return {false, 0.0};
        if (distS[v] + distTrev[v] != distS[t]) return {false, 0.0};

        double pathsThroughV = sigmaS[v] * sigmaRev[v];
        double totalPaths = sigmaS[t];
        if (totalPaths == 0.0) return {false, 0.0};
        double fraction = pathsThroughV / totalPaths;
        return {pathsThroughV > 0.0, fraction};
    }

    pair<bool,double> isOnShortestPathByEmail(const string &sEmail, const string &tEmail, const string &vEmail) {
        if (emailToToken.find(sEmail) == emailToToken.end()) throw invalid_argument("Source email not found: " + sEmail);
        if (emailToToken.find(tEmail) == emailToToken.end()) throw invalid_argument("Target email not found: " + tEmail);
        if (emailToToken.find(vEmail) == emailToToken.end()) throw invalid_argument("Candidate email not found: " + vEmail);
        string s = emailToToken[sEmail], t = emailToToken[tEmail], v = emailToToken[vEmail];
        return isOnShortestPath(s, t, v, graph, buildReverseGraph());
    }

    // Add a new user by email. Will generate a token deterministically.
    void addUser(const string &email) {
        if (emailToToken.find(email) != emailToToken.end()) {
            cout << "User already exists: " << email << "\n";
            return;
        }
        string token = makeToken(email);

        // In very rare case of collision, append numeric suffix until unique.
        int suffix = 1;
        while (graph.find(token) != graph.end()) {
            token = makeToken(email) + "_c" + to_string(suffix++);
        }

        graph[token] = {};
        tokenToEmail[token] = email;
        emailToToken[email] = token;

        compSize[token] = 1;
        parent[token] = token;
        indegreeZero.insert(token);

        referralCount[token] = 0;
    }

    // Get direct+indirect referral count stored for an email's token
    int getRefferalCount(const string &email) const {
        auto it = emailToToken.find(email);
        if (it == emailToToken.end()) throw invalid_argument("User not found: " + email);
        const string &token = it->second;
        auto it2 = referralCount.find(token);
        if (it2 == referralCount.end()) return 0;
        return it2->second;
    }

    // Add a directed referral (referrerEmail -> candidateEmail). Enforces constraints.
    void addReferralByEmail(const string &referrerEmail, const string &candidateEmail) {
        if (emailToToken.find(referrerEmail) == emailToToken.end() ||
            emailToToken.find(candidateEmail) == emailToToken.end()) {
            throw invalid_argument("Both users must be added before creating referral.");
        }
        string refToken = emailToToken[referrerEmail];
        string candToken = emailToToken[candidateEmail];

        if (refToken == candToken) throw invalid_argument("Self-referrals are not allowed.");
        if (referredBy.find(candToken) != referredBy.end()) throw invalid_argument("Candidate already has a referrer.");
        if (!dsu_union(refToken, candToken)) throw invalid_argument("Adding this referral would create a cycle.");

        // add edge
        graph[refToken].push_back(candToken);
        referredBy[candToken] = refToken;
        indegreeZero.erase(candToken);

        // update referral counts up the ancestor chain of refToken
        string cur = refToken;
        while (true) {
            int oldCount = referralCount[cur];
            if (oldCount > 0) removeFromReferalBucket(oldCount, cur);
            referralCount[cur] = oldCount + 1;
            referaltotoken[referralCount[cur]].insert(cur);

            if (referredBy.find(cur) == referredBy.end()) break;
            cur = referredBy[cur];
        }
    }

    // Return direct referrals (emails) for an email
    vector<string> getDirectReferralsByEmail(const string &email) const {
        vector<string> out;
        auto it = emailToToken.find(email);
        if (it == emailToToken.end()) return out;
        const string &token = it->second;
        auto it2 = graph.find(token);
        if (it2 == graph.end()) return out;
        for (const string &childToken : it2->second) out.push_back(tokenToEmail.at(childToken));
        return out;
    }

    // Return top-k referrers (by stored referral count) as emails.
    unordered_set<string> topKReferrers(int k) const {
        if (k <= 0) return {};
        // Note: if referredBy.size() is used to check bounds elsewhere, we keep current behavior
        unordered_set<string> res;
        for (const auto &entry : referaltotoken) {
            if (entry.first <= 0) continue;
            for (const auto &tok : entry.second) {
                res.insert(tokenToEmail.at(tok));
                if ((int)res.size() == k) return res;
            }
        }
        return res;
    }

    // returns vector cumulative where cumulative[d] = expected cumulative referrals by end of day d
    vector<long double> simulate(long double p, int days, int initialReferrers = 100, int capacity = 10) const {
        if (days < 0) return {};
        vector<long double> cumulative(days + 1, 0.0L); // index 0 = 0
        if (days == 0) return cumulative;

        // Precompute cdf[t] = P(Binomial(t, p) < capacity) for t = 0..days
        vector<long double> cdf(days + 1, 0.0L);
        long double q = 1.0L - p;
        for (int t = 0; t <= days; ++t) {
            if (p == 1.0L) {
                cdf[t] = (t < capacity) ? 1.0L : 0.0L;
                continue;
            }
            if (p == 0.0L) {
                cdf[t] = 1.0L;
                continue;
            }
            long double pmf = powl(q, t); // k=0
            long double sum = pmf;
            int upto = min(capacity - 1, t);
            for (int k = 1; k <= upto; ++k) {
                pmf = pmf * ((long double)(t - k + 1) / (long double)k) * (p / q);
                sum += pmf;
            }
            cdf[t] = sum;
        }

        // cohorts: cohort[s] = expected number of referrers that start on day s (1-based)
        vector<long double> cohort(days + 2, 0.0L);
        cohort[1] = (long double)initialReferrers;

        long double cum = 0.0L;
        const long double EPS = 1e-18L;
        int windowCut = days;
        for (int t = 0; t <= days; ++t) {
            if (cdf[t] < EPS) { windowCut = t; break; }
        }

        for (int d = 1; d <= days; ++d) {
            long double new_successes = 0.0L;
            int smin = max(1, d - windowCut);
            for (int s = smin; s <= d; ++s) {
                int pastTrials = d - s;
                new_successes += cohort[s] * p * cdf[pastTrials];
            }
            cum += new_successes;
            cumulative[d] = cum;
            if (d + 1 <= days + 1) cohort[d + 1] = new_successes;
        }

        return cumulative;
    }


    int days_to_target(long double p, long double target_total,
                       int initialReferrers = 100, int capacity = 10,
                       int max_days_limit = 100000) const {
        if (target_total <= 0.0L) return 0;
        if (p < 0.0L || p > 1.0L) throw invalid_argument("p must be in [0,1]");

        int maxT = max_days_limit;
        vector<long double> cdf(maxT + 1, 0.0L);
        long double q = 1.0L - p;
        for (int t = 0; t <= maxT; ++t) {
            if (p == 1.0L) { cdf[t] = (t < capacity) ? 1.0L : 0.0L; continue; }
            if (p == 0.0L) { cdf[t] = 1.0L; continue; }
            long double pmf = powl(q, t);
            long double sum = pmf;
            int upto = min(capacity - 1, t);
            for (int k = 1; k <= upto; ++k) {
                pmf = pmf * ((long double)(t - k + 1) / (long double)k) * (p / q);
                sum += pmf;
            }
            cdf[t] = sum;
        }

        vector<long double> cohort;
        cohort.reserve(1024);
        cohort.push_back(0.0L);             // index 0 unused
        cohort.push_back((long double)initialReferrers); // cohort[1]

        long double cum = 0.0L;
        const long double EPS = 1e-18L;
        int windowCut = maxT;
        for (int t = 0; t <= maxT; ++t) if (cdf[t] < EPS) { windowCut = t; break; }

        for (int d = 1; d <= max_days_limit; ++d) {
            if ((int)cohort.size() <= d) cohort.resize(d + 1, 0.0L);

            long double new_successes = 0.0L;
            int smin = max(1, d - windowCut);
            for (int s = smin; s <= d && s < (int)cohort.size(); ++s) {
                int pastTrials = d - s;
                new_successes += cohort[s] * p * cdf[pastTrials];
            }

            cum += new_successes;
            cohort.push_back(new_successes);
            if (cum >= target_total - 1e-12L) return d;
        }

        return -1;
    }

    int min_bonus_for_target(int days,
                             int target_hires,
                             function<long double(int)> adoption_prob,
                             long double eps = 1e-3L,
                             int maxBonus = 10000000 /* 10M */) const {
        if (days < 0 || target_hires <= 0) return 0;

        // helper: decide if 'bonus' is sufficient
        auto sufficient = [&](int bonus) -> bool {
            long double p = adoption_prob(bonus);
            if (p < 0.0L) p = 0.0L;
            if (p > 1.0L) p = 1.0L;
            int daysNeeded = days_to_target(p, (long double)target_hires, 100, 10, days);
            return (daysNeeded != -1 && daysNeeded <= days);
        };

        // quick check zero bonus
        if (sufficient(0)) return 0;

        // exponential search to find an upper bound where sufficient(high) == true
        int low = 0;
        int high = 10;
        int iter = 0;
        const int MAX_EXP_ITER = 40;
        while (high <= maxBonus && iter < MAX_EXP_ITER && !sufficient(high)) {
            low = high;
            high = high * 2;
            ++iter;
        }

        if (high > maxBonus) {
            if (!sufficient(maxBonus)) return -1;
            high = maxBonus;
        } else {
            if (iter >= MAX_EXP_ITER && !sufficient(high)) return -1;
        }

        // binary search on multiples of $10; convert to k = bonus/10 integers
        int lowK = (low + 9) / 10;
        int highK = (high + 9) / 10;
        while (lowK < highK) {
            int midK = lowK + (highK - lowK) / 2;
            int midBonus = midK * 10;
            if (sufficient(midBonus)) highK = midK;
            else lowK = midK + 1;
        }
        return highK * 10;
    }
};

int main() {
    ReferralGraph g;

    // create some users (keeps behavior identical to original)
    g.addUser("krish@gmail.com");
    g.addUser("bob@gmail.com");
    g.addUser("charlie@gmail.com");
    g.addUser("hj@gmail.com");

    // build referrals
    g.addReferralByEmail("krish@gmail.com", "hj@gmail.com");
    g.addReferralByEmail("bob@gmail.com", "charlie@gmail.com");
    g.addReferralByEmail("krish@gmail.com", "bob@gmail.com");

    // direct referrals print
    auto referrals = g.getDirectReferralsByEmail("krish@gmail.com");
    cout << "krish@gmail.com referred: ";
    for (auto &r : referrals) cout << r << " ";
    cout << "\n";

    cout << "krish total referrals: " << g.getRefferalCount("krish@gmail.com") << "\n";
    cout << "bob total referrals: " << g.getRefferalCount("bob@gmail.com") << "\n";
    cout << "charlie total referrals: " << g.getRefferalCount("charlie@gmail.com") << "\n";

    // shortest path checks (demo)
    cout << fixed << setprecision(4);
    auto res1 = g.isOnShortestPathByEmail("krish@gmail.com", "charlie@gmail.com", "bob@gmail.com");
    cout << "Is 'bob' on a shortest path krish->charlie? " << (res1.first ? "YES" : "NO") << " fraction=" << res1.second << "\n";
    auto res2 = g.isOnShortestPathByEmail("krish@gmail.com", "charlie@gmail.com", "hj@gmail.com");
    cout << "Is 'hj' on a shortest path krish->charlie? " << (res2.first ? "YES" : "NO") << " fraction=" << res2.second << "\n";

    // simulate example
    long double p = 0.2L;
    int days = 30;
    auto cum = g.simulate(p, days);
    cout << "\nSimulation (expected cumulative referrals):\n";
    for (int d = 0; d <= days; ++d) cout << "Day " << d << ": " << (double)cum[d] << "\n";

    // days_to_target example
    long double target = 50.0L;
    int need = g.days_to_target(p, target, 100, 10, 10000);
    cout << "\nDays to reach expected target " << target << ": " << need << "\n";

    // demo adoption_prob (monotonic), then compute min_bonus_for_target
    auto adoption_prob_demo = [](int bonus)->long double {
        long double b = (long double)bonus;
        long double p_ = 0.95L * (1.0L - expl(-b / 100.0L)); // simple demo mapping
        if (p_ < 0.0L) p_ = 0.0L;
        if (p_ > 1.0L) p_ = 1.0L;
        return p_;
    };

    int target_hires = 50;
    int minBonus = g.min_bonus_for_target(days, target_hires, adoption_prob_demo, 1e-3L, 1000000);
    if (minBonus >= 0) {
        cout << "\nMinimum bonus (rounded to $10) to reach " << target_hires << " expected hires in " << days << " days: $" << minBonus << "\n";
    } else {
        cout << "\nTarget not achievable within maxBonus limit.\n";
    }

    return 0;
}
