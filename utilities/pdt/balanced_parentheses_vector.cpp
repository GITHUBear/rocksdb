//
// Created by Dim Dew on 2020-07-23.
//

#include "balanced_parentheses_vector.h"
#include <cassert>
#include <limits>

namespace rocksdb {
namespace succinct {
    // excess tables for excess infos in a byte.
    class excess_tables
    {
    public:
        excess_tables() {
            for (int c = 0; c < 256; ++c) {
                for (uint8_t i = 0; i < 9; ++i) {
                    m_fwd_pos[c][i] = 0;
                    m_bwd_pos[c][i] = 0;
                }
                // populate m_fwd_pos, m_fwd_min, and m_fwd_min_idx
                int excess = 0;
                m_fwd_min[c] = 0;
                m_fwd_min_idx[c] = 0;

                for (char i = 0; i < 8; ++i) {
                    if ((c >> i) & 1) { // opening
                        ++excess;
                    } else { // closing
                        --excess;
                        if (excess < 0 &&
                            m_fwd_pos[c][-excess] == 0) { // not already found
                            m_fwd_pos[c][-excess] = uint8_t(i + 1);
                        }
                    }

                    if (-excess > m_fwd_min[c]) {
                        m_fwd_min[c] = uint8_t(-excess);
                        m_fwd_min_idx[c] = uint8_t(i + 1);
                    }
                }
                m_fwd_exc[c] = (char)excess;

                // populate m_bwd_pos and m_bwd_min
                excess = 0;
                m_bwd_min[c] = 0;

                for (uint8_t i = 0; i < 8; ++i) {
                    if ((c << i) & 128) { // opening
                        ++excess;
                        if (excess > 0 &&
                            m_bwd_pos[c][(uint8_t)excess] == 0) { // not already found
                            m_bwd_pos[c][(uint8_t)excess] = uint8_t(i + 1);
                        }
                    } else { // closing
                        --excess;
                    }

                    m_bwd_min[c] = uint8_t(std::max(excess, (int)m_bwd_min[c]));
                }
            }
        }

        char m_fwd_exc[256];

        uint8_t m_fwd_pos[256][9];
        uint8_t m_bwd_pos[256][9];

        uint8_t m_bwd_min[256];    // maximum mismatched "(" in word
        uint8_t m_fwd_min[256];    // maximum mismatched ")" in word

        uint8_t m_fwd_min_idx[256];
    };

    const static excess_tables tables;

    bool find_open_in_word(
            uint64_t word, uint64_t byte_counts,
            BpVector::excess_t cur_exc, uint64_t& ret) {
        assert(cur_exc > 0 && cur_exc <= 64);
        const uint64_t rev_byte_counts = util::reverse_bytes(byte_counts);
        const uint64_t cum_exc_step_8 =
                (uint64_t(cur_exc) - ((2 * rev_byte_counts - 8 * util::BYTE_UNIT) << 8)) * util::BYTE_UNIT;

        // maximum mismatched "(" in word
        uint64_t max_exc_step_8 = 0;
        for (size_t i = 0; i < 8; ++i) {
            size_t shift = i * 8;
            max_exc_step_8 |= ((uint64_t)(tables.m_bwd_min[(word >> (64 - shift - 8)) & 0xFF])) << shift;
        }

        const uint64_t has_result = util::leq_bytes(cum_exc_step_8, max_exc_step_8);

        unsigned long shift;
        if (util::lsb(has_result, shift)) {
            uint8_t bit_pos = tables.m_bwd_pos[(word >> (64 - shift - 8)) & 0xFF][(cum_exc_step_8 >> shift) & 0xFF];
            assert(bit_pos > 0);
            ret = 64 - (shift + bit_pos);
            return true;
        }
        return false;
    }

