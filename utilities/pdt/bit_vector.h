//
// Created by Dim Dew on 2020-07-18.
//

#ifndef PATH_DECOMPOSITION_TRIE_BIT_VECTOR_H
#define PATH_DECOMPOSITION_TRIE_BIT_VECTOR_H

#include <vector>
#include <algorithm>

#include "bit_util.h"
#include "mappable_vector.h"

namespace rocksdb {
namespace succinct {
    class BitVectorBuilder {
    public:
        typedef std::vector<uint64_t> bits_t;
        const uint64_t WORD_SIZE = 64;

        // noncopyable because we have a pointer member
        BitVectorBuilder &operator=(const BitVectorBuilder &builder) = delete;

        BitVectorBuilder(const BitVectorBuilder &builder) = delete;

        BitVectorBuilder() : m_size_(0), m_cur_word_(nullptr) {};

        // new a BitVectorBuilder containing a vector of `size` bits
        // the elements of the vector are all 0
        // if `set` is set, the elements of the vector are all 0 in bits
        BitVectorBuilder(uint64_t size, bool set = 0)
                : m_size_(size) {
            m_bits_.resize(word_for(size), uint64_t(-set));
            if (size) {
                m_cur_word_ = &m_bits_.back();
                if (set && size % WORD_SIZE) {
                    *m_cur_word_ >>= WORD_SIZE - (size % WORD_SIZE);
                }
            }
        }

        void reserve(uint64_t size) {
            m_bits_.reserve(word_for(size));
        }

        // append a `b` bit into vector
        inline void push_back(bool b) {
            uint64_t pos_in_word = m_size_ % WORD_SIZE;
            if (pos_in_word == 0) {
                m_bits_.push_back(0);
                m_cur_word_ = &m_bits_.back();
            }
            *m_cur_word_ |= (uint64_t) b << pos_in_word;
            ++m_size_;
        }

        // set `b` bit at `pos` index
        inline void set(uint64_t pos, bool b) {
            uint64_t word = pos / WORD_SIZE;
            uint64_t pos_in_word = pos % WORD_SIZE;

            m_bits_[word] &= ~(uint64_t(1) << pos_in_word);
            m_bits_[word] |= uint64_t(b) << pos_in_word;
        }

        // set `bits` of length `len` at `pos` index
        // len >= 0 && len <= 64
        inline void set_bits(uint64_t pos, uint64_t bits, size_t len) {
            assert(pos + len <= size());
            // check there are no spurious bits
            assert(len == WORD_SIZE || (bits >> len) == 0);
            if (!len) return;
            uint64_t mask = (len == WORD_SIZE) ? uint64_t(-1) : ((uint64_t(1) << len) - 1);
            uint64_t word = pos / WORD_SIZE;
            uint64_t pos_in_word = pos % WORD_SIZE;

            m_bits_[word] &= ~(mask << pos_in_word);
            m_bits_[word] |= bits << pos_in_word;

            uint64_t stored = WORD_SIZE - pos_in_word;
            if (stored < len) {
                m_bits_[word + 1] &= ~(mask >> stored);
                m_bits_[word + 1] |= bits >> stored;
            }
        }

        // append `bits` of length `len`
        // len >= 0 && len <= 64
        inline void append_bits(uint64_t bits, size_t len) {
            // check there are no spurious bits
            assert(len == WORD_SIZE || (bits >> len) == 0);
            if (!len) return;
            uint64_t pos_in_word = m_size_ % WORD_SIZE;
            m_size_ += len;
            if (pos_in_word == 0) {
                m_bits_.push_back(bits);
            } else {
                *m_cur_word_ |= bits << pos_in_word;
                if (len > WORD_SIZE - pos_in_word) {
                    m_bits_.push_back(bits >> (WORD_SIZE - pos_in_word));
                }
            }
            m_cur_word_ = &m_bits_.back();
        }

        // append n `0` bits
        inline void zero_extend(uint64_t n) {
            m_size_ += n;
            uint64_t needed = word_for(m_size_) - m_bits_.size();
            if (needed) {
                m_bits_.insert(m_bits_.end(), needed, 0);
                m_cur_word_ = &m_bits_.back();
            }
        }

        // append n `1` bits
        inline void one_extend(uint64_t n) {
            while (n >= 64) {
                append_bits(uint64_t(-1), WORD_SIZE);
                n -= WORD_SIZE;
            }
            if (n) {
                append_bits(uint64_t(-1) >> (WORD_SIZE - n), n);
            }
        }

