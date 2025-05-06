#ifndef HASH_H
#define HASH_H

#include <iostream>
#include <string>
#include <algorithm>
#include <cctype>
#include <random>
#include <chrono>

typedef std::size_t HASH_INDEX_T;

struct MyStringHash {
    // default debug values:
    HASH_INDEX_T rValues[5] {
        983132572u, 1468777056u, 552714139u, 984953261u, 261934300u
    };

    MyStringHash(bool debug = true) {
        if (!debug) {
            generateRValues();
        }
    }

    // --- your hash function ---
    HASH_INDEX_T operator()(const std::string& k) const {
        static const unsigned long long BASE = 36ull;
        // w[0]…w[4] all start at zero:
        unsigned long long w[5] = {0,0,0,0,0};

        int n = static_cast<int>(k.size());
        // Precompute up to 5 chunks of length ≤6, starting from the end:
        for (int chunk = 0; chunk < 5; ++chunk) {
            int endPos   = n - chunk * 6; 
            if (endPos <= 0) break;               // no more chars left
            int startPos = std::max(0, endPos - 6);

            // convert substring k[startPos..endPos-1] from base-36:
            unsigned long long v = 0;
            for (int i = startPos; i < endPos; ++i) {
                v = v * BASE + letterDigitToNumber(k[i]);
            }

            // store into w[4-chunk], so last-chunk → w[4], next → w[3], … first → w[0]
            w[4 - chunk] = v;
        }

        // compute the final hash = Σ rValues[i] * w[i]
        unsigned long long h = 0;
        for (int i = 0; i < 5; ++i) {
            h += static_cast<unsigned long long>(rValues[i]) * w[i];
        }
        return static_cast<HASH_INDEX_T>(h);
    }

    // helper: map 'a'/'A'→0, 'b'→1, …, 'z'→25, '0'→26, …, '9'→35
    HASH_INDEX_T letterDigitToNumber(char c) const {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            return static_cast<HASH_INDEX_T>(std::tolower(static_cast<unsigned char>(c)) - 'a');
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            return static_cast<HASH_INDEX_T>(c - '0' + 26);
        }
        // should never happen under the problem constraints:
        return 0;
    }

    // randomize rValues when debug==false
    void generateRValues() {
        unsigned seed = static_cast<unsigned>(
            std::chrono::system_clock::now()
                .time_since_epoch()
                .count()
        );
        std::mt19937 gen(seed);
        for (int i = 0; i < 5; ++i) {
            rValues[i] = gen();
        }
    }
};

#endif
