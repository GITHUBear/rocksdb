//
// Created by Dim Dew on 2020-08-11.
//

#include <gtest/gtest.h>
#include <string>
#include "compacted_trie_builder.h"
#include "default_tree_builder.h"

using namespace rocksdb;

std::vector<uint8_t> string_to_bytes(std::string s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

template <typename TrieBuilder>
void append_to_trie(
        succinct::trie::compacted_trie_builder<TrieBuilder>& builder,
        std::string s) {
    auto s1 = string_to_bytes(s);
    builder.append(s1);
}

template <typename TrieBuilder>
std::string get_label(typename TrieBuilder::representation_type& root) {
    std::string label;
    for (auto byte : root->m_labels) {
        if (byte >> 8 == 1) {
            label += std::to_string(uint8_t(byte));
        } else if (byte >> 8 == 2) {
            label += '#';
        } else {
            label += (char(byte) ? char(byte) : '$');
        }
    }
    return label;
}

template <typename TrieBuilder>
std::string get_bp_str(typename TrieBuilder::representation_type& root) {
    succinct::BitVector bitVec(&root->m_bp);
    succinct::BitVector::enumerator iter(bitVec, 0);
    size_t bp_idx = 0;
    std::string bp_str;
    while (bp_idx < bitVec.size()) {
        if (iter.next()) {
            bp_str += "(";
        } else {
            bp_str += ")";
        }
        bp_idx++;
    }
    return bp_str;
}

template <typename TrieBuilder>
std::string get_branch_str(typename TrieBuilder::representation_type& root) {
    std::string branch_str;
    for (auto byte : root->m_branches) {
        branch_str += (char)byte;
    }
    return branch_str;
}

TEST(TRIE_BUILD, PAPER_EXAMPLE_LEX) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "three");
    append_to_trie(trieBuilder, "trial");
    append_to_trie(trieBuilder, "triangle");
    append_to_trie(trieBuilder, "triangular");
    append_to_trie(trieBuilder, "trie");
    append_to_trie(trieBuilder, "triple");
    append_to_trie(trieBuilder, "triply");
    trieBuilder.finish();

    succinct::DefaultTreeBuilder<true>::representation_type root = trieBuilder.get_root();
    assert(get_label<succinct::DefaultTreeBuilder<true>>(root) == "t0hree$#i1a0l$#g0le$#lar$#$#l0e$#$#");
    assert(get_bp_str<succinct::DefaultTreeBuilder<true>>(root) == "(()((()()))())");
    assert(get_branch_str<succinct::DefaultTreeBuilder<true>>(root) == "rpenuy");
}

TEST(TRIE_BUILD, PAPER_EXAMPLE_CENTROID) {
    succinct::DefaultTreeBuilder<> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "three");
    append_to_trie(trieBuilder, "trial");
    append_to_trie(trieBuilder, "triangle");
    append_to_trie(trieBuilder, "triangular");
    append_to_trie(trieBuilder, "trie");
    append_to_trie(trieBuilder, "triple");
    append_to_trie(trieBuilder, "triply");
    trieBuilder.finish();

    succinct::DefaultTreeBuilder<>::representation_type root = trieBuilder.get_root();
    assert(get_label<succinct::DefaultTreeBuilder<>>(root) == "t0ri1a0ng0le$#lar$#$#$#l0e$#$#ree$#");
    assert(get_bp_str<succinct::DefaultTreeBuilder<>>(root) == "(((((())))()))");
    assert(get_branch_str<succinct::DefaultTreeBuilder<>>(root) == "hpeluy");
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}