    // search in the block index range `block_offset + [0, start - 1]`
    inline bool BpVector::find_open_in_block(
            uint64_t block_offset, BpVector::excess_t excess, uint64_t start, uint64_t& ret) const {
        // `excess` is the number of ")" that is mismatched.
        // `excess` > start * 64 means that even if all the remaining bits are 1,
        // the "(" we need to look for is not in local block.
        // It also excludes the case of start = 0.
        if (excess > excess_t(start * 64)) {
            return false;
        }
        // In the case where `find_open_in_word` does not find the result:
        assert(excess >= 0);

        for (uint64_t sub_block_offset = start - 1; sub_block_offset + 1 > 0; --sub_block_offset) {
            assert(excess > 0);
            uint64_t sub_block = block_offset + sub_block_offset;
            uint64_t word = m_bits_[sub_block];
            uint64_t byte_counts = util::byte_counts(word);
            if (excess <= 64) {
                if (find_open_in_word(word, byte_counts, excess, ret)) {
                    ret += sub_block * 64;
                    return true;
                }
            }
            excess -= static_cast<excess_t>(2 * util::bytes_sum(byte_counts) - 64);
        }
        return false;
    }

    // `direction`: backward when direction = 0.
    // `block`: block index
    // `excess`: the excess in bit index range [0, pos)
    template <int direction>
    inline bool BpVector::search_block_in_superblock(
            uint64_t block, excess_t excess, size_t& found_block) const {
        size_t superblock = block / superblock_size;
        excess_t superblock_excess = get_block_excess(superblock * superblock_size);
        if (direction) {
            for (size_t cur_block = block;
                 cur_block < std::min((superblock + 1) * superblock_size, (size_t)m_block_excess_min_.size());
                 ++cur_block) {
                if (excess >= superblock_excess + m_block_excess_min_[cur_block]) {
                    found_block = cur_block;
                    return true;
                }
            }
        } else {
            for (size_t cur_block = block;
                 cur_block + 1 >= (superblock * superblock_size) + 1;
                 --cur_block) {
                if (excess >= superblock_excess + m_block_excess_min_[cur_block]) {
                    found_block = cur_block;
                    return true;
                }
            }
        }

        return false;
    }

    inline bool BpVector::in_node_range(uint64_t node, excess_t excess) const {
        assert(m_superblock_excess_min_[node] != excess_t(size()));
        return excess >= m_superblock_excess_min_[node];
    }

    template <int direction>
    inline uint64_t BpVector::search_min_tree(uint64_t block, excess_t excess) const {
        size_t found_block = -1U;
        if (search_block_in_superblock<direction>(block, excess, found_block)) {
            return found_block;
        }

        // climbing `m_superblock_excess_min_` binary tree for searching a wide node range
        size_t cur_superblock = block / superblock_size;
        size_t cur_node = m_internal_nodes_ + cur_superblock;
        while (true) {
            assert(cur_node);
            // left sub-tree or right sub-tree
            bool going_back = (cur_node & 1) == direction;
            if (!going_back) {
                // direction = 0 and cur_node is right sub-node or
                // direction = 1 and cur_node is left sub-node
                size_t next_node = direction ? (cur_node + 1) : (cur_node - 1);
                // For `direction` = 0, left node is checked;
                // while `direction` = 1, we check right node.
                if (in_node_range(next_node, excess)) {
                    cur_node = next_node;
                    break;
                }
            }
            cur_node /= 2;
        }

        // `cur_node` represents a superblock index range.
        // In this range, we can find a superblock contains Open/Close Parentheses we want.
        assert(cur_node);

        // sliding down `m_superblock_excess_min_` binary tree for searching the superblock.
        while (cur_node < m_internal_nodes_) {
            uint64_t next_node = cur_node * 2 + (1 - direction);
            if (in_node_range(next_node, excess)) {
                cur_node = next_node;
                continue;
            }

            next_node = direction ? (next_node + 1) : (next_node - 1);
            // if it is not one child, it must be the other
            assert(in_node_range(next_node, excess));
            cur_node = next_node;
        }

        size_t next_superblock = cur_node - m_internal_nodes_;
        bool ret = search_block_in_superblock<direction>(next_superblock * superblock_size + (1 - direction) * (superblock_size - 1),
                                                         excess, found_block);
        assert(ret); (void)ret;

        return found_block;
    }

