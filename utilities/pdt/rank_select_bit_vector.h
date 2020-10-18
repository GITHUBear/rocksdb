//
// Created by Dim Dew on 2020-07-22.
//

#ifndef PATH_DECOMPOSITION_TRIE_RANK_SELECT_BIT_VECTOR_H
#define PATH_DECOMPOSITION_TRIE_RANK_SELECT_BIT_VECTOR_H

#include "bit_vector.h"

namespace rocksdb {
namespace succinct {

    // In BitVector, bits are combined in the following format:
    //
    //         64bit  64bit        64bit  64bit
    //       +------+------+     +------+------+
    //       |      |      | ... |      |      |
    //       +------+------+     +------+------+
    //
    // While in RsBitVector, 64bits are organized logically in the following format:
    //
    //                 +-----+-----+-----+
    //       block 0   |     |     |     | ...  length: block_size
    //                 +-----+-----+-----+
    //                 A                     A
    //                 |                     |
    //                 next_rank             sub_rank
    //                 (calculated at        (calculated at
    //                 block head)           block tail)
    //
    //       block 1    ......
    //
    //         ...
    //
    //   dummy tail block
    //          A
    //          |
    // at last, a dummy tail block is used to save the number of one-bits in `next_rank`
    //
    class RsBitVector : public BitVector {
    public:
        RsBitVector()
            : BitVector()
        {}

        RsBitVector(const std::vector<bool>& bools,
                    bool with_select_hints = false,
                    bool with_select0_hints = false)
                    : BitVector(bools) {
            build_indices(with_select_hints, with_select0_hints);
        }

        RsBitVector(BitVectorBuilder* builder,
                    bool with_select_hints = false,
                    bool with_select0_hints = false)
                    : BitVector(builder) {
            build_indices(with_select_hints, with_select0_hints);
        }

        RsBitVector(const uint64_t* raw_data,
                    uint64_t word_size,
                    size_t bit_size,
                    bool with_select_hints = false,
                    bool with_select0_hints = false)
                    : BitVector(raw_data, word_size, bit_size) {
            build_indices(with_select_hints, with_select0_hints);
        }

        void swap(RsBitVector& other) {
            BitVector::swap(other);
            m_block_rank_pairs_.swap(other.m_block_rank_pairs_);
            m_select_hints_.swap(other.m_select_hints_);
            m_select0_hints_.swap(other.m_select0_hints_);
        }

        inline uint64_t num_ones() const {
            return *(m_block_rank_pairs_.end() - 2);
        }

        inline uint64_t num_zeros() const {
            return size() - num_ones();
        }

        // get the number of 1-bits in range [0, `pos`)
        inline uint64_t rank(uint64_t pos) const {
            assert(pos <= size());
            if (pos == size()) {
                return num_ones();
            }

            uint64_t word = pos / 64;
            uint64_t r = word_rank(word);
            uint64_t sub_left = pos % 64;
            if (sub_left) {
                r += util::popcount(m_bits_[word] << (64 - sub_left));
            }
            return r;
        }

        // get the number of 0-bits in range [0, `pos`)
        inline uint64_t rank0(uint64_t pos) const {
            return pos - rank(pos);
        }

        // get `n`-th 1-bit's position in bits
        // `n` starts from 0
        inline uint64_t select(uint64_t n) const {
            assert(n < num_ones());
            // The possible block index range of `n`-th 1-bit
            // is [block_begin, block end)
            uint64_t block_begin = 0;
            uint64_t block_end = num_blocks();
            if (m_select_hints_.size()) {
                uint64_t chunk = n / select_ones_per_hint;
                if (chunk != 0) {
                    block_begin = m_select_hints_[chunk - 1];
                }
                block_end = m_select_hints_[chunk] + 1;
            }

            uint64_t block = 0;
            // binary search in block index range [block_begin, block_end)
            while (block_end - block_begin > 1) {
                uint64_t mid = block_begin + (block_end - block_begin) / 2;
                uint64_t x = block_rank(mid);
                if (x <= n) {
                    block_begin = mid;
                } else {
                    block_end = mid;
                }
            }
            block = block_begin;

            assert(block < num_blocks());
            uint64_t block_offset = block * block_size;
            uint64_t cur_rank = block_rank(block);
            assert(cur_rank <= n);

            uint64_t w_ranks = word_ranks(block);
            uint64_t sub_block_offset = get_sub_block_offset(w_ranks, n - cur_rank);
            cur_rank += w_ranks >> ((7 - sub_block_offset) * 9) & 0x1FF;
            assert(cur_rank <= n);

            uint64_t word_offset = block_offset + sub_block_offset;
            return word_offset * 64 + util::select_in_word(m_bits_[word_offset], n - cur_rank);
        }

