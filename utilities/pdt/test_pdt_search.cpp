//
// Created by Dim Dew on 2020-10-09.
//
#include <gtest/gtest.h>
#include "path_decomposed_trie.h"

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

std::string get_label(const std::vector<uint16_t>& labels) {
    std::string label;
    for (auto byte : labels) {
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

std::string get_bp_str(const succinct::BpVector& bp) {
    size_t bp_idx = 0;
    std::string bp_str;
    while (bp_idx < bp.size()) {
        if (bp[bp_idx]) {
            bp_str += "(";
        } else {
            bp_str += ")";
        }
        bp_idx++;
    }
    return bp_str;
}

std::string get_branch_str(const std::vector<uint8_t>& branches) {
    std::string branch_str;
    for (auto byte : branches) {
        branch_str += (char)byte;
    }
    return branch_str;
}

TEST(PDT_TEST, CREATE_1) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "three");
    append_to_trie(trieBuilder, "trial");
    append_to_trie(trieBuilder, "triangle");
    append_to_trie(trieBuilder, "triangular");
    append_to_trie(trieBuilder, "triangulate");
    append_to_trie(trieBuilder, "triangulaus");
    append_to_trie(trieBuilder, "trie");
    append_to_trie(trieBuilder, "triple");
    append_to_trie(trieBuilder, "triply");
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);
    EXPECT_EQ(get_label(pdt.get_labels()), "t0hree#i1a0l#g0le#la1r#e#s##l0e##");
    EXPECT_EQ(get_branch_str(pdt.get_branches()), "rpenuuty");
    EXPECT_EQ(get_bp_str(pdt.get_bp()), "(()((()()(())))())");
    for (auto& pos : pdt.word_positions) {
        printf("%lu ", pos);
    }
    printf("\n");
}

TEST(PDT_TEST, SEARCH_UTIL_1) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "three");
    append_to_trie(trieBuilder, "trial");
    append_to_trie(trieBuilder, "triangle");
    append_to_trie(trieBuilder, "triangular");
    append_to_trie(trieBuilder, "triangulate");
    append_to_trie(trieBuilder, "triangulaus");
    append_to_trie(trieBuilder, "trie");
    append_to_trie(trieBuilder, "triple");
    append_to_trie(trieBuilder, "triply");
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);

    size_t end, num;
    pdt.get_branch_idx_by_node_idx(0, end, num);
    EXPECT_EQ(end, 0);
    EXPECT_EQ(num, 1);
    pdt.get_branch_idx_by_node_idx(1, end, num);
    EXPECT_EQ(end, 3);
    EXPECT_EQ(num, 3);
    pdt.get_branch_idx_by_node_idx(2, end, num);
    EXPECT_EQ(end, 4);
    EXPECT_EQ(num, 1);
    pdt.get_branch_idx_by_node_idx(3, end, num);
    EXPECT_EQ(end, 6);
    EXPECT_EQ(num, 2);
    pdt.get_branch_idx_by_node_idx(4, end, num);
    EXPECT_EQ(end, 6);
    EXPECT_EQ(num, 0);
    pdt.get_branch_idx_by_node_idx(5, end, num);
    EXPECT_EQ(end, 6);
    EXPECT_EQ(num, 0);
    pdt.get_branch_idx_by_node_idx(6, end, num);
    EXPECT_EQ(end, 6);
    EXPECT_EQ(num, 0);
    pdt.get_branch_idx_by_node_idx(7, end, num);
    EXPECT_EQ(end, 7);
    EXPECT_EQ(num, 1);
    pdt.get_branch_idx_by_node_idx(8, end, num);
    EXPECT_EQ(end, 7);
    EXPECT_EQ(num, 0);

    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(3), 7);
    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(4), 6);
    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(5), 2);

    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(1), 1);

    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(7), 3);

    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(9), 5);
    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(10), 4);

    EXPECT_EQ(pdt.get_node_idx_by_branch_idx(15), 8);

    size_t parent, branch_no;
    uint8_t branch;
    pdt.get_parent_node_branch_by_node_idx(7, parent, branch, branch_no);
    EXPECT_EQ(parent, 1);
    EXPECT_EQ(branch, static_cast<uint8_t>('p'));
    EXPECT_EQ(branch_no, 3);

    pdt.get_parent_node_branch_by_node_idx(2, parent, branch, branch_no);
    EXPECT_EQ(parent, 1);
    EXPECT_EQ(branch, static_cast<uint8_t>('n'));
    EXPECT_EQ(branch_no, 1);

    pdt.get_parent_node_branch_by_node_idx(5, parent, branch, branch_no);
    EXPECT_EQ(parent, 3);
    EXPECT_EQ(branch, static_cast<uint8_t>('u'));
    EXPECT_EQ(branch_no, 2);

    pdt.get_parent_node_branch_by_node_idx(1, parent, branch, branch_no);
    EXPECT_EQ(parent, 0);
    EXPECT_EQ(branch, static_cast<uint8_t>('r'));
    EXPECT_EQ(branch_no, 1);

    pdt.get_parent_node_branch_by_node_idx(6, parent, branch, branch_no);
    EXPECT_EQ(parent, 1);
    EXPECT_EQ(branch, static_cast<uint8_t>('e'));
    EXPECT_EQ(branch_no, 2);

    pdt.get_parent_node_branch_by_node_idx(4, parent, branch, branch_no);
    EXPECT_EQ(parent, 3);
    EXPECT_EQ(branch, static_cast<uint8_t>('t'));
    EXPECT_EQ(branch_no, 1);

    pdt.get_parent_node_branch_by_node_idx(8, parent, branch, branch_no);
    EXPECT_EQ(parent, 7);
    EXPECT_EQ(branch, static_cast<uint8_t>('y'));
    EXPECT_EQ(branch_no, 1);

    pdt.get_parent_node_branch_by_node_idx(3, parent, branch, branch_no);
    EXPECT_EQ(parent, 2);
    EXPECT_EQ(branch, static_cast<uint8_t>('u'));
    EXPECT_EQ(branch_no, 1);

    EXPECT_FALSE(pdt.get_parent_node_branch_by_node_idx(0, parent, branch, branch_no));
}

