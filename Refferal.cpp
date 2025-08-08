#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stack>
#include <stdexcept>
#include <string>

using namespace std;

class ReferralGraph {
private:
    unordered_map<string, vector<string>> graph;      // token -> direct referrals
    unordered_map<string, string> referredBy;         // token -> referrer token

    unordered_map<string, string> tokenToEmail;       // token -> email
    unordered_map<string, string> emailToToken;       // email -> token

    bool createsCycle(const string& referrerToken, const string& candidateToken) {
        unordered_set<string> visited;
        stack<string> stk;
        stk.push(candidateToken);

        while (!stk.empty()) {
            string current = stk.top();
            stk.pop();
            if (current == referrerToken) return true;
            visited.insert(current);
            for (const auto& neighbor : graph[current]) {
                if (visited.find(neighbor) == visited.end()) {
                    stk.push(neighbor);
                }
            }
        }
        return false;
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

public:
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

        if (createsCycle(referrerToken, candidateToken)) {
            throw invalid_argument("Adding this referral would create a cycle.");
        }

        graph[referrerToken].push_back(candidateToken);
        referredBy[candidateToken] = referrerToken;
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

    // Add users with emails
    g.addUser("krish@gmail.com");
    g.addUser("bob@gamil.com");
    g.addUser("charlie@gamil.com");

    // Add referral relationships by email
    g.addReferralByEmail("alice@gmail.com", "bob@gmail.com");

    // Print direct referrals of Alice
    auto referrals = g.getDirectReferralsByEmail("alice@example.com");
    cout << "alice@example.com referred: ";
    for (const auto& r : referrals) {
        cout << r << " ";
    }
    cout << endl;

    return 0;
}
