//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "rocksdb/filter_policy.h"

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
        succinct::DefaultTreeBuilder<true> pdt_builder;
        succinct::trie::compacted_trie_builder
                <succinct::DefaultTreeBuilder<true>>
                trieBuilder(pdt_builder);
        
        for (size_t i = 0; i < static_cast<size_t>(n); i++) {
            std::vector<uint8_t> bytes(keys[i].data(), keys[i].data() + keys[i].size());
            trieBuilder.append(bytes);
        }
        trieBuilder.finish();

    }
private:
    const bool use_block_based_builder_;
};
}