TEST(PDT_TEST, INDEX_1) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "three");
    append_to_trie(trieBuilder, "trial");
    append_to_trie(trieBuilder, "triangle");
    append_to_trie(trieBuilder, "triangular");
    append_to_trie(trieBuilder, "triangulate");
    append_to_trie(trieBuilder, "triangulaus");
    append_to_trie(trieBuilder, "trie");
    append_to_trie(trieBuilder, "triple");
    append_to_trie(trieBuilder, "triply");
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);
    std::string s("triple");
    EXPECT_EQ(pdt.index(s), 7);
    s = "three";
    EXPECT_EQ(pdt.index(s), 0);
    s = "triply";
    EXPECT_EQ(pdt.index(s), 8);
    s = "trie";
    EXPECT_EQ(pdt.index(s), 6);
    s = "triangular";
    EXPECT_EQ(pdt.index(s), 3);
    s = "trial";
    EXPECT_EQ(pdt.index(s), 1);
    s = "triangle";
    EXPECT_EQ(pdt.index(s), 2);
    s = "triangulaus";
    EXPECT_EQ(pdt.index(s), 5);
    s = "triangulate";
    EXPECT_EQ(pdt.index(s), 4);

    s = "tr";
    EXPECT_EQ(pdt.index(s), -1);
    s = "";
    EXPECT_EQ(pdt.index(s), -1);
    s = "triangulates";
    EXPECT_EQ(pdt.index(s), -1);
    s = "pikachu";
    EXPECT_EQ(pdt.index(s), -1);
    s = "trip";
    EXPECT_EQ(pdt.index(s), -1);
}

