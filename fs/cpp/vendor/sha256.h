/*
 * PicoSHA2: a C++11 header-only SHA-256 implementation.
 * https://github.com/okvis/picosha2
 * Licensed under the MIT License.
 */

#ifndef PICOSHA2_H
#define PICOSHA2_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>
#include <iomanip>

namespace picosha2 {
typedef std::uint32_t word_t;
typedef std::uint8_t byte_t;

static const word_t k_initial_h[8] = {
    0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

static const word_t k_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline word_t ch(word_t x, word_t y, word_t z) { return (x & y) ^ ((~x) & z); }
inline word_t maj(word_t x, word_t y, word_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}
inline word_t rotr(word_t x, std::size_t n) {
    assert(n < 32);
    return (x >> n) | (x << (32 - n));
}
inline word_t sigma0(word_t x) {
    return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
}
inline word_t sigma1(word_t x) {
    return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
}
inline word_t gamma0(word_t x) {
    return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
}
inline word_t gamma1(word_t x) {
    return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
}

class hash256_one_by_one {
   public:
    hash256_one_by_one() { init(); }
    void init() {
        buffer_.clear();
        std::copy(k_initial_h, k_initial_h + 8, h_);
        data_length_ = 0;
    }
    template <typename RaIt>
    void process(RaIt first, RaIt last) {
        for (auto it = first; it != last; ++it) {
            buffer_.push_back(static_cast<byte_t>(*it));
            if (buffer_.size() == 64) {
                process_block(buffer_.begin());
                buffer_.clear();
            }
            data_length_ += 8;
        }
    }
    void finish() {
        std::uint64_t temp_data_length = data_length_;
        buffer_.push_back(0x80);
        while (buffer_.size() != 56) {
            if (buffer_.size() == 64) {
                process_block(buffer_.begin());
                buffer_.clear();
            }
            buffer_.push_back(0x00);
        }
        for (int i = 0; i < 8; ++i) {
            buffer_.push_back(
                static_cast<byte_t>((temp_data_length >> (56 - 8 * i)) & 0xff));
        }
        process_block(buffer_.begin());
        buffer_.clear();
    }
    template <typename OutIt>
    void get_hash_bytes(OutIt first, OutIt last) const {
        for (const word_t* it = h_; it != h_ + 8; ++it) {
            for (int i = 0; i < 4; ++i) {
                *first = static_cast<byte_t>((*it >> (24 - 8 * i)) & 0xff);
                ++first;
                if (first == last) {
                    return;
                }
            }
        }
    }

   private:
    void process_block(std::vector<byte_t>::const_iterator it) {
        word_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<word_t>(it[4 * i]) << 24) |
                   (static_cast<word_t>(it[4 * i + 1]) << 16) |
                   (static_cast<word_t>(it[4 * i + 2]) << 8) |
                   (static_cast<word_t>(it[4 * i + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];
        }
        word_t a = h_[0];
        word_t b = h_[1];
        word_t c = h_[2];
        word_t d = h_[3];
        word_t e = h_[4];
        word_t f = h_[5];
        word_t g = h_[6];
        word_t h = h_[7];
        for (int i = 0; i < 64; ++i) {
            word_t t1 = h + sigma1(e) + ch(e, f, g) + k_constants[i] + w[i];
            word_t t2 = sigma0(a) + maj(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h_[0] += a;
        h_[1] += b;
        h_[2] += c;
        h_[3] += d;
        h_[4] += e;
        h_[5] += f;
        h_[6] += g;
        h_[7] += h;
    }
    word_t h_[8];
    std::vector<byte_t> buffer_;
    std::uint64_t data_length_;
};

inline std::string bytes_to_hex_string(const std::vector<byte_t>& bytes) {
    std::stringstream ss;
    for (byte_t b : bytes) {
        ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(b);
    }
    return ss.str();
}

inline std::string hash256_hex_string(const std::string& src) {
    hash256_one_by_one hasher;
    hasher.process(src.begin(), src.end());
    hasher.finish();
    std::vector<byte_t> hash(32);
    hasher.get_hash_bytes(hash.begin(), hash.end());
    return bytes_to_hex_string(hash);
}

}  // namespace picosha2

#endif