    // `pos`: bit index
    uint64_t BpVector::find_open(uint64_t pos) const {
        assert(pos);
        uint64_t ret = -1U;
        // Search in current word
        uint64_t word_pos = (pos / 64);
        uint64_t len = pos % 64;
        // Rest is padded with "close"
        uint64_t shifted_word = -(len != 0) & (m_bits_[word_pos] << (64 - len));
        uint64_t byte_counts = util::byte_counts(shifted_word);

        excess_t word_exc = 1;
        if (find_open_in_word(shifted_word, byte_counts, word_exc, ret)) {
            ret += pos - 64;
            return ret;
        }

        // Otherwise search in the local block
        uint64_t block = word_pos / bp_block_size;
        uint64_t block_offset = block * bp_block_size;
        uint64_t sub_block = word_pos % bp_block_size;
        uint64_t local_rank = util::bytes_sum(byte_counts); // no need to subtract the padding
        excess_t local_excess = -static_cast<excess_t>((2 * local_rank) - len);
        if (find_open_in_block(block_offset, local_excess + 1, sub_block, ret)) {
            return ret;
        }

        // Otherwise, find the first appropriate block
        excess_t pos_excess = excess(pos) - 1;
        uint64_t found_block = search_min_tree<0>(block - 1, pos_excess);
        uint64_t found_block_offset = found_block * bp_block_size;
        // Since search is backwards, have to add the current block
        excess_t found_block_excess = get_block_excess(found_block + 1);

        // Search in the found block
        bool found = find_open_in_block(found_block_offset, found_block_excess - pos_excess, bp_block_size, ret);
        assert(found); (void)found;
        return ret;
    }

    bool find_close_in_word(
            uint64_t word, uint64_t byte_counts,
            BpVector::excess_t cur_exc, uint64_t& ret) {
        assert(cur_exc > 0 && cur_exc <= 64);
        const uint64_t cum_exc_step_8 =
                (uint64_t(cur_exc) + ((2 * byte_counts - 8 * util::BYTE_UNIT) << 8)) * util::BYTE_UNIT;

        uint64_t min_exc_step_8 = 0;
        for (size_t i = 0; i < 8; ++i) {
            size_t shift = i * 8;
            min_exc_step_8 |= ((uint64_t)(tables.m_fwd_min[(word >> shift) & 0xFF])) << shift;
        }

        const uint64_t has_result = util::leq_bytes(cum_exc_step_8, min_exc_step_8);

        unsigned long shift;
        if (util::lsb(has_result, shift)) {
            uint8_t bit_pos = tables.m_fwd_pos[(word >> shift) & 0xFF][(cum_exc_step_8 >> shift) & 0xFF];
            assert(bit_pos > 0);
            ret = shift + bit_pos - 1;
            return true;
        }
        return false;
    }

    inline bool BpVector::find_close_in_block(
            uint64_t block_offset, BpVector::excess_t excess,
            uint64_t start, uint64_t& ret) const {
        if (excess > excess_t((bp_block_size - start) * 64)) {
            return false;
        }
        assert(excess > 0);
        for (uint64_t sub_block_offset = start; sub_block_offset < bp_block_size; ++sub_block_offset) {
            uint64_t sub_block = block_offset + sub_block_offset;
            uint64_t word = m_bits_[sub_block];
            uint64_t byte_counts = util::byte_counts(word);
            assert(excess > 0);
            if (excess <= 64) {
                if (find_close_in_word(word, byte_counts, excess, ret)) {
                    ret += sub_block * 64;
                    return true;
                }
            }
            excess += static_cast<excess_t>(2 * util::bytes_sum(byte_counts) - 64);
        }
        return false;
    }