TEST(PDT_TEST, INDEX_2) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "pace");    // 0
    append_to_trie(trieBuilder, "package"); // 1
    append_to_trie(trieBuilder, "pacman");  // 2
    append_to_trie(trieBuilder, "pancake"); // 3
    append_to_trie(trieBuilder, "pea");     // 4
    append_to_trie(trieBuilder, "peek");    // 5
    append_to_trie(trieBuilder, "peel");    // 6
    append_to_trie(trieBuilder, "pikachu"); // 7
    append_to_trie(trieBuilder, "pod");     // 8
    append_to_trie(trieBuilder, "pokemon"); // 9
    append_to_trie(trieBuilder, "pool");    // 10
    append_to_trie(trieBuilder, "proof");   // 11
    append_to_trie(trieBuilder, "three");   // 12
    append_to_trie(trieBuilder, "trial");   // 13
    append_to_trie(trieBuilder, "triangle");// 14
    append_to_trie(trieBuilder, "triangular"); // 15
    append_to_trie(trieBuilder, "triangulate");// 16
    append_to_trie(trieBuilder, "triangulaus");// 17
    append_to_trie(trieBuilder, "trie");       // 18
    append_to_trie(trieBuilder, "triple");     // 19
    append_to_trie(trieBuilder, "triply");     // 20
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);

    printf("%s\n", get_label(pdt.get_labels()).c_str());
    printf("%s\n", get_bp_str(pdt.get_bp()).c_str());
    printf("%s\n", get_branch_str(pdt.get_branches()).c_str());
    std::string s("pokemon");
    EXPECT_EQ(pdt.index(s), 9);
    s = "pikachu";
    EXPECT_EQ(pdt.index(s), 7);
    s = "trie";
    EXPECT_EQ(pdt.index(s), 18);
    s = "pt";
    EXPECT_EQ(pdt.index(s), -1);
}

inline std::string ubyes2str(std::vector<uint8_t> ubyte) {
    return std::string(ubyte.begin(), ubyte.end());
}

TEST(PDT_TEST, OPERATOR_INDEX_1) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "three");      // 0
    append_to_trie(trieBuilder, "trial");      // 1
    append_to_trie(trieBuilder, "triangle");   // 2
    append_to_trie(trieBuilder, "triangular"); // 3
    append_to_trie(trieBuilder, "triangulate");// 4
    append_to_trie(trieBuilder, "triangulaus");// 5
    append_to_trie(trieBuilder, "trie");       // 6
    append_to_trie(trieBuilder, "triple");     // 7
    append_to_trie(trieBuilder, "triply");     // 8
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);

    EXPECT_EQ(ubyes2str(pdt[7]), "triple");
    EXPECT_EQ(ubyes2str(pdt[2]), "triangle");
    EXPECT_EQ(ubyes2str(pdt[4]), "triangulate");
    EXPECT_EQ(ubyes2str(pdt[8]), "triply");
    EXPECT_EQ(ubyes2str(pdt[1]), "trial");
    EXPECT_EQ(ubyes2str(pdt[6]), "trie");
    EXPECT_EQ(ubyes2str(pdt[0]), "three");
    EXPECT_EQ(ubyes2str(pdt[3]), "triangular");
    EXPECT_EQ(ubyes2str(pdt[5]), "triangulaus");
}

TEST(PDT_TEST, OPERATOR_INDEX_2) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "pace");    // 0
    append_to_trie(trieBuilder, "package"); // 1
    append_to_trie(trieBuilder, "pacman");  // 2
    append_to_trie(trieBuilder, "pancake"); // 3
    append_to_trie(trieBuilder, "pea");     // 4
    append_to_trie(trieBuilder, "peek");    // 5
    append_to_trie(trieBuilder, "peel");    // 6
    append_to_trie(trieBuilder, "pikachu"); // 7
    append_to_trie(trieBuilder, "pod");     // 8
    append_to_trie(trieBuilder, "pokemon"); // 9
    append_to_trie(trieBuilder, "pool");    // 10
    append_to_trie(trieBuilder, "proof");   // 11
    append_to_trie(trieBuilder, "three");   // 12
    append_to_trie(trieBuilder, "trial");   // 13
    append_to_trie(trieBuilder, "triangle");// 14
    append_to_trie(trieBuilder, "triangular"); // 15
    append_to_trie(trieBuilder, "triangulate");// 16
    append_to_trie(trieBuilder, "triangulaus");// 17
    append_to_trie(trieBuilder, "trie");       // 18
    append_to_trie(trieBuilder, "triple");     // 19
    append_to_trie(trieBuilder, "triply");     // 20
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);

    EXPECT_EQ(ubyes2str(pdt[9]), "pokemon");
    EXPECT_EQ(ubyes2str(pdt[7]), "pikachu");
    EXPECT_EQ(ubyes2str(pdt[17]), "triangulaus");
    EXPECT_EQ(ubyes2str(pdt[6]), "peel");
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}