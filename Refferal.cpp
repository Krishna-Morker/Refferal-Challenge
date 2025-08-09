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

using namespace std;

class ReferralGraph {
private:
    unordered_map<string, vector<string>> graph;      // token -> direct referrals
    unordered_map<string, string> referredBy;         // token -> referrer token

    unordered_map<string, string> tokenToEmail;       // token -> email
    unordered_map<string, string> emailToToken;       // email -> token

    unordered_map<string,string> parent;    // mapping token to parent token (DSU)
    unordered_map<string,int> size;         // size of the component for union-find

    // NEW: store referral count (direct + indirect descendants) per token
    unordered_map<string,int> referralCount;
    map<int,unordered_set<string>,greater<int>> referaltotoken; // map referral count to tokens

    unordered_set<string> indegree_zero; // in-degree for each token


    string find(const string& token) {
        if (parent[token] != token)
            parent[token] = find(parent[token]);
        return parent[token];
    }

    bool unionSet(const string& a, const string& b) {
        string rootA = find(a);
        string rootB = find(b);
        if (rootA == rootB) return false; // Cycle detected
        parent[rootB] = rootA;
        size[rootA] += size[rootB];
        return true;
    }

    unsigned long long fnv1aHash(const string& str) {
        const unsigned long long FNV_offset_basis = 14695981039346656037ULL;
        const unsigned long long FNV_prime = 1099511628211ULL;

        unsigned long long hash = FNV_offset_basis;
        for (char c : str) {
            hash ^= static_cast<unsigned long long>(c);
            hash *= FNV_prime;
        }
        return hash;
    }

    string generateToken(const string& email) {
        return "token_" + to_string(fnv1aHash(email));
    }

    int getComponentSize(const string& token) {
        return size[(token)];
    }

public:
    // Helper: return list of all tokens (nodes)
    vector<string> getAllTokens() const {
        vector<string> nodes;
        nodes.reserve(tokenToEmail.size());
        for (const auto &p : tokenToEmail) nodes.push_back(p.first);
        return nodes;
    }

    // Build reversed adjacency (contains all nodes, even if no incoming/outgoing edges)
    unordered_map<string, vector<string>> buildReverseGraph() const {
        unordered_map<string, vector<string>> r;
        // ensure every node exists
        for (const auto &p : tokenToEmail) {
            r[p.first] = {};
        }
        for (const auto &kv : graph) {
            const string &u = kv.first;
            for (const string &v : kv.second) {
                r[v].push_back(u);
            }
        }
        return r;
    }