    uint64_t BpVector::find_close(uint64_t pos) const {
        assert((*this)[pos]); // check there is an opening parenthesis in pos
        uint64_t ret = -1U;
        // Search in current word
        uint64_t word_pos = (pos + 1) / 64;
        uint64_t shift = (pos + 1) % 64;
        uint64_t shifted_word = m_bits_[word_pos] >> shift;
        // Pad with "open"
        uint64_t padded_word = shifted_word | (-(shift != 0) & (~0ULL << (64 - shift)));
        uint64_t byte_counts = util::byte_counts(padded_word);

        excess_t word_exc = 1;
        if (find_close_in_word(padded_word, byte_counts, word_exc, ret)) {
            ret += pos + 1;
            return ret;
        }

        // Otherwise search in the local block
        uint64_t block = word_pos / bp_block_size;
        uint64_t block_offset = block * bp_block_size;
        uint64_t sub_block = word_pos % bp_block_size;
        uint64_t local_rank = util::bytes_sum(byte_counts) - shift; // subtract back the padding
        excess_t local_excess = static_cast<excess_t>((2 * local_rank) - (64 - shift));
        if (find_close_in_block(block_offset, local_excess + 1, sub_block + 1, ret)) {
            return ret;
        }

        // Otherwise, find the first appropriate block
        excess_t pos_excess = excess(pos);
        uint64_t found_block = search_min_tree<1>(block + 1, pos_excess);
        uint64_t found_block_offset = found_block * bp_block_size;
        excess_t found_block_excess = get_block_excess(found_block);

        // Search in the found block
        bool found = find_close_in_block(found_block_offset, found_block_excess - pos_excess, 0, ret);
        assert(found); (void)found;
        return ret;
    }

    // `word_start`: bit index
    // search in byte step.
    inline void
    excess_rmq_in_word(uint64_t word, BpVector::excess_t& exc, uint64_t word_start,
                       BpVector::excess_t& min_exc, uint64_t& min_exc_idx) {
        BpVector::excess_t min_byte_exc = min_exc;
        uint64_t min_byte_idx = 0;

        for (size_t i = 0; i < 8; ++i) {
            size_t shift = i * 8;
            size_t byte = (word >> shift) & 0xFF;
            BpVector::excess_t cur_min = exc - tables.m_fwd_min[byte];

            min_byte_idx = (cur_min < min_byte_exc) ? i : min_byte_idx;
            min_byte_exc = (cur_min < min_byte_exc) ? cur_min : min_byte_exc;

            exc += tables.m_fwd_exc[byte];
        }

        if (min_byte_exc < min_exc) {
            min_exc = min_byte_exc;
            uint64_t shift = min_byte_idx * 8;
            min_exc_idx = word_start + shift + tables.m_fwd_min_idx[(word >> shift) & 0xFF];
        }
    }

    // [start, end), search in word step.
    void BpVector::excess_rmq_in_block(uint64_t start, uint64_t end,
                             BpVector::excess_t& exc,
                             BpVector::excess_t& min_exc,
                             uint64_t& min_exc_idx) const {
        assert(start <= end);
        if (start == end) return;

        assert((start / bp_block_size) == ((end - 1) / bp_block_size));
        for (size_t w = start; w < end; ++w) {
            excess_rmq_in_word(m_bits_[w], exc, w * 64,
                               min_exc, min_exc_idx);
        }
    }