        // get `n`-th 0-bit's position in bits
        // `n` starts from 0
        inline uint64_t select0(uint64_t n) const {
            assert(n < num_zeros());
            uint64_t block_begin = 0;
            uint64_t block_end = num_blocks();
            if (m_select0_hints_.size()) {
                uint64_t chunk = n / select_zeros_per_hint;
                if (chunk != 0) {
                    block_begin = m_select0_hints_[chunk - 1];
                }
                block_end = m_select0_hints_[chunk] + 1;
            }

            uint64_t block = 0;
            while (block_end - block_begin > 1) {
                uint64_t mid = block_begin + (block_end - block_begin) / 2;
                uint64_t x = block_rank0(mid);
                if (x <= n) {
                    block_begin = mid;
                } else {
                    block_end = mid;
                }
            }
            block = block_begin;

            assert(block < num_blocks());
            uint64_t block_offset = block * block_size;
            uint64_t cur_rank0 = block_rank0(block);
            assert(cur_rank0 <= n);

            uint64_t w_ranks = 64 * INV_CNT_UNIT - word_ranks(block);
            uint64_t sub_block_offset = get_sub_block_offset(w_ranks, n - cur_rank0);
            cur_rank0 += w_ranks >> (7 - sub_block_offset) * 9 & 0x1FF;
            assert(cur_rank0 <= n);

            uint64_t word_offset = block_offset + sub_block_offset;
            return word_offset * 64 + util::select_in_word(~m_bits_[word_offset], n - cur_rank0);
        }

    protected:
        inline uint64_t num_blocks() const {
            // dummy block is excluded.
            return m_block_rank_pairs_.size() / 2 - 1;
        }

        inline uint64_t block_rank(uint64_t block) const {
            // get `next_rank` from `m_block_rank_pairs_`.
            return m_block_rank_pairs_[block * 2];
        }

        inline uint64_t block_rank0(uint64_t block) const {
            return block * block_size * 64 - block_rank(block);
        }

        // get `sub_rank` from `block`-th block
        // `block`: block index
        inline uint64_t word_ranks(uint64_t block) const {
            return m_block_rank_pairs_[block * 2 + 1];
        }

        // `word`-th word's rank = block's next_rank + word's sub_rank
        // `word`: word index
        inline uint64_t word_rank(uint64_t word) const {
            uint64_t r = 0;
            uint64_t block = word / block_size;
            r += block_rank(block);
            uint64_t left = word % block_size;
            r += word_ranks(block) >> ((7 - left) * 9) & 0x1FF;
            return r;
        }

        // `block_offset` is in word.
        // `block`: block index
        // `k`: 1-bit's rank in `block`-indexed block
        inline uint64_t get_sub_block_offset(uint64_t w_ranks, uint64_t k) const {
            uint64_t w_ks = k * SUB_RANK_UNIT;

            const uint64_t indicator = 0x100ULL * SUB_RANK_UNIT;
            // let r = (w_ks | indicator) - (w_ranks & ~indicator)
            // let k = w_ks and x = w_ranks
            // use `w_flags` to represent if w_ks >= w_ranks in each 9-bits
            // We have the logistic statement:
            //   k(r|(~r~x)) & ~k(r~x)
            // = krx | k~x | ~kr~x
            // = r(kx & ~k~x) | k~x
            // = r & (~(k ^ x)) | (k & ~x)
            const uint64_t r = (w_ks | indicator) - (w_ranks & (~indicator));
            uint64_t w_flags =
                    ((r & (~(w_ks ^ w_ranks))) | (w_ks & (~w_ranks))) & indicator;

            return (((w_flags >> 8) * SUB_RANK_UNIT) >> 54) & 0x7;
        }

