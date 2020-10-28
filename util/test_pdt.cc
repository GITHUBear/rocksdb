#include "include/rocksdb/filter_policy.h"
#include "include/rocksdb/slice.h"
#include "util/coding.h"
#include "test_util/testharness.h"
#include "test_util/testutil.h"
#include "util/gflags_compat.h"

using GFLAGS_NAMESPACE::ParseCommandLineFlags;

namespace rocksdb {
    static const int kVerbose = 1;

    static Slice Key(int i, char* buffer) {
        std::string s;
        PutFixed32(&s, static_cast<uint32_t>(i));
        memcpy(buffer, s.c_str(), sizeof(i));
        return Slice(buffer, sizeof(i));
    }

    static int NextLength(int length) {
        if (length < 10) {
            length += 1;
        } else if (length < 100) {
            length += 10;
        } else if (length < 1000) {
            length += 100;
        } else {
            length += 1000;
        }
        return length;
    }

    class PdtTest : public testing::Test {
        private:
            const FilterPolicy* pdt_filter_;
            std::string filter_;
            std::vector<std::string> keys_;
        
        public:
            PdtTest()
                : pdt_filter_(NewCentriodPdtFilterPolicy()) {}
            
            ~PdtTest() { delete pdt_filter_; }

            void Reset() {
                keys_.clear();
                filter_.clear();
            }

            void Add(const Slice& s) {
                keys_.push_back(s.ToString());
            }

            void Build() {
                std::vector<Slice> key_slices;
                for (size_t i = 0; i < keys_.size(); i++) {
                    key_slices.push_back(Slice(keys_[i]));
                }
                filter_.clear();
                pdt_filter_->CreateFilter(&key_slices[0], static_cast<int>(key_slices.size()),
                                    &filter_);
                keys_.clear();
                if (kVerbose >= 2) DumpFilter();
            }

            size_t FilterSize() const {
                return filter_.size();
            }

            void DumpFilter() {
                fprintf(stderr, "F(");
                for (size_t i = 0; i+1 < filter_.size(); i++) {
                    const unsigned int c = static_cast<unsigned int>(filter_[i]);
                    for (int j = 0; j < 8; j++) {
                        fprintf(stderr, "%c", (c & (1 <<j)) ? '1' : '.');
                    }
                }
                fprintf(stderr, ")\n");
            }

            bool Matches(const Slice& s) {
                if (!keys_.empty()) {
                    Build();
                }
                return pdt_filter_->KeyMayMatch(s, filter_);
            }

            double FalsePositiveRate() {
                char buffer[sizeof(int)];
                int result = 0;
                for (int i = 0; i < 10000; i++) {
                    if (Matches(Key(i + 1000000000, buffer))) {
                        result++;
                    }
                }
                return result / 10000.0;
            }
    };

    TEST_F(PdtTest, Small) {
        Add("hello");
        Add("world");
        ASSERT_TRUE(Matches("hello"));
        ASSERT_TRUE(Matches("world"));
        ASSERT_TRUE(! Matches("x"));
        ASSERT_TRUE(! Matches("foo"));
    }

    // TEST_F(PdtTest, EmptyFilter) {
    //     ASSERT_TRUE(! Matches("hello"));
    //     ASSERT_TRUE(! Matches("world"));
    // }

    TEST_F(PdtTest, VaryingLengths) {
        char buffer[sizeof(int)];

        for (int length = 1; length <= 10000; length = NextLength(length)) {
            Reset();
            for (int i = 0; i < length; i++) {
                Add(Key(i, buffer));
            }
            Build();

            // All added keys must match
            for (int i = 0; i < length; i++) {
            ASSERT_TRUE(Matches(Key(i, buffer)))
                << "Length " << length << "; key " << i;
            }

            // Check false positive rate
            double rate = FalsePositiveRate();
            if (kVerbose >= 1) {
            fprintf(stderr, "False positives: %5.2f%% @ length = %6d ; bytes = %6d\n",
                    rate*100.0, length, static_cast<int>(FilterSize()));
            }
        }
    }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  ParseCommandLineFlags(&argc, &argv, true);

  return RUN_ALL_TESTS();
}