//
// Created by Dim Dew on 2020-10-18.
//
#include <gtest/gtest.h>
#include "balanced_parentheses_vector.h"

using namespace rocksdb;

succinct::BpVector parentheses2BpVector(const std::string& s) {
    succinct::BitVectorBuilder builder;
    for (auto bin : s) {
        assert(bin == '(' || bin == ')');
        if (bin == ')') {
            builder.push_back(false);
        } else {
            builder.push_back(true);
        }
    }
    return succinct::BpVector(&builder);
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

TEST(BP_VECTOR_ENCODE_DECODE, TEST_1) {
    std::string s("((((((((())))()()))(())))()((()()(())))())"
                  "((((((((())))()()))(())))()((()()(())))())");
    succinct::BpVector bpVector = parentheses2BpVector(s);

    uint64_t word_size = bpVector.data().size();
    EXPECT_EQ(word_size, 2);
    size_t bit_size = bpVector.size();
    EXPECT_EQ(bit_size, s.size());

    std::string encode;
    size_t byte_size = word_size * sizeof(uint64_t);
    encode.resize(byte_size, 0);
    char* wp = &encode[0];
    for (size_t i = 0; i < word_size; i++) {
        memcpy(wp, &(bpVector.data()[i]), sizeof(uint64_t));
        wp += sizeof(uint64_t);
    }
    std::string encode_copy(encode);

    {
        auto p = encode.data();
        succinct::BpVector bpVector2(reinterpret_cast<const uint64_t *>(p), word_size, bit_size);
        EXPECT_EQ(get_bp_str(bpVector2), s);
    }
    EXPECT_EQ(encode, encode_copy);
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}