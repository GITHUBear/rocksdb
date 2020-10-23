//
// Created by Dim Dew on 2020-08-07.
//

#ifndef PATH_DECOMPOSITION_TRIE_DEFAULT_TREE_BUILDER_H
#define PATH_DECOMPOSITION_TRIE_DEFAULT_TREE_BUILDER_H

#include <vector>
#include <memory>
#include "bit_vector.h"

namespace rocksdb {
namespace succinct {
    template <bool Lexicographic = false>
    class DefaultTreeBuilder {
    public:
        // node label is encoded by uint16_t:
        //
        //        special_char_flag (uint8_t: 0/1/2) + char/branching number (uint8_t)
        //
        static const size_t SPECIAL_CHAR_FLAG = 256;
        static const size_t DELIMITER_FLAG = 512;
        static const size_t WORD_EOF = 1024;

        struct subtree {
            // infos for decomposition path of the subtree
            std::vector<uint16_t> m_decomposition_path_label;
            std::vector<uint16_t> m_decomposition_branches;
            // infos for other path of subtree
            std::vector<uint16_t> m_labels;      // `L` in paper
            std::vector<uint16_t> m_branches;     // `B` in paper
            BitVectorBuilder m_bp;               // `BP` in paper
            // m_num_leaves is used to check if m_decomposition_branches & m_bp is right.
            // TODO: remove m_num_leaves if it is checked
            size_t m_num_leaves;

            subtree() : m_num_leaves(1) {}

            size_t size() const {
                // each decomposition branch has at least one leaf
                // TODO: Test following modification
                // Origin:
                //     return (m_bp.size() + 1) / 2 + m_decomposition_branches.size();
                // now:
                //     return (m_bp.size() + 1 + m_decomposition_branches.size() + 1) / 2;
                size_t size_by_bp_branches = (m_bp.size() + m_decomposition_branches.size() + 2) / 2;
                assert(m_num_leaves == size_by_bp_branches);
                return size_by_bp_branches;
            }

            void append_to(subtree& tree)
            {
                tree.m_num_leaves += m_num_leaves;

                if (m_decomposition_path_label.size()) {
                    tree.m_labels.insert(tree.m_labels.end(),
                            m_decomposition_path_label.rbegin(), m_decomposition_path_label.rend());
                } else {
                    // we need this to obtain the right number of strings in the pool, and we have to special-case 0s anyway
                    tree.m_labels.push_back(0);
                }
                // TODO: In ot/path_decomposed_tries every byte sequence is ended with "\0"?
                // assert(tree.m_labels.back() == 0);

                tree.m_bp.one_extend(m_decomposition_branches.size());
                tree.m_bp.push_back(0);

                tree.m_branches.insert(tree.m_branches.end(),
                        m_decomposition_branches.rbegin(), m_decomposition_branches.rend());

                tree.m_bp.append(m_bp);
                // Deconstruct this m_bp
                BitVectorBuilder().swap(m_bp);
                tree.m_branches.insert(tree.m_branches.end(), m_branches.begin(), m_branches.end());
                std::vector<uint16_t>().swap(m_branches);
                tree.m_labels.insert(tree.m_labels.end(), m_labels.begin(), m_labels.end());
                std::vector<uint16_t>().swap(m_labels);
            }
        };

        typedef std::shared_ptr<subtree> representation_type;
        typedef std::vector<std::pair<uint16_t, representation_type>> children_type;

        representation_type node(
                children_type& children, const uint16_t* buf,
                size_t offset, size_t skip) {
            representation_type ret;

            if (children.size()) {
                // prefix-free
                assert(children.size() > 1);
                // find heavy child
                size_t largest_child = -1;
                if (Lexicographic) {
                    largest_child = 0;
                } else {
                    size_t largest_child_size = 0;

                    for (size_t i = 0; i < children.size(); ++i) {
                        if (i == 0 || children[i].second->size() > largest_child_size) {
                            largest_child = i;
                            largest_child_size = children[i].second->size();
                        }
                    }
                }
                assert(largest_child != static_cast<size_t>(-1));
                // Pick heavy subtrie from compacted trie.
                children[largest_child].second.swap(ret);
                size_t n_branches = children.size() - 1;
                assert(n_branches > 0);
                assert(n_branches <= std::numeric_limits<uint16_t>::max());
                ret->m_decomposition_path_label.push_back(children[largest_child].first);
                ret->m_decomposition_path_label.push_back(uint16_t(SPECIAL_CHAR_FLAG + n_branches - 1));

                // append children (note that branching chars are reversed)
                for (size_t i = 0; i < children.size(); ++i) {
                    if (i != largest_child) {
                        ret->m_decomposition_branches.push_back(children[i].first);
                        children[i].second->append_to(*ret);
                    }
                }
            } else {
                ret = std::make_shared<subtree>();
            }

            // append in reverse order
            if (!children.size()) {
                // Leaf -- We add a special delimiter.
                ret->m_decomposition_path_label.push_back(uint16_t(DELIMITER_FLAG));
            }
            for (size_t i = offset + skip - 1; i != offset - 1; --i) {
                ret->m_decomposition_path_label.push_back(buf[i]);
            }

            return ret;
        }

        void root(representation_type& root_node)
        {
            representation_type ret = std::make_shared<subtree>();

            ret->m_bp.reserve(
                    root_node->m_bp.size() + root_node->m_decomposition_branches.size() + 2);
            ret->m_bp.push_back(1); // DFUDS fake root
            root_node->append_to(*ret);
            assert(ret->m_bp.size() % 2 == 0);

            m_root_node = ret;
        }

        representation_type get_root() const
        {
            return m_root_node;
        }
    private:
        representation_type m_root_node;
    };
}
}

#endif //PATH_DECOMPOSITION_TRIE_DEFAULT_TREE_BUILDER_H
