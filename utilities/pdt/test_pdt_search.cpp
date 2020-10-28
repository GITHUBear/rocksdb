//
// Created by Dim Dew on 2020-10-09.
//
#include <gtest/gtest.h>
#include "path_decomposed_trie.h"

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

std::string get_label(const succinct::mappable_vector<uint16_t>& labels) {
    std::string label;
    for (size_t i = 0; i < static_cast<size_t>(labels.size()); i++) {
        uint16_t byte = labels[i];
        if (byte >> 8 == 1) {
            label += std::to_string(uint8_t(byte));
        } else if (byte >> 8 == 2) {
            label += '#';
        } else if (byte >> 8 == 4) {
            label += '$';  
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

std::string get_branch_str(const succinct::mappable_vector<uint16_t>& branches) {
    std::string branch_str;
    for (size_t i = 0; i < static_cast<size_t>(branches.size()); i++) {
        if (branches[i] == 512) branch_str += '#';
        else if (branches[i] == 1024) branch_str += '$';
        else branch_str += static_cast<char>(branches[i]);
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
    EXPECT_EQ(get_label(pdt.get_labels()), "t0hree$#i1a0l$#g0le$#la1r$#e$#s$#$#l0e$#$#");
    EXPECT_EQ(get_branch_str(pdt.get_branches()), "rpenuuty");
    EXPECT_EQ(get_bp_str(pdt.get_bp()), "(()((()()(())))())");
    for (size_t i = 0; i < static_cast<size_t>(pdt.word_positions.size()); i++) {
        printf("%lu ", pdt.word_positions[i]);
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
    uint16_t branch;
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

TEST(PDT_TEST, INDEX_3) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    std::vector<std::string> strs{"p", "pa", "pac",
                                  "pace", "pack", "packa", "package", "pacman", "pancake",
                                  "pea", "peek", "peel", "pikachu",
                                  "pod", "poe", "poem", "pok", "poke", "pokem", "pokemon",
                                  "pool", "proof",
                                  "three", "trial", "triangle", "triangular",
                                  "triangulaus", "trie", "triple", "triply"};
    for (auto s : strs) {
        append_to_trie(trieBuilder, s);
    }
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);

    for (size_t i = 0; i < strs.size(); i++) {
        EXPECT_EQ(pdt.index(strs[i]), i);
    }
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

TEST(PDT_TEST, TEST_ONLY_ONE) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    append_to_trie(trieBuilder, "pace");    // 0
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);

    EXPECT_EQ(ubyes2str(pdt[0]), "pace");
    EXPECT_EQ(pdt.index("pace"), 0);
}

std::string get_raw_label(const succinct::mappable_vector<uint16_t>& labels) {
    std::string label;
    for (size_t i = 0; i < static_cast<size_t>(labels.size()); i++) {
        uint16_t byte = labels[i];
        if (byte >> 8 == 1) {
            label += std::to_string(uint8_t(byte));
        } else if (byte >> 8 == 2) {
            label += '#';
        } else if (byte >> 8 == 4) {
            label += '$';  
        } else {
            label += '[';
            label += std::to_string(uint8_t(byte));
            label += ']';
        }
    }
    return label;
}

std::string get_raw_branch_str(const succinct::mappable_vector<uint16_t>& branches) {
    std::string branch_str;
    for (size_t i = 0; i < static_cast<size_t>(branches.size()); i++) {
        if (branches[i] == 512) branch_str += '#';
        else if (branches[i] == 1024) branch_str += '$';
        else {
            branch_str += '[';
            branch_str += std::to_string(branches[i]);
            branch_str += ']';
        }
    }
    return branch_str;
}

TEST(PDT_TEST, TEST_BUG) {
    succinct::DefaultTreeBuilder<true> pdt_builder;
    succinct::trie::compacted_trie_builder
            <succinct::DefaultTreeBuilder<true>>
            trieBuilder(pdt_builder);
    std::vector<uint8_t> bytes = {127, 0, 0, 0};
    trieBuilder.append(bytes);
    bytes[0] = 128;
    trieBuilder.append(bytes);
    // bytes[0] = 129;
    // trieBuilder.append(bytes);
    trieBuilder.finish();

    succinct::trie::DefaultPathDecomposedTrie<true> pdt(trieBuilder);

    printf("%s\n", get_bp_str(pdt.get_bp()).c_str());
    printf("%s\n", get_raw_label(pdt.get_labels()).c_str());
    printf("%s\n", get_raw_branch_str(pdt.get_branches()).c_str());
    bytes[0] = 128;
    std::string s(bytes.begin(), bytes.end());
    EXPECT_EQ(pdt.index(s), 1);
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}