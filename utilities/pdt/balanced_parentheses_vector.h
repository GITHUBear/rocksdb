//
// Created by Dim Dew on 2020-07-23.
//

#ifndef PATH_DECOMPOSITION_TRIE_BALANCED_PARENTHESES_VECTOR_H
#define PATH_DECOMPOSITION_TRIE_BALANCED_PARENTHESES_VECTOR_H

#include "rank_select_bit_vector.h"

namespace rocksdb {
namespace succinct {
    //
    // In a sequence of n balanced parentheses (BP), each open parenthesis
    // "(" can be associated to its mate ")". Operations FindClose and FindOpen can be defined,
    // which find the position of the mate of respectively an open or close parenthesis.
    // The sequence can be represented as a bit vector, where 1 represents "(" and 0 represents ")"
    //
    // In BpVector, 64bits are organized logically in the following format:
    //
    //                                              +---- block_min_excess(calculated at the beginning of block)
    //                                              |
    //                                              V
    //           +-----+-----+-----+-----+          +-----+-----+-----+-----+
    //           |     |     |     |     |    ...   |     |     |     |     |   ...
    //           +-----+-----+-----+-----+          +-----+-----+-----+-----+
    //           |<-   bp_block_size   ->|          |<-   bp_block_size   ->|
    //           |<-                  superblock_size                     ->|
    //
    // `m_block_excess_min_` and `m_superblock_excess_min_` maintain minimum excess
    // (the number of mismatched "(") in different logical level:
    //
    //                                 4superblock
    //                                 excess
    //                                 min
    //                          /                        
    //                         /                          
    //                        /                            
    //                       /                              
    //
    //                2superblock                         2superblock
    //                excess                              excess
    //                min                                 min
    //             /                \                 /                 
    //            /                  \               /                   
    //         1superblock      1superblock       1superblock       1superblock
    //         excess           excess            excess            excess
    //         min              min               min               min
    //     +--------------+  +--------------+  +--------------+  +--------------+
    //     |superblock    |  |superblock    |  |superblock    |  |superblock    |
    //     | +---+   +---+|  | +---+   +---+|  | +---+   +---+|  | +---+   +---+|
    //     | |blk|...|blk||  | |blk|...|blk||  | |blk|...|blk||  | |blk|...|blk||  ...
    //     | +---+   +---+|  | +---+   +---+|  | +---+   +---+|  | +---+   +---+|
    //     +--------------+  +--------------+  +--------------+  +--------------+
    //
    // `m_superblock_excess_min_` is used in RMQ algorithm.
    //
    class BpVector : public RsBitVector {
    public:
        BpVector() : RsBitVector() {}

        BpVector(const std::vector<bool>& bools,
                 bool with_select_hints = false,
                 bool with_select0_hints = false)
                 : RsBitVector(bools, with_select_hints, with_select0_hints) {
            build_min_tree();
        }

        BpVector(BitVectorBuilder* builder,
                 bool with_select_hints = false,
                 bool with_select0_hints = false)
                 : RsBitVector(builder, with_select_hints, with_select0_hints) {
            build_min_tree();
        }
        
        BpVector(const uint64_t* raw_data,
                 uint64_t word_size,
                 size_t bit_size,
                 bool with_select_hints = false,
                 bool with_select0_hints = false)
                 : RsBitVector(raw_data, word_size, bit_size, with_select_hints, with_select0_hints) {
            build_min_tree();
        }

        void swap(BpVector& other) {
            RsBitVector::swap(other);
            std::swap(m_internal_nodes_, other.m_internal_nodes_);
            m_block_excess_min_.swap(other.m_block_excess_min_);
            m_superblock_excess_min_.swap(other.m_superblock_excess_min_);
        }

        uint64_t find_open(uint64_t pos) const;

        uint64_t find_close(uint64_t pos) const;

        typedef int32_t excess_t;

