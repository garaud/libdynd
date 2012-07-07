//
// Copyright (C) 2011-12, Dynamic NDArray Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include <vector>

#include <dnd/memblock/pod_memory_block.hpp>

using namespace std;
using namespace dnd;

namespace {
    struct pod_memory_block {
        /** Every memory block object needs this at the front */
        memory_block_data m_mbd;
        intptr_t m_total_allocated_capacity;
        /** References to other memory_blocks */
        vector<memory_block_data *> m_blockrefs;
        /** The malloc'd memory */
        vector<char *> m_memory_handles;
        /** The current malloc'd memory being doled out */
        char *m_memory_begin, *m_memory_current, *m_memory_end;

        /**
         * Allocates some new memory from which to dole out
         * more. Adds it to the memory handles vector.
         */
        void append_memory(intptr_t capacity_bytes)
        {
            m_memory_handles.push_back(NULL);
            m_memory_begin = reinterpret_cast<char *>(malloc(capacity_bytes));
            m_memory_handles.back() = m_memory_begin;
            if (m_memory_begin == NULL) {
                m_memory_handles.pop_back();
                throw bad_alloc();
            }
            m_memory_current = m_memory_begin;
            m_memory_end = m_memory_current + capacity_bytes;
            m_total_allocated_capacity += capacity_bytes;
        }

        pod_memory_block(intptr_t initial_capacity_bytes)
            : m_mbd(1, pod_memory_block_type), m_total_allocated_capacity(0),
                    m_blockrefs(0), m_memory_handles()
        {
            append_memory(initial_capacity_bytes);
        }

        pod_memory_block(memory_block_ptr *blockrefs_begin, memory_block_ptr *blockrefs_end, intptr_t initial_capacity_bytes)
            : m_mbd(1, pod_memory_block_type), m_total_allocated_capacity(0),
                    m_blockrefs(blockrefs_end - blockrefs_begin), m_memory_handles()
        {
            for (int i = 0; i < blockrefs_end - blockrefs_begin; ++i) {
                m_blockrefs[i] = blockrefs_begin[i].release();
            }
            append_memory(initial_capacity_bytes);
        }

        pod_memory_block(memory_block_data **blockrefs_begin, memory_block_data **blockrefs_end, intptr_t initial_capacity_bytes)
            : m_mbd(1, pod_memory_block_type), m_total_allocated_capacity(0),
                    m_blockrefs(blockrefs_begin, blockrefs_end), m_memory_handles()
        {
            append_memory(initial_capacity_bytes);
        }

        ~pod_memory_block()
        {
            for (size_t i = 0, i_end = m_blockrefs.size(); i != i_end; ++i) {
                memory_block_decref(m_blockrefs[i]);
            }
            for (size_t i = 0, i_end = m_memory_handles.size(); i != i_end; ++i) {
                free(m_memory_handles[i]);
            }
        }
    };
} // anonymous namespace

memory_block_ptr dnd::make_pod_memory_block(intptr_t initial_capacity_bytes)
{
    pod_memory_block *pmb = new pod_memory_block(initial_capacity_bytes);
    return memory_block_ptr(reinterpret_cast<memory_block_data *>(pmb), false);
}

memory_block_ptr dnd::make_pod_memory_block(memory_block_ptr *blockrefs_begin, memory_block_ptr *blockrefs_end,
                        intptr_t initial_capacity_bytes)
{
    pod_memory_block *pmb = new pod_memory_block(blockrefs_begin, blockrefs_end, initial_capacity_bytes);
    return memory_block_ptr(reinterpret_cast<memory_block_data *>(pmb), false);
}

memory_block_ptr make_pod_memory_block(memory_block_data **blockrefs_begin, memory_block_data **blockrefs_end,
                        intptr_t initial_capacity_bytes)
{
    pod_memory_block *pmb = new pod_memory_block(blockrefs_begin, blockrefs_end, initial_capacity_bytes);
    return memory_block_ptr(reinterpret_cast<memory_block_data *>(pmb), false);
}

namespace dnd { namespace detail {

void free_pod_memory_block(memory_block_data *memblock)
{
    pod_memory_block *emb = reinterpret_cast<pod_memory_block *>(memblock);
    delete emb;
}

static void allocate(memory_block_data *self, intptr_t size_bytes, intptr_t alignment, char **out_begin, char **out_end)
{
    // Allocate new POD memory of the requested size and alignment
    pod_memory_block *emb = reinterpret_cast<pod_memory_block *>(self);
    char *begin = reinterpret_cast<char *>(
                    (reinterpret_cast<uintptr_t>(emb->m_memory_current) + alignment - 1) & ~(alignment - 1));
    char *end = begin + size_bytes;
    if (end > emb->m_memory_end) {
        // Shrink the malloc'd memory to fit how much we used
        realloc(emb->m_memory_begin, emb->m_memory_current - emb->m_memory_begin);
        // Allocate memory to double the amount used so far, or the requested size, whichever is larger
        // NOTE: We're assuming malloc produces memory which has good enough alignment for anything
        emb->append_memory(max(emb->m_total_allocated_capacity, size_bytes));
        begin = emb->m_memory_begin;
        end = begin + size_bytes;
    }

    // Return the allocated memory
    *out_begin = begin;
    *out_end = end;
}

static void resize(memory_block_data *self, intptr_t size_bytes, char **inout_begin, char **inout_end)
{
    // Resizes previously allocated POD memory to the requested size
    pod_memory_block *emb = reinterpret_cast<pod_memory_block *>(self);
    if (*inout_end != emb->m_memory_current) {
        // Simple sanity check
        throw runtime_error("pod_memory_block resize must be called only using the most recently allocated memory");
    }
    char *end = *inout_begin + size_bytes;
    if (end <= emb->m_memory_end) {
        // If it fits, just adjust the current allocation point
        emb->m_memory_current = end;
        *inout_end = end;
    } else {
        // If it doesn't fit, need to copy to newly malloc'd memory
        char *old_begin = emb->m_memory_begin, *old_current = *inout_begin;
        // Allocate memory to double the amount used so far, or the requested size, whichever is larger
        // NOTE: We're assuming malloc produces memory which has good enough alignment for anything
        emb->append_memory(max(emb->m_total_allocated_capacity, size_bytes));
        memcpy(emb->m_memory_begin, *inout_begin, *inout_end - *inout_begin);
        *inout_begin = emb->m_memory_begin;
        *inout_end = emb->m_memory_begin + size_bytes;
        // Shrink the previous malloc'd memory to fit how much was used
        realloc(old_begin, old_current - old_begin);
    }
}

memory_block_pod_allocator_api pod_memory_block_allocator_api = {
    &allocate,
    &resize
};

}} // namespace dnd::detail