//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "rocksdb/filter_policy.h"

#include "util/coding.h"
#include "utilities/pdt/default_tree_builder.h"
#include "utilities/pdt/path_decomposed_trie.h"

namespace rocksdb {
// An implementation of filter policy
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
        
        for (size_t i = 0; i < static_cast<size_t>(n); i++) {
            std::vector<uint8_t> bytes(keys[i].data(), keys[i].data() + keys[i].size());
            trieBuilder.append(bytes);
        }
        trieBuilder.finish();
        succinct::trie::DefaultPathDecomposedTrie<Lexicographic> pdt(trieBuilder);
        
        auto& labels = pdt.get_labels();
        assert(labels.size() < std::numeric_limits<uint32_t>::max());
        PutVarint32(dst, static_cast<uint32_t>(labels.size()));

        size_t cur_size = dst->size();
        size_t byte_size = labels.size() * sizeof(uint16_t);
        dst->resize(cur_size + byte_size, 0);
        char* array = &(*dst)[cur_size];
        for (auto label : labels) {
            EncodeFixed16(array, label);
            array += 2;   
        }

        auto& branches = pdt.get_branches();
        assert(branches.size() < std::numeric_limits<uint32_t>::max());
        PutVarint32(dst, static_cast<uint32_t>(branches.size()));

        cur_size = dst->size();
        byte_size = branches.size() * sizeof(uint8_t);
        dst->resize(cur_size + byte_size, 0);
        array = &(*dst)[cur_size];
        for (auto branch : branches) {
            *array = branch;
            array++;
        }

        auto& bp = pdt.get_bp().data();
        PutVarint64(dst, bp.size());
        for (size_t i = 0; i < static_cast<size_t>(bp.size()); i++) {
            PutVarint64(dst, bp[i]);
        }

        auto& word_pos = pdt.get_word_pos();
        PutVarint64(dst, static_cast<uint64_t>(word_pos.size()));
        for (auto pos : word_pos) {
            PutVarint64(dst, static_cast<uint64_t>(pos));
        }
    }

    bool KeyMayMatch(const Slice& key, const Slice& bloom_filter) const override {
        const size_t len = bloom_filter.size();
        const char* array = bloom_filter.data();

        
    }
private:
    const bool use_block_based_builder_;
};
}