    // [block_start, block_end), search in block step.
    void BpVector::excess_rmq_in_superblock(
            uint64_t block_start, uint64_t block_end,
            BpVector::excess_t& block_min_exc,
            uint64_t& block_min_idx) const
    {
        assert(block_start <= block_end);
        if (block_start == block_end) return;

        uint64_t superblock = block_start / superblock_size;

        assert(superblock == ((block_end - 1) / superblock_size));
        excess_t superblock_excess = get_block_excess(superblock * superblock_size);

        for (uint64_t block = block_start; block < block_end; ++block) {
            if (superblock_excess + m_block_excess_min_[block] < block_min_exc) {
                block_min_exc = superblock_excess + m_block_excess_min_[block];
                block_min_idx = block;
            }
        }
    }

    // RMQ Algorithm
    // [superblock_start, superblock_end)
    void BpVector::find_min_superblock(uint64_t superblock_start, uint64_t superblock_end,
                                   BpVector::excess_t& superblock_min_exc,
                                   uint64_t& superblock_min_idx) const {
        if (superblock_start == superblock_end) return;

        uint64_t cur_node = m_internal_nodes_ + superblock_start;
        uint64_t rightmost_span = superblock_start;

        excess_t node_min_exc = m_superblock_excess_min_[cur_node];
        uint64_t node_min_idx = cur_node;

        // code below assumes that there is at least one right-turn in
        // the node-root-node path, so we must handle this case
        // separately
        if (superblock_end - superblock_start == 1) {
            superblock_min_exc = node_min_exc;
            superblock_min_idx = superblock_start;
            return;
        }

        // go up the tree until we find the lowest node that spans the
        // whole superblock range
        size_t h = 0;
        while (true) {
            assert(cur_node);
            if ((cur_node & 1) == 0) { // is a left child
                // add right subtree to candidate superblocks
                uint64_t right_sibling = cur_node + 1;
                rightmost_span += uint64_t(1) << h;
                if (rightmost_span < superblock_end &&
                    m_superblock_excess_min_[right_sibling] < node_min_exc) {
                    node_min_exc = m_superblock_excess_min_[right_sibling];
                    node_min_idx = right_sibling;
                }
                if (rightmost_span >= superblock_end - 1) {
                    cur_node += 1;
                    break;
                }
            }
            cur_node /= 2; // parent
            h += 1;
        }

        assert(cur_node);

        // go down until we reach superblock_end
        while (rightmost_span > superblock_end - 1) {
            assert(cur_node < m_superblock_excess_min_.size());
            assert(h > 0);
            h -= 1;
            uint64_t left_child = cur_node * 2;
            uint64_t right_child_span = uint64_t(1) << h;
            if ((rightmost_span - right_child_span) >= (superblock_end - 1)) {
                // go to left child
                rightmost_span -= right_child_span;
                cur_node = left_child;
            } else {
                // go to right child and add left subtree to candidate
                // subblocks
                if (m_superblock_excess_min_[left_child] < node_min_exc) {
                    node_min_exc = m_superblock_excess_min_[left_child];
                    node_min_idx = left_child;
                }
                cur_node = left_child + 1;
            }
        }

        // check last left-turn
        if (rightmost_span < superblock_end &&
            m_superblock_excess_min_[cur_node] < node_min_exc) {
            node_min_exc = m_superblock_excess_min_[cur_node];
            node_min_idx = cur_node;
        }

        assert(rightmost_span == superblock_end - 1);

        // now reach the minimum leaf in the found subtree (cur_node),
        // which is entirely contained in the range
        if (node_min_exc < superblock_min_exc) {
            cur_node = node_min_idx;
            while (cur_node < m_internal_nodes_) {
                cur_node *= 2;
                // remember that past-the-end nodes are filled with size()
                if (m_superblock_excess_min_[cur_node + 1] <
                    m_superblock_excess_min_[cur_node]) {
                    cur_node += 1;
                }
            }

            assert(m_superblock_excess_min_[cur_node] == node_min_exc);
            superblock_min_exc = node_min_exc;
            superblock_min_idx = cur_node - m_internal_nodes_;

            assert(superblock_min_idx >= superblock_start);
            assert(superblock_min_idx < superblock_end);
        }
    }