        // append another BitVectorBuilder's m_bits
        void append(const BitVectorBuilder &rhs) {
            if (!rhs.size()) return;

            uint64_t pos = m_bits_.size();
            uint64_t shift = size() % WORD_SIZE;
            m_size_ = size() + rhs.size();
            m_bits_.resize(word_for(m_size_));

            if (shift == 0) { // word-aligned, easy case
                std::copy(rhs.m_bits_.begin(), rhs.m_bits_.end(),
                          m_bits_.begin() + std::ptrdiff_t(pos));
            } else {
                uint64_t *cur_word = &m_bits_.front() + pos - 1;
                for (size_t i = 0; i < rhs.m_bits_.size() - 1; ++i) {
                    uint64_t w = rhs.m_bits_[i];
                    *cur_word |= w << shift;
                    *++cur_word = w >> (WORD_SIZE - shift);
                }
                *cur_word |= rhs.m_bits_.back() << shift;
                if (cur_word < &m_bits_.back()) {
                    *++cur_word = rhs.m_bits_.back() >> (WORD_SIZE - shift);
                }
            }
            m_cur_word_ = &m_bits_.back();
        }

        // reverse in place
        void reverse() {
            uint64_t shift = WORD_SIZE - (size() % WORD_SIZE);

            uint64_t remainder = 0;
            for (size_t i = 0; i < m_bits_.size(); ++i) {
                uint64_t cur_word;
                if (shift != WORD_SIZE) { // this should be hoisted out
                    cur_word = remainder | (m_bits_[i] << shift);
                    remainder = m_bits_[i] >> (WORD_SIZE - shift);
                } else {
                    cur_word = m_bits_[i];
                }
                // reverse bits in a word
                m_bits_[i] = succinct::util::reverse_bits(cur_word);
            }
            assert(remainder == 0);
            std::reverse(m_bits_.begin(), m_bits_.end());
        }

        void swap(BitVectorBuilder &other) {
            m_bits_.swap(other.m_bits_);
            std::swap(m_size_, other.m_size_);
            std::swap(m_cur_word_, other.m_cur_word_);
        }

        bits_t& move_bits() {
            assert(word_for(m_size_) == m_bits_.size());
            return m_bits_;
        }


        uint64_t size() const {
            return m_size_;
        }

    private:
        inline uint64_t word_for(uint64_t size) {
            return (size + WORD_SIZE - 1) / WORD_SIZE;
        }

        bits_t m_bits_;
        uint64_t m_size_;           // bit size
        uint64_t *m_cur_word_;
    };

    class BitVector {
    public:
        BitVector(): m_size_(0) {}

        // use a boolean vector to create a new bit vector.
        BitVector(const std::vector<bool>& bools) {
            std::vector<uint64_t> bits;
            const uint64_t first_mask = uint64_t(1);
            uint64_t mask = first_mask;
            uint64_t cur_val = 0;
            m_size_ = 0;
            for (auto bit : bools) {
                if (bit) {
                    cur_val |= mask;
                }
                mask <<= 1;
                m_size_ += 1;
                if (!mask) {
                    bits.push_back(cur_val);
                    mask = first_mask;
                    cur_val = 0;
                }
            }
            if (mask != first_mask) {
                bits.push_back(cur_val);
            }
            m_bits_.steal(bits);
        }

        // create BitVector from BitVectorBuilder.
        BitVector(BitVectorBuilder* builder) {
            m_size_ = builder->size();
            m_bits_.steal(builder->move_bits());
        }

        BitVector(const uint64_t* raw_data, uint64_t word_size, size_t bit_size)
                : m_size_(bit_size)
                , m_bits_(raw_data, word_size)
        {}

        void swap(BitVector& other) {
            std::swap(other.m_size_, m_size_);
            other.m_bits_.swap(m_bits_);
        }

        inline size_t size() const {
            return m_size_;
        }

        // get bit at `pos`.
        inline bool operator[](uint64_t pos) const {
            assert(pos < m_size_);
            uint64_t block = pos / 64;
            assert(block < m_bits_.size());
            uint64_t shift = pos % 64;
            return (m_bits_[block] >> shift) & 1;
        }

