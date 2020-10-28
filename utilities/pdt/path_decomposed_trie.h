//
// Created by Dim Dew on 2020-08-11.
//

#ifndef PATH_DECOMPOSITION_TRIE_PATH_DECOMPOSED_TRIE_H
#define PATH_DECOMPOSITION_TRIE_PATH_DECOMPOSED_TRIE_H

#include "util/coding.h"
#include "compacted_trie_builder.h"
#include "default_tree_builder.h"
#include "balanced_parentheses_vector.h"

namespace rocksdb {
namespace succinct {
    namespace trie {
        // false - CENTROID, true - LEX
        template<bool Lexicographic = false>
        struct DefaultPathDecomposedTrie {
            mappable_vector<uint16_t> m_labels;      // `L` in paper
            mappable_vector<uint16_t> m_branches;     // `B` in paper
            BpVector m_bp;                       // `BP` in paper
            // TODO: Use elias-fano encoding later.
            mappable_vector<uint64_t> word_positions;
            bool is_portable = false;

            DefaultPathDecomposedTrie(compacted_trie_builder
                                      <DefaultTreeBuilder<Lexicographic>> &trieBuilder) {
                assert(trieBuilder.is_finish());

                typename DefaultTreeBuilder<Lexicographic>::representation_type
                        root = trieBuilder.get_root();
                m_labels.steal(root->m_labels);
                m_branches.steal(root->m_branches);
                // [double free error] m_bp = BpVector(&root->m_bp, false, true);(fxxk c++!!!!)
                auto tmp = BpVector(&root->m_bp, false, true);
                m_bp.swap(tmp);

                assert(m_labels.back() == DefaultTreeBuilder<Lexicographic>::DELIMITER_FLAG);

                // This method to calculate delimiter position in m_labels is naive.
                // TODO: optimal needed
                std::vector<uint64_t> tmp_vec;
                tmp_vec.push_back(0);
                for (size_t i = 0; i + 1 < m_labels.size(); i++) {
                    if (get_portable16(m_labels[i]) == DefaultTreeBuilder<Lexicographic>::DELIMITER_FLAG) {
                        tmp_vec.push_back(i + 1);
                    }
                }
                tmp_vec.push_back(m_labels.size());
                word_positions.steal(tmp_vec);
            }

            // The constructor is used for decoding.
            DefaultPathDecomposedTrie(const uint16_t* label_ptr, uint64_t label_len,
                                      const uint16_t* branch_ptr, uint64_t branch_len,
                                      const uint64_t* raw_data, uint64_t word_size, size_t bit_size,
                                      const uint64_t* pos_ptr, uint64_t pos_len,
                                      bool portable = false)
                                      : m_labels(label_ptr, label_len)
                                      , m_branches(branch_ptr, branch_len)
                                      , m_bp(raw_data, word_size, bit_size, false, true)
                                      , word_positions(pos_ptr, pos_len)
                                      , is_portable(portable) {}

            const mappable_vector<uint16_t> &get_labels() const {
                return m_labels;
            }

            const mappable_vector<uint16_t> &get_branches() const {
                return m_branches;
            }

            // TODO: return a const reference OK?
            const BpVector& get_bp() const {
                return m_bp;
            }

            const mappable_vector<uint64_t> &get_word_pos() const {
                return word_positions;
            }

            size_t size() const {
                return m_labels.size() + m_branches.size() + m_bp.size();
            }

            bool get_branch_idx_by_node_idx(size_t node_idx, size_t& end, size_t& num) const {
                size_t bp_idx = m_bp.select0(node_idx);
                if (m_bp.rank(bp_idx) < 2) {
                    return false;
                }
                end = m_bp.rank(bp_idx) - 2;
                if (!node_idx) {
                    num = end + 1;
                    return true;
                }
                num = bp_idx - m_bp.predecessor0(bp_idx - 1) - 1;
                return false;
            }

            // `branch_idx` for `m_bp`
            size_t get_node_idx_by_branch_idx(size_t branch_idx) const {
                assert(branch_idx != 0 && m_bp[branch_idx]);
                return m_bp.rank0(m_bp.successor0(m_bp.find_close(branch_idx) + 1));
            }

            bool get_parent_node_branch_by_node_idx(
                    size_t node_idx, size_t& parent_idx,
                    uint16_t& branch, size_t& branch_no) const {
                if (!node_idx) return false;
                size_t node_bp_idx = m_bp.select0(node_idx);
                assert(node_bp_idx >= 1);
                size_t parent_open = m_bp.find_open(m_bp.predecessor0(node_bp_idx - 1));
                assert(m_bp[parent_open]);
                parent_idx = m_bp.rank0(parent_open);
                size_t parent_node_bp_end = m_bp.successor0(parent_open);
                size_t parent_branch_end = m_bp.rank(parent_node_bp_end) - 2;
                branch_no = parent_node_bp_end - parent_open;
                branch = get_portable16(m_branches[parent_branch_end + parent_open + 1 - parent_node_bp_end]);
                return true;
            }