        // call after BitVector is constructed.
        void build_indices(bool with_select_hints, bool with_select0_hints) {
            // 1. build m_block_rank_pairs_
            std::vector<uint64_t> block_rank_pairs;

            uint64_t next_rank = 0;
            uint64_t cur_sub_rank = 0;
            uint64_t sub_ranks = 0;
            // Initially, next_rank = 0
            block_rank_pairs.push_back(0);
            for (uint64_t i = 0; i < m_bits_.size(); i++) {
                uint64_t word_pop = util::popcount(m_bits_[i]);
                uint64_t shift = i % block_size;
                // TODO: Is if-statement necessary?
                if (shift) {
                    // 8 words in a block. We have 8 * 64 popcount at most.
                    // 2^3 * 2^6, so 3 + 6 = 9.
                    sub_ranks <<= 9;
                    sub_ranks |= cur_sub_rank;
                    // sub_ranks contains 7 cur_sub_rank
                }
                next_rank += word_pop;
                cur_sub_rank += word_pop;
                if (shift == block_size - 1) {
                    block_rank_pairs.push_back(sub_ranks);
                    block_rank_pairs.push_back(next_rank);
                    sub_ranks = 0;
                    cur_sub_rank = 0;
                }
            }
            // add last sub_ranks of a block (Maybe dummy).
            uint64_t left = block_size - m_bits_.size() % block_size;
            for (uint64_t i = 0; i < left; ++i) {
                sub_ranks <<= 9;
                sub_ranks |= cur_sub_rank;
            }
            block_rank_pairs.push_back(sub_ranks);
            // a dummy tail block
            if (m_bits_.size() % block_size) {
                block_rank_pairs.push_back(next_rank);
                block_rank_pairs.push_back(0);
            }

            m_block_rank_pairs_.steal(block_rank_pairs);

            // 2. build m_select_hints_
            if (with_select_hints) {
                std::vector<uint64_t> select_hints;
                uint64_t cur_ones_threshold = select_ones_per_hint;
                for (uint64_t i = 0; i < num_blocks(); ++i) {
                    if (block_rank(i + 1) > cur_ones_threshold) {
                        select_hints.push_back(i);
                        cur_ones_threshold += select_ones_per_hint;
                    }
                }
                select_hints.push_back(num_blocks());
                m_select_hints_.steal(select_hints);
            }

            // 3. build m_select0_hints_
            if (with_select0_hints) {
                std::vector<uint64_t> select0_hints;
                uint64_t cur_zeros_threshold = select_zeros_per_hint;
                for (uint64_t i = 0; i < num_blocks(); ++i) {
                    if (block_rank0(i + 1) > cur_zeros_threshold) {
                        select0_hints.push_back(i);
                        cur_zeros_threshold += select_zeros_per_hint;
                    }
                }
                select0_hints.push_back(num_blocks());
                m_select0_hints_.steal(select0_hints);
            }
        }

        static const uint64_t block_size = 8; // in 64bit words
        static const uint64_t select_ones_per_hint = 64 * block_size * 2; // must be > block_size * 64
        static const uint64_t select_zeros_per_hint = select_ones_per_hint;

        static const uint64_t SUB_RANK_UNIT =
                1ULL << 0 | 1ULL << 9 | 1ULL << 18 | 1ULL << 27 | 1ULL << 36 | 1ULL << 45 | 1ULL << 54;

        static const uint64_t INV_CNT_UNIT =
                1ULL << 54 | 2ULL << 45 | 3ULL << 36 | 4ULL << 27 | 5ULL << 18 | 6ULL << 9 | 7ULL;

        typedef mappable_vector<uint64_t> uint64_vec;
        uint64_vec m_block_rank_pairs_;               // `next_rank` & `sub_ranks` pairs
        uint64_vec m_select_hints_;                   // block index
        uint64_vec m_select0_hints_;
    };
}
}

#endif //PATH_DECOMPOSITION_TRIE_RANK_SELECT_BIT_VECTOR_H