        excess_t excess(uint64_t pos) const {
            return static_cast<excess_t>(2 * rank(pos) - pos);
        }

        uint64_t excess_rmq(uint64_t a, uint64_t b, excess_t& min_exc) const;

        inline uint64_t excess_rmq(uint64_t a, uint64_t b) const {
            excess_t foo;
            return excess_rmq(a, b, foo);
        }

    protected:
        static const size_t bp_block_size = 4; // to increase confusion, bp block_size is not necessarily rs_bit_vector block_size
        static const size_t superblock_size = 32; // number of blocks in superblock

        typedef int16_t block_min_excess_t;

        // return true if we can find matched "(" in block, the position is returned by `ret`
        // `ret` is the bit index relative to `m_bits_`.
        //
        // `pos`: word index at the boundary of block
        // `excess`: the number of ")" that is mismatched
        // `max_sub_blocks`: sub-index in block
        bool find_open_in_block(uint64_t pos, excess_t excess,
                                uint64_t max_sub_blocks, uint64_t& ret) const;

        bool find_close_in_block(uint64_t pos, excess_t excess,
                                 uint64_t max_sub_blocks, uint64_t& ret) const;

        void excess_rmq_in_block(uint64_t start, uint64_t end,
                                 BpVector::excess_t& exc,
                                 BpVector::excess_t& min_exc,
                                 uint64_t& min_exc_idx) const;

        void excess_rmq_in_superblock(uint64_t block_start, uint64_t block_end,
                                      BpVector::excess_t& block_min_exc,
                                      uint64_t& block_min_idx) const;

        void find_min_superblock(uint64_t superblock_start, uint64_t superblock_end,
                                 BpVector::excess_t& superblock_min_exc,
                                 uint64_t& superblock_min_idx) const;

        // accumulate the excess in the block index range [0, block)
        // `block`: block index
        inline excess_t get_block_excess(uint64_t block) const {
            uint64_t word_idx = block * bp_block_size;
            uint64_t block_pos = word_idx * 64;
            // word_rank(word_idx) - (block_pos - word_rank(word_idx))
            excess_t excess = static_cast<excess_t>(2 * word_rank(word_idx) - block_pos);
            assert(excess >= 0);
            return excess;
        }

        inline bool in_node_range(uint64_t node, excess_t excess) const;

        template <int direction>
        inline bool search_block_in_superblock(
                uint64_t block, excess_t excess, size_t& found_block) const;

        template <int direction>
        inline uint64_t search_min_tree(uint64_t block, excess_t excess) const;

        void build_min_tree();

        // In fact, `m_internal_nodes` is not the number of internal nodes
        // in `m_superblock_excess_min_`. Actually it's the offset of leaf nodes
        // in `m_superblock_excess_min_`.
        uint64_t m_internal_nodes_;
        mappable_vector<block_min_excess_t> m_block_excess_min_;
        // Exactly, `m_superblock_excess_min_` is a binary tree stored in vector.
        mappable_vector<excess_t> m_superblock_excess_min_;

    };

    // return true if we can find matched "(" in word, the position is returned by `ret`
    // `ret` is the bit index relative to `word`.
    //
    // `word`: 64-length bit sequence
    // `byte_counts`: the byte count of `word`
    // `cur_exc`: the num of ")" we want to match
    // `ret`: position of "("
    bool find_open_in_word(
            uint64_t word, uint64_t byte_counts,
            BpVector::excess_t cur_exc, uint64_t& ret);

    bool find_close_in_word(
            uint64_t word, uint64_t byte_counts,
            BpVector::excess_t cur_exc, uint64_t& ret);

    void excess_rmq_in_word(uint64_t word, BpVector::excess_t& exc, uint64_t word_start,
                       BpVector::excess_t& min_exc, uint64_t& min_exc_idx);
}
}
#endif //PATH_DECOMPOSITION_TRIE_BALANCED_PARENTHESES_VECTOR_H