    // BFS that returns pair: dist map and sigma map (number of shortest paths)
    // NOTE: initialize dist and sigma for ALL nodes (tokenToEmail), not just keys in adj
    pair<unordered_map<string,int>, unordered_map<string,double>> 
    bfs_count_paths(const string &source,
                    const unordered_map<string, vector<string>> &adj) {
        unordered_map<string,int> dist;
        unordered_map<string,double> sigma;

        // initialize for all nodes so nodes with 0-degree are included
        for (const auto &p : tokenToEmail) {
            dist[p.first] = -1;
            sigma[p.first] = 0.0;
        }

        queue<string> q;
        if (dist.find(source) == dist.end()) {
            // unknown source token
            return {dist, sigma};
        }

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

    // Check if v is on the shortest path from s to t
    pair<bool,double> isOnShortestPath(
        const string &s, const string &t, const string &v,
        const unordered_map<string, vector<string>> &adj,
        const unordered_map<string, vector<string>> &rgraph) {

        auto ds_sigma = bfs_count_paths(s, adj);
        auto dtrev_sigmarev = bfs_count_paths(t, rgraph); // BFS from t on reversed graph

        auto &dist_s = ds_sigma.first;
        auto &sigma_s = ds_sigma.second;
        auto &dist_trev = dtrev_sigmarev.first;
        auto &sigma_rev = dtrev_sigmarev.second;

        // check reachable and distance condition
        if (dist_s.find(t) == dist_s.end() || dist_s[t] < 0) {
            return {false, 0.0}; // no s->t path at all
        }
        if (dist_s.find(v) == dist_s.end() || dist_trev.find(v) == dist_trev.end()) {
            return {false, 0.0};
        }
        if (dist_s[v] < 0 || dist_trev[v] < 0) return {false, 0.0};

        if (dist_s[v] + dist_trev[v] != dist_s[t]) return {false, 0.0};

        double paths_through_v = sigma_s[v] * sigma_rev[v]; // number of s->t shortest paths going through v
        double total_paths = sigma_s[t];
        if (total_paths == 0.0) return {false, 0.0}; // safety check

        double fraction = paths_through_v / total_paths;
        return {paths_through_v > 0.0, fraction};
    }

    // Public wrapper: take emails, map -> tokens, build reverse graph, call isOnShortestPath
    pair<bool,double> isOnShortestPathByEmail(const string &sEmail, const string &tEmail, const string &vEmail) {
        if (emailToToken.find(sEmail) == emailToToken.end()) 
            throw invalid_argument("Source email not found: " + sEmail);
        if (emailToToken.find(tEmail) == emailToToken.end()) 
            throw invalid_argument("Target email not found: " + tEmail);
        if (emailToToken.find(vEmail) == emailToToken.end()) 
            throw invalid_argument("Candidate (v) email not found: " + vEmail);

        string s = emailToToken[sEmail];
        string t = emailToToken[tEmail];
        string v = emailToToken[vEmail];

        auto rgraph = buildReverseGraph();
        return isOnShortestPath(s, t, v, graph, rgraph);
    }

    vector<string> findRootReferrer() {

        vector<string> directReferrals;
        unordered_set<string> visited;
        for (auto &email : tokenToEmail) {
            string token = email.first;
            if (indegree_zero.count(token) > 0) {


                stack<string> s;
                s.push(token);
                directReferrals.push_back(tokenToEmail[token]);

                while (!s.empty()) {
                    string current = s.top();
                    s.pop();

                    if (visited.count(current)) continue;
                    visited.insert(current);

                    for (const auto& child : graph[current]) {
                        s.push(child);
                    }
                }

                if (visited.size() == tokenToEmail.size()) {
                    break;
                }
            }
        }

        return directReferrals; // No such referrer found
    }

    // Add user by email, generate and store token
    void addUser(const string& email) {
        if (emailToToken.find(email) != emailToToken.end()) {
            cout << "User already exists: " << email << endl;
            return;
        }

        string token = generateToken(email);

        if (graph.find(token) != graph.end()) {
            throw runtime_error("Hash collision detected for token: " + token);
        }

        graph[token] = {};
        tokenToEmail[token] = email;
        emailToToken[email] = token;
        size[token]=1;
        parent[token] = token;
        indegree_zero.insert(token); // add to indegree zero set
        // initialize referral count
        referralCount[token] = 0;
    }

    // Get referral count by email (direct + indirect descendants)
    int getRefferalCount(const string& email) {
        if (emailToToken.find(email) == emailToToken.end()) {
            throw invalid_argument("User not found: " + email);
        }
        string token = emailToToken[email];
        // return stored referral count (O(1))
        return referralCount[token];
    }

    // Add referral link by emails
    void addReferralByEmail(const string& referrerEmail, const string& candidateEmail) {
        if (emailToToken.find(referrerEmail) == emailToToken.end() ||
            emailToToken.find(candidateEmail) == emailToToken.end()) {
            throw invalid_argument("Both users must be added before creating referral.");
        }

        string referrerToken = emailToToken[referrerEmail];
        string candidateToken = emailToToken[candidateEmail];

        if (referrerToken == candidateToken) {
            throw invalid_argument("Self-referrals are not allowed.");
        }

        if (referredBy.find(candidateToken) != referredBy.end()) {
            throw invalid_argument("Candidate already has a referrer.");
        }

        if (!unionSet(referrerToken, candidateToken)) {
            throw invalid_argument("Adding this referral would create a cycle.");
        }

        // Add edge in directed graph

        graph[referrerToken].push_back(candidateToken);
        referredBy[candidateToken] = referrerToken;
        indegree_zero.erase(candidateToken);

        // Update referralCount for referrer and all its ancestors
        string cur = referrerToken;
        while (true) {
            long long count=getRefferalCount(tokenToEmail[cur]);
            if(count > 0) {
                referaltotoken[count].erase(cur);
                if (referaltotoken[count].empty()) {
                    referaltotoken.erase(count);   // optional: clean up empty set
                }
            }
            referralCount[cur] += 1;
            // if(referralCount[cur] > 0) {
                referaltotoken[referralCount[cur]].insert(cur);
            // }

            if (referredBy.find(cur) == referredBy.end()) break; // reached top/root
            cur = referredBy[cur];
        }
    }

    unordered_set<string> topKReferrers(int k) {
        if(k>referredBy.size()) {
            throw invalid_argument("k exceeds the number of users.");
        }
        if (k <= 0) {
            return {}; // return empty vector for non-positive k
        }
        unordered_set<string> result;
        for (const auto& entry : referaltotoken) {
            if (entry.first > 0) { // only consider positive referral counts
                for (const auto& token : entry.second) {
                    result.insert(tokenToEmail[token]);
                    if (result.size() == k) return result; // return top k
                }
            }
        }
        return result; // return all if less than k
    }

    // Get direct referrals by email
    vector<string> getDirectReferralsByEmail(const string& email) {
        if (emailToToken.find(email) == emailToToken.end()) {
            return {};
        }

        string token = emailToToken[email];
        vector<string> result;
        for (const auto& referralToken : graph[token]) {
            result.push_back(tokenToEmail[referralToken]);
        }
        return result;
    }
};

int main() {
    ReferralGraph g;

    g.addUser("krish@gmail.com");
    g.addUser("bob@gmail.com");
    g.addUser("charlie@gmail.com");
    g.addUser("hj@gmail.com");

    // Build referrals
    g.addReferralByEmail("krish@gmail.com", "hj@gmail.com");
    g.addReferralByEmail("bob@gmail.com", "charlie@gmail.com");
    // Also connect krish -> bob to produce a path krish->bob->charlie
    g.addReferralByEmail("krish@gmail.com", "bob@gmail.com");

    // Print direct referrals of krish
    auto referrals = g.getDirectReferralsByEmail("krish@gmail.com");
    cout << "krish@gmail.com referred: ";
    for (const auto& r : referrals) {
        cout << r << " ";
    }
    cout << endl;

    cout << "krish total referrals: " << g.getRefferalCount("krish@gmail.com") << endl;
    cout << "bob total referrals: " << g.getRefferalCount("bob@gmail.com") << endl;
    cout << "charlie total referrals: " << g.getRefferalCount("charlie@gmail.com") << endl;

    // Now check some s, t, v triples
    cout << fixed << setprecision(4);
    auto res1 = g.isOnShortestPathByEmail("krish@gmail.com", "charlie@gmail.com", "bob@gmail.com");
    cout << "Is 'bob' on a shortest path krish->charlie? " << (res1.first ? "YES" : "NO") 
         << "  fraction=" << res1.second << "\n";

    auto res2 = g.isOnShortestPathByEmail("krish@gmail.com", "charlie@gmail.com", "hj@gmail.com");
    cout << "Is 'hj' on a shortest path krish->charlie? " << (res2.first ? "YES" : "NO") 
         << "  fraction=" << res2.second << "\n";

 
    return 0;
}