        // `len` <= 64
        inline uint64_t get_bits(uint64_t pos, uint64_t len) const {
            assert(pos + len <= size());
            if (!len) {
                return 0;
            }
            uint64_t block = pos / 64;
            uint64_t shift = pos % 64;
            uint64_t mask = -(len == 64) | ((1ULL << len) - 1);
            if (shift + len <= 64) {
                return m_bits_[block] >> shift & mask;
            } else {
                return (m_bits_[block] >> shift) | (m_bits_[block + 1] << (64 - shift) & mask);
            }
        }

        // same as get_bits(pos, 64) but it can extend further size(), padding with zeros
        inline uint64_t get_word(uint64_t pos) const
        {
            assert(pos < size());
            uint64_t block = pos / 64;
            uint64_t shift = pos % 64;
            uint64_t word = m_bits_[block] >> shift;
            if (shift && block + 1 < m_bits_.size()) {
                word |= m_bits_[block + 1] << (64 - shift);
            }
            return word;
        }

        // find first 0 bit in BitVector at pos <= `pos`.
        // TODO: Is it OK for all set BitVector?
        inline uint64_t predecessor0(uint64_t pos) const {
            assert(pos < m_size_);
            uint64_t block = pos / 64;
            uint64_t shift = 64 - pos % 64 - 1;
            uint64_t word = ~m_bits_[block];
            word = (word << shift) >> shift;

            unsigned long ret;
            while (!util::msb(word, ret)) {
                assert(block);
                word = ~m_bits_[--block];
            }
            return block * 64 + ret;
        }

        // find first 0 bit in BitVector at pos >= `pos`.
        inline uint64_t successor0(uint64_t pos) const {
            assert(pos < m_size_);
            uint64_t block = pos / 64;
            uint64_t shift = pos % 64;
            uint64_t word = (~m_bits_[block] >> shift) << shift;

            unsigned long ret;
            while (!util::lsb(word, ret)) {
                ++block;
                assert(block < m_bits_.size());
                word = ~m_bits_[block];
            }
            return block * 64 + ret;
        }

        // find first 1 bit in BitVector at pos <= `pos`.
        inline uint64_t predecessor1(uint64_t pos) const {
            assert(pos < m_size_);
            uint64_t block = pos / 64;
            uint64_t shift = 64 - pos % 64 - 1;
            uint64_t word = m_bits_[block];
            word = (word << shift) >> shift;

            unsigned long ret;
            while (!util::msb(word, ret)) {
                assert(block);
                word = m_bits_[--block];
            };
            return block * 64 + ret;
        }

        // find first 1 bit in BitVector at pos >= `pos`.
        inline uint64_t successor1(uint64_t pos) const {
            assert(pos < m_size_);
            uint64_t block = pos / 64;
            uint64_t shift = pos % 64;
            uint64_t word = (m_bits_[block] >> shift) << shift;

            unsigned long ret;
            while (!util::lsb(word, ret)) {
                ++block;
                assert(block < m_bits_.size());
                word = m_bits_[block];
            };
            return block * 64 + ret;
        }

        const mappable_vector<uint64_t>& data() const {
            return m_bits_;
        }

        // ------------ the bitwise iterator of BitVector -------------
        struct enumerator {
            enumerator():
                m_bv_(0),
                m_pos_(uint64_t(-1)) {}

            enumerator(const BitVector& bv, size_t pos)
                    : m_bv_(&bv)
                    , m_pos_(pos)
                    , m_buf_(0)
                    , m_avail_(0){
                // In ot/path_decomposition_trie call prefetch here
                // TODO: Can we do nothing here?
            }

            // get next `m_pos_` bit in BitVector
            inline bool next()
            {
                if (!m_avail_) fill_buf();
                bool b = m_buf_ & 1;
                m_buf_ >>= 1;
                m_avail_ -= 1;
                m_pos_ += 1;
                return b;
            }

            // take bits of length `l` at `m_pos_`
            inline uint64_t take(size_t l)
            {
                if (m_avail_ < l) fill_buf();
                uint64_t val;
                if (l != 64) {
                    val = m_buf_ & ((uint64_t(1) << l) - 1);
                    m_buf_ >>= l;
                } else {
                    val = m_buf_;
                }
                m_avail_ -= l;
                m_pos_ += l;
                return val;
            }

            // skip next zeros and the first 1.
            // RETURN: the size of zeros
            inline uint64_t skip_zeros()
            {
                uint64_t zs = 0;
                // XXX the loop may be optimized by aligning access
                while (!m_buf_) {
                    m_pos_ += m_avail_;
                    zs += m_avail_;
                    m_avail_ = 0;
                    fill_buf();
                }

                uint64_t l = util::lsb(m_buf_);
                m_buf_ >>= l;
                m_buf_ >>= 1;
                m_avail_ -= l + 1;
                m_pos_ += l + 1;
                return zs + l;
            }

