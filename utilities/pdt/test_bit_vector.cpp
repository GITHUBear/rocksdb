//
// Created by Dim Dew on 2020-07-21.
//
#include <gtest/gtest.h>
#include <iostream>
#include "bit_vector.h"
#include "rank_select_bit_vector.h"

using namespace rocksdb;

succinct::RsBitVector seq012BitVector(const std::string& s) {
    succinct::BitVectorBuilder builder;
    for (auto bin : s) {
        assert(bin == '0' || bin == '1');
        if (bin == '0') {
            builder.push_back(false);
        } else {
            builder.push_back(true);
        }
    }
    return succinct::RsBitVector(&builder);
}

TEST(BIT_VECTOR_TEST, RANK_1) {
    std::string s("0100001001110101101110111110101100001"
                  "0100001001110101101110111110101100001");
    succinct::RsBitVector rsBitVector = seq012BitVector(s);

    uint64_t res = rsBitVector.rank(1);
    EXPECT_EQ(res, 0);

    res = rsBitVector.rank(2);
    EXPECT_EQ(res, 1);

    res = rsBitVector.rank(6);
    EXPECT_EQ(res, 1);

    res = rsBitVector.rank(8);
    EXPECT_EQ(res, 2);

    res = rsBitVector.rank(9);
    EXPECT_EQ(res, 2);

    res = rsBitVector.rank(s.size());
    EXPECT_EQ(res, 40);
}

TEST(BIT_VECTOR_TEST, SELECT_1) {
    std::string s("0100001001110101101110111110101100001"
                  "0100001001110101101110111110101100001");
    succinct::RsBitVector rsBitVector = seq012BitVector(s);

    uint64_t res = rsBitVector.select(0);
    EXPECT_EQ(res, 1);

    res = rsBitVector.select(1);
    EXPECT_EQ(res, 6);

    res = rsBitVector.select(39);
    EXPECT_EQ(res, s.size() - 1);
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
