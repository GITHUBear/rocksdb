#include <gtest/gtest.h>
#include "util/pdt.h"

using namespace rocksdb;

TEST(TEST_PDT_FILTER_POLICY, TEST_1) {
    PdtFilterPolicy<> pdt(true);
    std::vector<Slice> slices{"three", "trial", "triangle",
                              "triangular", "triangulate", "triangulaus",
                              "trie", "triple", "triply"};
    
    std::string encoded_str;
    pdt.CreateFilter(slices.data(), slices.size(), &encoded_str);

    Slice encoded_slice(encoded_str);
    EXPECT_TRUE(pdt.KeyMayMatch("trie", encoded_slice));
    EXPECT_TRUE(pdt.KeyMayMatch("triangulate", encoded_slice));
    EXPECT_TRUE(pdt.KeyMayMatch("three", encoded_slice));
    EXPECT_TRUE(pdt.KeyMayMatch("trial", encoded_slice));
    EXPECT_FALSE(pdt.KeyMayMatch("tr", encoded_slice));
    EXPECT_FALSE(pdt.KeyMayMatch("pokemon", encoded_slice));
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}