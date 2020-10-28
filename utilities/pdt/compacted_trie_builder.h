//
// Created by Dim Dew on 2020-07-30.
//

#ifndef PATH_DECOMPOSITION_TRIE_COMPACTED_TRIE_BUILDER_H
#define PATH_DECOMPOSITION_TRIE_COMPACTED_TRIE_BUILDER_H

#include <vector>
#include <string>
#include <algorithm>
#include <memory>

namespace rocksdb {
namespace succinct {
    namespace trie {
        template <typename TreeBuilder>
        struct compacted_trie_builder {
        public:
            compacted_trie_builder(const compacted_trie_builder&) = delete;

            compacted_trie_builder &operator=(const compacted_trie_builder&) = delete;

            compacted_trie_builder(TreeBuilder& builder_)
                : is_finish_(false), builder(builder_) {
                assert(node_stack.empty() && last_string.empty());
            }

            void append(std::vector<uint8_t>& raw_bytes) {
                std::vector<uint16_t> bytes(raw_bytes.begin(), raw_bytes.end());
                bytes.push_back(1024);

                assert(!is_finish_);
                if (bytes.empty()) return;

                if (node_stack.empty()) {
                    // first bytes
                    last_string.assign(bytes.begin(), bytes.end());
                    node_stack.push_back(node(0, last_string.size()));
                } else {
                    size_t min_len = std::min(bytes.size(), last_string.size());
                    auto match_res =
                            std::mismatch(
                                    bytes.begin(),
                                    bytes.begin() + min_len,
                                    last_string.begin());

                    // Prefix-free and non-duplicate
                    assert(match_res.first != bytes.end() &&
                           match_res.second != last_string.end());
                    // Sorted
                    assert(*match_res.second == 1024 || 
                          (*match_res.first != 1024 && *match_res.first > *match_res.second));

                    size_t mismatch_idx = match_res.first - bytes.begin();
                    size_t split_node_idx = 0;
                    // find the node to split
                    while (mismatch_idx > node_stack[split_node_idx].get_end()) {
                        assert(split_node_idx < node_stack.size());
                        ++split_node_idx;
                    }
                    node& split_node = node_stack[split_node_idx];
                    assert(mismatch_idx >= split_node.path_len &&
                           mismatch_idx <= split_node.get_end());
                    // pop the node after split_node in node_stack
                    for (size_t idx = node_stack.size() - 1; idx > split_node_idx; idx--) {
                        node& child = node_stack[idx];
                        typename TreeBuilder::representation_type subtrie =
                                builder.node(child.children, &last_string[0], child.path_len, child.skip);
                        uint16_t branch_byte = last_string[child.path_len - 1];
                        node_stack[idx - 1].children.push_back(std::make_pair(branch_byte, subtrie));
                    }
                    node_stack.resize(split_node_idx + 1);

                    // if the current string splits the skip, split the current node
                    if (mismatch_idx < split_node.path_len + split_node.skip) {
                        typename TreeBuilder::representation_type subtrie =
                                builder.node(split_node.children, &last_string[0],
                                        mismatch_idx + 1,
                                        split_node.path_len + split_node.skip - mismatch_idx - 1);
                        uint16_t branching_char = last_string[mismatch_idx];
                        split_node.children.clear();
                        split_node.children.push_back(std::make_pair(branching_char, subtrie));
                        split_node.skip = mismatch_idx - split_node.path_len;
                    }

                    assert(split_node.path_len + split_node.skip == mismatch_idx);
                    // open a new leaf with the current suffix
                    node_stack.push_back(node(mismatch_idx + 1, bytes.size() - mismatch_idx - 1));

                    // copy the current string (iter could be invalid in next iteration)
                    last_string.assign(bytes.begin(), bytes.end());
                }
            }

            void finish() {
                assert(!is_finish_);
                assert(!node_stack.empty());

                // close the remaining path
                for (size_t node_idx = node_stack.size() - 1; node_idx > 0; --node_idx) {
                    node& child = node_stack[node_idx];
                    typename TreeBuilder::representation_type subtrie =
                            builder.node(child.children, &last_string[0], child.path_len, child.skip);
                    uint8_t branching_char = last_string[child.path_len - 1];
                    node_stack[node_idx - 1].children.push_back(std::make_pair(branching_char, subtrie));
                }

                typename TreeBuilder::representation_type root =
                        builder.node(node_stack[0].children, &last_string[0],
                                      node_stack[0].path_len, node_stack[0].skip);
                builder.root(root);
                node_stack.clear();

                is_finish_ = true;
            }

            bool is_finish() const {
                return is_finish_;
            }

            typename TreeBuilder::representation_type get_root() {
                return builder.get_root();
            }

        private:
            struct node {
                node()
                        : skip(-1)
                {}

                node(size_t path_len_, size_t skip_)
                        : path_len(path_len_)
                        , skip(skip_)
                {}

                size_t get_end() { return path_len + skip; }

                size_t path_len;
                size_t skip;

                typedef std::pair<uint16_t, typename TreeBuilder::representation_type> subtrie;
                std::vector<subtrie> children;
            };

            bool is_finish_;            // Is building process finish?

            TreeBuilder& builder;
            std::vector<node> node_stack;
            std::vector<uint16_t> last_string;

        };
    }
}
}

#endif //PATH_DECOMPOSITION_TRIE_COMPACTED_TRIE_BUILDER_H
