//
// Created by Dim Dew on 2020-07-28.
//
#include <gtest/gtest.h>
#include "balanced_parentheses_vector.h"

using namespace rocksdb;

TEST(FIND_OPEN, FIND_OPEN_IN_WORD_1) {
    uint64_t res;
    uint64_t word = 0x0F53800000000000ULL;
    uint64_t byte_cnt = succinct::util::byte_counts(word);

    EXPECT_EQ(byte_cnt, 0x0404010000000000ULL);
    ASSERT_TRUE(succinct::find_open_in_word(word, byte_cnt, 1, res));
    EXPECT_EQ(res, 47);
}

TEST(FIND_OPEN, FIND_OPEN_IN_WORD_2) {
    uint64_t res;
    uint64_t word = 0x0F53FFFFFFFFFFFFULL;
    uint64_t byte_cnt = succinct::util::byte_counts(word);

    EXPECT_EQ(byte_cnt, 0x0404080808080808ULL);
    ASSERT_TRUE(succinct::find_open_in_word(word, byte_cnt, 1, res));
    EXPECT_EQ(res, 47);
}

TEST(FIND_OPEN, FIND_OPEN_IN_WORD_3) {
    uint64_t res;
    uint64_t word = 0x2974FFFFFFFFFFFFULL;
    uint64_t byte_cnt = succinct::util::byte_counts(word);

    EXPECT_EQ(byte_cnt, 0x0304080808080808ULL);
    ASSERT_TRUE(succinct::find_open_in_word(word, byte_cnt, 1, res));
    EXPECT_EQ(res, 45);

    ASSERT_TRUE(succinct::find_open_in_word(word, byte_cnt, 2, res));
    EXPECT_EQ(res, 44);
}

TEST(FIND_OPEN, FIND_OPEN_IN_WORD_4) {
    uint64_t res;
    uint64_t word = 0x0ULL;
    uint64_t byte_cnt = succinct::util::byte_counts(word);

    EXPECT_EQ(byte_cnt, 0x0ULL);
    ASSERT_FALSE(succinct::find_open_in_word(word, byte_cnt, 1, res));
}

succinct::BpVector parentheses2BpVector(std::string s) {
    succinct::BitVectorBuilder bitVectorBuilder;
    for (auto p : s) {
        assert(p == '(' || p == ')');
        if (p == '(') {
            bitVectorBuilder.push_back(true);
        } else {
            bitVectorBuilder.push_back(false);
        }
    }
    return succinct::BpVector(&bitVectorBuilder);
}

TEST(FIND_OPEN_BP_VECTOR, FIND_OPEN_0) {
    std::string s("(()((()()))(()()()()()()()()()()()()()()()()()"
                  "()()()()()()()()()()()()()()()()()()()()()()()))");
    succinct::BpVector bpVector = parentheses2BpVector(s);
    uint64_t res = bpVector.find_close(0);
    EXPECT_EQ(res, s.size() - 1);

    res = bpVector.find_close(1);
    EXPECT_EQ(res, 2);

    res = bpVector.find_close(3);
    EXPECT_EQ(res, 10);

    res = bpVector.find_close(4);
    EXPECT_EQ(res, 9);

    res = bpVector.find_close(5);
    EXPECT_EQ(res, 6);

    res = bpVector.find_close(7);
    EXPECT_EQ(res, 8);

    res = bpVector.find_close(11);
    EXPECT_EQ(res, s.size() - 2);
}

TEST(FIND_OPEN_BP_VECTOR, FIND_OPEN_1) {
    std::string s("(()((()()))(()()()()()()()()()()()()()()()()()"
                  "()()()()()()()()()()()()()()()()()()()()()()()))");
    succinct::BpVector bpVector = parentheses2BpVector(s);
    uint64_t res = bpVector.find_open(s.size() - 1);
    EXPECT_EQ(res, 0);

    res = bpVector.find_open(2);
    EXPECT_EQ(res, 1);

    res = bpVector.find_open(10);
    EXPECT_EQ(res, 3);

    res = bpVector.find_open(9);
    EXPECT_EQ(res, 4);

    res = bpVector.find_open(6);
    EXPECT_EQ(res, 5);

    res = bpVector.find_open(8);
    EXPECT_EQ(res, 7);

    res = bpVector.find_open(s.size() - 2);
    EXPECT_EQ(res, 11);
}

GTEST_API_ int main(int argc, char ** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
