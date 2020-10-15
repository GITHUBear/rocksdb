//
// Created by Dim Dew on 2020-10-08.
//

#ifndef PATH_DECOMPOSITION_TRIE_VARINT_ENCODE_H
#define PATH_DECOMPOSITION_TRIE_VARINT_ENCODE_H

#include <cstddef>
#include <vector>
#include "bit_util.h"

namespace rocksdb {
// A temporary `varint` library for this module.
namespace succinct {
    // Use `inline` & `template` in header to avoid redefinition。
    // calculate the length of encode sequence.
    inline size_t varint_encode_len(size_t val) {
        unsigned long len;
        if(!util::msb(val, len)) len = 0;
        return (len + 7) / 7;
    }

    // encode `val` to `v` and return length of appended data.(in byte)
    inline size_t varint_encode_to(std::vector<uint8_t>& v, size_t val) {
        size_t len = varint_encode_len(val);
        for (int i = len - 1; i >= 0; i--) {
            uint8_t byte = (val >> (i * 7)) & 0x7F;
            v.push_back(byte | (i ? 0x80 : 0));
        }
        return len;
    }

    // decode `val` from `v` at the index of `offset`
    // return length of encoded data.(in byte)
    inline size_t varint_decode_from(const std::vector<uint8_t>& v, size_t offset, size_t& val) {
        size_t pos = offset;
        val = 0;
        uint8_t byte;
        do {
            byte = v[pos++];
            val <<= 7;
            val |= (byte & 0x7F);
        } while (byte & 0x80);
        return pos - offset;
    }
}
}

#endif //PATH_DECOMPOSITION_TRIE_VARINT_ENCODE_H