    // `a`/`b`: bit index
    // return range [0, pos) where excess is minimum in [a, b).
    uint64_t BpVector::excess_rmq(uint64_t a, uint64_t b, excess_t& min_exc) const {
        assert(a <= b);

        excess_t cur_exc = excess(a);
        min_exc = cur_exc;
        uint64_t min_exc_idx = a;

        if (a == b) {
            return min_exc_idx;
        }

        uint64_t range_len = b - a;

        uint64_t word_a_idx = a / 64;
        uint64_t word_b_idx = (b - 1) / 64;

        // search in word_a
        uint64_t shift_a = a % 64;
        uint64_t shifted_word_a = m_bits_[word_a_idx] >> shift_a;
        uint64_t subword_len_a = std::min(64 - shift_a, range_len);

        // 0 bits should be padded because "0" is ")".
        uint64_t padded_word_a =
                (subword_len_a == 64)
                ? shifted_word_a
                : (shifted_word_a | (~0ULL << subword_len_a));

        excess_rmq_in_word(padded_word_a, cur_exc, a,
                           min_exc, min_exc_idx);

        if (word_a_idx == word_b_idx) {
            // single word
            return min_exc_idx;
        }

        uint64_t block_a = word_a_idx / bp_block_size;
        uint64_t block_b = word_b_idx / bp_block_size;

        cur_exc -= 64 - excess_t(subword_len_a); // remove padding

        // After this if-statement start & end will be in the same word.
        if (block_a == block_b) {
            // same block
            excess_rmq_in_block(word_a_idx + 1, word_b_idx,
                                cur_exc, min_exc, min_exc_idx);

        } else {
            // search in partial block of word_a
            excess_rmq_in_block(word_a_idx + 1, (block_a + 1) * bp_block_size,
                                cur_exc, min_exc, min_exc_idx);

            // search in blocks
            excess_t block_min_exc = min_exc;
            uint64_t block_min_idx = -1U;

            uint64_t superblock_a = (block_a + 1) / superblock_size;
            uint64_t superblock_b = block_b / superblock_size;

            // After this if-statement start & end will be in the same block.
            if (superblock_a == superblock_b) {
                // same superblock
                excess_rmq_in_superblock(block_a + 1, block_b,
                                         block_min_exc, block_min_idx);
            } else {
                // partial superblock of a
                excess_rmq_in_superblock(block_a + 1,
                                         (superblock_a + 1) * superblock_size,
                                         block_min_exc,
                                         block_min_idx);

                // search min superblock in the min tree
                excess_t superblock_min_exc = min_exc;
                uint64_t superblock_min_idx = -1U;
                find_min_superblock(superblock_a + 1, superblock_b,
                                    superblock_min_exc, superblock_min_idx);

                if (superblock_min_exc < min_exc) {
                    excess_rmq_in_superblock(superblock_min_idx * superblock_size,
                                             (superblock_min_idx + 1) * superblock_size,
                                             block_min_exc,
                                             block_min_idx);
                }

                // partial superblock of b
                excess_rmq_in_superblock(superblock_b * superblock_size,
                                         block_b,
                                         block_min_exc,
                                         block_min_idx);
            }

            if (block_min_exc < min_exc) {
                cur_exc = get_block_excess(block_min_idx);
                excess_rmq_in_block(block_min_idx * bp_block_size,
                                    (block_min_idx + 1) * bp_block_size,
                                    cur_exc, min_exc, min_exc_idx);
                assert(min_exc == block_min_exc);
            }

            // search in partial block of word_b
            cur_exc = get_block_excess(block_b);
            excess_rmq_in_block(block_b * bp_block_size, word_b_idx,
                                cur_exc, min_exc, min_exc_idx);
        }

        // search in word_b
        uint64_t word_b = m_bits_[word_b_idx];
        uint64_t offset_b = b % 64;
        uint64_t padded_word_b =
                (offset_b == 0)
                ? word_b
                : (word_b | (~0ULL << offset_b));

        excess_rmq_in_word(padded_word_b, cur_exc, word_b_idx * 64,
                           min_exc, min_exc_idx);

        assert(min_exc_idx >= a);
        assert(min_exc == excess(min_exc_idx));

        return min_exc_idx;
    }

