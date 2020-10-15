//
// Created by Dim Dew on 2020-10-08.
//
#include <gtest/gtest.h>
#include "varint_encode.h"

using namespace rocksdb;

TEST(VARINT_TEST, TEST_1) {
    std::vector<size_t> benches{847174, 9584965, 0, 455678087, 54766, 124};
    std::vector<uint8_t> bytes;

    for (auto n : benches) {
        succinct::varint_encode_to(bytes, n);
    }

    size_t offset = 0;
    size_t res;
    int idx = 0;
    while (offset < bytes.size()) {
        offset += succinct::varint_decode_from(bytes, offset, res);
        EXPECT_EQ(res, benches[idx++]);
    }

    EXPECT_EQ(idx, benches.size());
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
