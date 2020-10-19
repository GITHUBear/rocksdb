#include <gtest/gtest.h>
#include "include/rocksdb/filter_policy.h"
#include "include/rocksdb/slice.h"

using namespace rocksdb;

TEST(TEST_PDT_FILTER_POLICY, TEST_1) {
    const FilterPolicy* pdt = NewCentriodPdtFilterPolicy(true);
    std::vector<Slice> slices{"three", "trial", "triangle",
                              "triangular", "triangulate", "triangulaus",
                              "trie", "triple", "triply"};
    
    std::string encoded_str;
    pdt->CreateFilter(slices.data(), slices.size(), &encoded_str);

    Slice encoded_slice(encoded_str);
    EXPECT_TRUE(pdt->KeyMayMatch("trie", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("triangulate", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("three", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("trial", encoded_slice));
    EXPECT_FALSE(pdt->KeyMayMatch("trianguluas", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("triangulaus", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("triangle", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("triangulate", encoded_slice));
    EXPECT_FALSE(pdt->KeyMayMatch("tr", encoded_slice));
    EXPECT_FALSE(pdt->KeyMayMatch("pokemon", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("triple", encoded_slice));
}

TEST(TEST_PDT_FILTER_POLICY, TEST_2) {
    const FilterPolicy* pdt = NewCentriodPdtFilterPolicy(true);
    std::vector<Slice> slices{"pace", "package", "pacman",
                              "pancake", "pea", "peek",
                              "peel", "pikachu", "pod", "pokemon",
                              "pool", "proof",
                              "three", "trial", "triangle",
                              "triangular", "triangulate", "triangulaus",
                              "trie", "triple", "triply"};

    std::string encoded_str;
    pdt->CreateFilter(slices.data(), slices.size(), &encoded_str);

    Slice encoded_slice(encoded_str);
    EXPECT_TRUE(pdt->KeyMayMatch("pikachu", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("pea", encoded_slice));
    EXPECT_TRUE(pdt->KeyMayMatch("trie", encoded_slice));
    EXPECT_FALSE(pdt->KeyMayMatch("peace", encoded_slice));
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}