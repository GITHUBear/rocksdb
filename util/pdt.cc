//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#include "include/rocksdb/filter_policy.h"
#include "port/port.h"
#include "util/coding.h"
#include "utilities/pdt/default_tree_builder.h"
#include "utilities/pdt/path_decomposed_trie.h"

namespace rocksdb {
// An implementation of filter policy
namespace {
template<bool Lexicographic = false>
class PdtFilterPolicy : public FilterPolicy {
public:
    explicit PdtFilterPolicy(bool use_block_based_builder)
        : use_block_based_builder_(use_block_based_builder){}

    ~PdtFilterPolicy() override {}

    const char* Name() const override { return "rocksdb.PdtFilter"; } 

    void CreateFilter(const Slice* keys, int n, std::string* dst) const override {
        succinct::DefaultTreeBuilder<Lexicographic> pdt_builder;
        succinct::trie::compacted_trie_builder
                <succinct::DefaultTreeBuilder<Lexicographic>>
                trieBuilder(pdt_builder);
        
        std::vector<Slice> sorted_slices;
        for (size_t i = 0; i < static_cast<size_t>(n); i++) {
            sorted_slices.push_back(keys[i]);
        }
        std::sort(sorted_slices.begin(), sorted_slices.end(), [] (const Slice& s1, const Slice& s2) {
            return s1.compare(s2) < 0;
        });

        // for (size_t i = 0; i + 1 < sorted_slices.size(); i++) {
        //     int res = sorted_slices[i].compare(sorted_slices[i + 1]);
        //     assert(res < 0);
        // }

        for (size_t i = 0; i < sorted_slices.size(); i++) {
            std::vector<uint8_t> bytes(sorted_slices[i].data(), 
                                       sorted_slices[i].data() + sorted_slices[i].size());
            trieBuilder.append(bytes);
        }
        trieBuilder.finish();
        succinct::trie::DefaultPathDecomposedTrie<Lexicographic> pdt(trieBuilder);
        
        auto& labels = pdt.get_labels();
        auto& branches = pdt.get_branches();
        auto& bp = pdt.get_bp().data();
        auto& word_pos = pdt.get_word_pos();

        PutVarint64(dst, static_cast<uint64_t>(pdt.get_bp().size())); // valid bits size of bp
        PutVarint64(dst, labels.size() * sizeof(uint16_t));           // label byte size
        PutVarint64(dst, branches.size() * sizeof(uint16_t));         // branch byte size
        PutVarint64(dst, bp.size() * sizeof(uint64_t));               // bp byte size
        PutVarint64(dst, word_pos.size() * sizeof(uint64_t));         // word_pos byte size

        for (size_t i = 0; i < static_cast<size_t>(labels.size()); i++) {
            PutFixed16(dst, labels[i]);
        }
        for (size_t i = 0; i < static_cast<size_t>(branches.size()); i++) {
            PutFixed16(dst, branches[i]);
        }
        // bp encoding is not portable now for the sake of performance.
        for (size_t i = 0; i < static_cast<size_t>(bp.size()); i++) {
            dst->append(reinterpret_cast<const char*>(&bp[i]), sizeof(uint64_t));
        }
        for (size_t i = 0; i < static_cast<size_t>(word_pos.size()); i++) {
            PutFixed64(dst, word_pos[i]);
        }
    }

    bool KeyMayMatch(const Slice& key, const Slice& bloom_filter) const override {
        const size_t len = bloom_filter.size();
        const char* array = bloom_filter.data();

        Slice tmp(bloom_filter);
        uint64_t bp_bit_size, label_size, branch_size, bp_byte_size, pos_size;
        assert(GetVarint64(&tmp, &bp_bit_size));
        assert(GetVarint64(&tmp, &label_size));
        assert(GetVarint64(&tmp, &branch_size));
        assert(GetVarint64(&tmp, &bp_byte_size));
        assert(GetVarint64(&tmp, &pos_size));
        
        auto label_ptr = reinterpret_cast<const uint16_t*>(tmp.data());
        auto branch_ptr = reinterpret_cast<const uint16_t*>(tmp.data() + label_size);
        auto bp_ptr = reinterpret_cast<const uint64_t*>(
                            tmp.data() + label_size + branch_size);
        auto pos_ptr = reinterpret_cast<const uint64_t*>(
                            tmp.data() + label_size + branch_size + bp_byte_size);
        succinct::trie::DefaultPathDecomposedTrie<Lexicographic> pdt(
            label_ptr, label_size / sizeof(uint16_t),
            branch_ptr, branch_size / sizeof(uint16_t),
            bp_ptr, bp_byte_size / sizeof(uint64_t), bp_bit_size,
            pos_ptr, pos_size / sizeof(uint64_t), true
        );

        std::string s = key.ToString();
        int res = pdt.index(s);
        return res != -1;
    }

    FilterBitsBuilder* GetFilterBitsBuilder() const {
        return nullptr;
    }

    FilterBitsReader* GetFilterBitsReader(const Slice& /*contents*/) const {
        return nullptr;
    }
private:
    const bool use_block_based_builder_;
};
}

const FilterPolicy* NewLexPdtFilterPolicy(bool use_block_based_builer) {
    return new PdtFilterPolicy<true>(use_block_based_builer);
}

const FilterPolicy* NewCentriodPdtFilterPolicy(bool use_block_based_builder) {
    return new PdtFilterPolicy<false>(use_block_based_builder);
}
}