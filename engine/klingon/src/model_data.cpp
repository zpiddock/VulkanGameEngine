#include "klingon/model_data.hpp"
#include <algorithm>
#include <functional>

namespace klingon {

auto ModelData::get_node_world_matrix(uint32_t node_index, const glm::mat4& model_root_matrix) const -> glm::mat4 {
    if (node_index >= nodes.size()) {
        return model_root_matrix;
    }

    // Build transform chain from root to this node
    std::vector<uint32_t> node_chain;

    // Find path from root to target node
    std::function<bool(uint32_t, uint32_t)> find_path = [&](uint32_t current, uint32_t target) -> bool {
        if (current == target) {
            node_chain.push_back(current);
            return true;
        }

        const auto& current_node = nodes[current];
        for (uint32_t child_idx : current_node.children) {
            if (find_path(child_idx, target)) {
                node_chain.push_back(current);
                return true;
            }
        }

        return false;
    };

    find_path(root_node_index, node_index);

    // Reverse to get root-to-target order
    std::reverse(node_chain.begin(), node_chain.end());

    // Accumulate transform matrices
    glm::mat4 result = model_root_matrix;
    for (uint32_t idx : node_chain) {
        result = result * nodes[idx].transform.mat4();
    }

    return result;
}

} // namespace klingon
