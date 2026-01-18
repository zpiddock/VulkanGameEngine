#include "batleth/render_graph_pass.hpp"

namespace batleth {
    auto compute_resource_state(
        const ResourceAccess &access,
        std::uint32_t queue_family
    ) -> ResourceState {
        ResourceState state = usage_to_state(access.usage, queue_family);

        // Apply stage override if specified
        if (access.stage_override != 0) {
            state.stage_mask = access.stage_override;
        }

        return state;
    }

    auto has_dependency(
        const ResourceAccess &first,
        const ResourceAccess &second
    ) -> bool {
        // Same resource?
        if (first.handle != second.handle) {
            return false;
        }

        bool first_writes = is_write_usage(first.usage);
        bool second_writes = is_write_usage(second.usage);

        // RAW (Read-After-Write): first writes, second reads
        // WAR (Write-After-Read): first reads, second writes
        // WAW (Write-After-Write): both write
        // RAR (Read-After-Read): no dependency

        return first_writes || second_writes;
    }
} // namespace batleth