    void BpVector::build_min_tree() {
        if (!size()) return;

        std::vector<block_min_excess_t> block_excess_min;
        excess_t cur_block_min = 0, cur_superblock_excess = 0;
        for (uint64_t sub_block = 0; sub_block < m_bits_.size(); ++sub_block) {
            if (sub_block % bp_block_size == 0) {
                if (sub_block % (bp_block_size * superblock_size) == 0) {
                    cur_superblock_excess = 0;
                }
                if (sub_block) {
                    assert(cur_block_min >= std::numeric_limits<block_min_excess_t>::min());
                    assert(cur_block_min <= std::numeric_limits<block_min_excess_t>::max());
                    block_excess_min.push_back((block_min_excess_t)cur_block_min);
                    cur_block_min = cur_superblock_excess;
                }
            }
            uint64_t word = m_bits_[sub_block];
            uint64_t mask = 1ULL;
            // for last block stop at bit boundary
            uint64_t n_bits =
                    (sub_block == m_bits_.size() - 1 && size() % 64)
                    ? size() % 64
                    : 64;

            for (uint64_t i = 0; i < n_bits; ++i) {
                cur_superblock_excess += (word & mask) ? 1 : -1;
                cur_block_min = std::min(cur_block_min, cur_superblock_excess);
                mask <<= 1;
            }
        }
        // Flush last block mins
        assert(cur_block_min >= std::numeric_limits<block_min_excess_t>::min());
        assert(cur_block_min <= std::numeric_limits<block_min_excess_t>::max());
        block_excess_min.push_back((block_min_excess_t)cur_block_min);

        size_t n_blocks = (m_bits_.size() + bp_block_size - 1) / bp_block_size;
        assert(n_blocks == block_excess_min.size());

        size_t n_superblocks = (n_blocks + superblock_size - 1) / superblock_size;

        size_t n_complete_leaves = 1;
        // TODO: use util::msb to calc `n_complete_leaves`.
        while (n_complete_leaves < n_superblocks) n_complete_leaves <<= 1;
        m_internal_nodes_ = n_complete_leaves;
        size_t treesize = m_internal_nodes_ + n_superblocks;

        std::vector<excess_t> superblock_excess_min(treesize);

        // Fill in the leaves of the tree
        for (size_t superblock = 0; superblock < n_superblocks; ++superblock) {
            excess_t cur_super_min = static_cast<excess_t>(size());
            excess_t superblock_excess = get_block_excess(superblock * superblock_size);

            for (size_t block = superblock * superblock_size;
                 block < std::min((superblock + 1) * superblock_size, n_blocks);
                 ++block) {
                cur_super_min = std::min(cur_super_min, superblock_excess + block_excess_min[block]);
            }
            assert(cur_super_min >= 0 && cur_super_min < excess_t(size()));

            superblock_excess_min[m_internal_nodes_ + superblock] = cur_super_min;
        }

        // fill in the internal nodes with past-the-boundary values
        // (they will also serve as sentinels in debug)
        for (size_t node = 0; node < m_internal_nodes_; ++node) {
            superblock_excess_min[node] = static_cast<excess_t>(size());
        }

        // Fill bottom-up the other layers: each node updates the parent
        for (size_t node = treesize - 1; node > 1; --node) {
            size_t parent = node / 2;
            superblock_excess_min[parent] = std::min(superblock_excess_min[parent],
                                                     superblock_excess_min[node]);
        }

        m_block_excess_min_.steal(block_excess_min);
        m_superblock_excess_min_.steal(superblock_excess_min);
    }
}
}