            // In rocksdb, we will not use string any more, maybe InternalKey...
            // get the index of `val` in the string set, if not exists return -1.
            int index(const std::string &s) const {
                std::vector<uint16_t> val;
                val.resize(s.size());
                std::transform(s.begin(), s.end(), val.begin(), [] (char c) {
                    return static_cast<uint16_t>(static_cast<uint8_t>(c));
                });
                val.push_back(DefaultTreeBuilder<Lexicographic>::WORD_EOF);
                size_t len = val.size();
                size_t cur_node_idx = 0;
                size_t matching_idx = 0;
                // matching in the trie.
                while (true) {
                    size_t cur_label_idx = static_cast<size_t>(get_portable64(word_positions[cur_node_idx]));
                    size_t cur_node_bp_idx = m_bp.select0(cur_node_idx);
                    size_t all_branch_num = 0, branch_end = 0;
                    get_branch_idx_by_node_idx(cur_node_idx, branch_end, all_branch_num);
                    size_t cur_branch_idx = (branch_end + 1) - all_branch_num;
                    // matching in a node.
                    while (true) {
                        if (get_portable16(m_labels[cur_label_idx]) ==
                            DefaultTreeBuilder<Lexicographic>::DELIMITER_FLAG) {
                            return (matching_idx == len ? cur_node_idx : -1);
                        }
                        if (matching_idx >= len) {
                            return -1;
                        }
                        if (get_portable16(m_labels[cur_label_idx]) >> 8 == 1) {
                            // assert(get_portable16(m_labels[cur_label_idx + 1]) >> 8 == 0);
                            auto branch0 = get_portable16(m_labels[cur_label_idx + 1]);
                            size_t cur_branch_num = static_cast<uint8_t>(get_portable16(m_labels[cur_label_idx])) + 1;

                            if (branch0 == val[matching_idx]) {
                                // update `cur_branch_idx`.
                                cur_branch_idx += cur_branch_num;
                                matching_idx++;
                                cur_label_idx += 2;
                                continue;
                            } else {
                                // check branches.
                                assert(cur_branch_num <= all_branch_num);
                                bool find_branch = false;
                                size_t cur_branch_end = cur_branch_idx + cur_branch_num - 1;
                                while (cur_branch_idx <= cur_branch_end) {
                                    if (get_portable16(m_branches[cur_branch_idx]) == val[matching_idx]) {
                                        matching_idx++;
                                        // update `cur_node_idx`.
                                        cur_node_idx = get_node_idx_by_branch_idx(
                                                cur_node_bp_idx + cur_branch_idx - (branch_end + 1)
                                                );
                                        find_branch = true;
                                        break;
                                    }
                                    cur_branch_idx++;
                                }
                                if (find_branch) break;
                                return -1;
                            }
                        } else {
                            if (get_portable16(m_labels[cur_label_idx]) == val[matching_idx]) {
                                matching_idx++;
                            } else {
                                return -1;
                            }
                        }
                        cur_label_idx++;
                    }
                }
            }

            // It seems that we can't avoid copy for returning result.
            // get `idx`-th string in string set.
            std::vector<uint8_t> operator[](size_t idx) const {
                std::vector<uint8_t> res;
                if (idx + 1 >= word_positions.size()) return res;
                uint16_t branch;
                size_t branch_no = 0;
                do {
                    if (get_portable64(word_positions[idx + 1]) < 2) continue;
                    size_t cur_label_idx = static_cast<size_t>(get_portable64(word_positions[idx + 1])) - 2;
                    size_t branch_cnt = 0;
                    while (true) {
                        if (get_portable16(m_labels[cur_label_idx]) ==
                            DefaultTreeBuilder<Lexicographic>::DELIMITER_FLAG) {
                            break;
                        }
                        if (cur_label_idx && get_portable16(m_labels[cur_label_idx - 1]) >> 8 == 1) {
                            size_t cur_branch_num = static_cast<uint8_t>(get_portable16(m_labels[cur_label_idx - 1])) + 1;
                            if (branch_no) {
                                if (branch_cnt + cur_branch_num >= branch_no) {
                                    if (branch_cnt < branch_no) {
                                        res.push_back(branch);
                                    } else {
                                        res.push_back(static_cast<uint8_t>(get_portable16(m_labels[cur_label_idx])));
                                    }
                                }
                            } else {
                                res.push_back(static_cast<uint8_t>(get_portable16(m_labels[cur_label_idx])));
                            }
                            branch_cnt += cur_branch_num;
                            if (cur_label_idx == 1) break;
                            cur_label_idx -= 2;
                            continue;
                        } else {
                            if (!branch_no || branch_cnt >= branch_no) {
                                res.push_back(static_cast<uint8_t>(get_portable16(m_labels[cur_label_idx])));
                            }
                        }
                        if (!cur_label_idx) break;
                        cur_label_idx--;
                    }
                } while (get_parent_node_branch_by_node_idx(idx, idx, branch, branch_no));
                std::reverse(res.begin(), res.end());
                res.pop_back();
                return res;
            }

            private:
                inline uint16_t get_portable16(uint16_t n) const {
                    if (!is_portable) return n;
                    return DecodeFixed16(reinterpret_cast<const char*>(&n));
                }

                inline uint32_t get_portable32(uint32_t n) const {
                    if (!is_portable) return n;
                    return DecodeFixed32(reinterpret_cast<const char*>(&n));
                }

                inline uint64_t get_portable64(uint64_t n) const {
                    if (!is_portable) return n;
                    return DecodeFixed64(reinterpret_cast<const char*>(&n));
                }
        };
    }
}
}

#endif //PATH_DECOMPOSITION_TRIE_PATH_DECOMPOSED_TRIE_H