            inline uint64_t position() const
            {
                return m_pos_;
            }

        private:
            // load a word in BitVector at `m_pos_`
            inline void fill_buf()
            {
                m_buf_ = m_bv_->get_word(m_pos_);
                m_avail_ = 64;
            }

            const BitVector* m_bv_;
            size_t m_pos_;         // current bit position
            uint64_t m_buf_;       // buffer of `m_avail_` length of bits
            size_t m_avail_;       // available bit length of `m_buf_`
        };

        // -------------- the iterator of set-bits of BitVector ---------------
        struct unary_enumerator {
            unary_enumerator()
                    : m_data_(0)
                    , m_pos_(0)
                    , m_buf_(0)
            {}

            unary_enumerator(const BitVector& bv, uint64_t pos)
            {
                m_data_ = bv.data().data();
                m_pos_ = pos;
                m_buf_ = m_data_[pos / 64];
                // clear low bits
                m_buf_ &= uint64_t(-1) << (pos % 64);
            }

            uint64_t position() const
            {
                return m_pos_;
            }

            // Update `m_buf_` and `m_pos_` to next set-bit's position.
            // RETURN: next set-bit's position.
            uint64_t next()
            {
                unsigned long pos_in_word;
                uint64_t buf = m_buf_;
                while (!util::lsb(buf, pos_in_word)) {
                    m_pos_ += 64;
                    buf = m_data_[m_pos_ / 64];
                }

                m_buf_ = buf & (buf - 1); // clear LSB
                m_pos_ = (m_pos_ & ~uint64_t(63)) + pos_in_word;
                return m_pos_;
            }

            // skip to the k-th one after the current position
            // `k` starts from 0
            // After `skip`, `m_pos_` points to the k-th 1-bit.
            void skip(uint64_t k)
            {
                uint64_t skipped = 0;
                uint64_t buf = m_buf_;
                uint64_t w = 0;
                while (skipped + (w = util::popcount(buf)) <= k) {
                    skipped += w;
                    m_pos_ += 64;
                    buf = m_data_[m_pos_ / 64];
                }
                assert(buf);
                uint64_t pos_in_word = util::select_in_word(buf, k - skipped);
                m_buf_ = buf & (uint64_t(-1) << pos_in_word);
                m_pos_ = (m_pos_ & ~uint64_t(63)) + pos_in_word;
            }

            // return the position of the k-th one after the current position.
            uint64_t skip_no_move(uint64_t k)
            {
                uint64_t position = m_pos_;
                uint64_t skipped = 0;
                uint64_t buf = m_buf_;
                uint64_t w = 0;
                while (skipped + (w = util::popcount(buf)) <= k) {
                    skipped += w;
                    position += 64;
                    buf = m_data_[position / 64];
                }
                assert(buf);
                uint64_t pos_in_word = util::select_in_word(buf, k - skipped);
                position = (position & ~uint64_t(63)) + pos_in_word;
                return position;
            }

            // skip to the k-th zero after the current position
            // `k` starts from 0
            // After `skip0`, `m_pos_` points to the k-th 0-bit.
            void skip0(uint64_t k)
            {
                uint64_t skipped = 0;
                uint64_t pos_in_word = m_pos_ % 64;
                uint64_t buf = ~m_buf_ & (uint64_t(-1) << pos_in_word);
                uint64_t w = 0;
                while (skipped + (w = util::popcount(buf)) <= k) {
                    skipped += w;
                    m_pos_ += 64;
                    buf = ~m_data_[m_pos_ / 64];
                }
                assert(buf);
                pos_in_word = util::select_in_word(buf, k - skipped);
                m_buf_ = ~buf & (uint64_t(-1) << pos_in_word);
                m_pos_ = (m_pos_ & ~uint64_t(63)) + pos_in_word;
            }

        private:
            const uint64_t* m_data_;
            uint64_t m_pos_;
            uint64_t m_buf_;           // m_buf_ is aligned
        };

    protected:
        size_t m_size_;                     // bit size
        mappable_vector<uint64_t> m_bits_;
    };
}
}

#endif //PATH_DECOMPOSITION_TRIE_BIT_VECTOR_H
