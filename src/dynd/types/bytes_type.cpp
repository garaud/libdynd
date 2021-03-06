//
// Copyright (C) 2011-14 Mark Wiebe, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include <dynd/types/bytes_type.hpp>
#include <dynd/memblock/pod_memory_block.hpp>
#include <dynd/kernels/bytes_assignment_kernels.hpp>
#include <dynd/types/fixedbytes_type.hpp>
#include <dynd/exceptions.hpp>
#include <dynd/gfunc/make_callable.hpp>

#include <algorithm>

using namespace std;
using namespace dynd;

bytes_type::bytes_type(size_t alignment)
    : base_bytes_type(bytes_type_id, bytes_kind, sizeof(bytes_type_data),
                    sizeof(const char *), type_flag_scalar|type_flag_zeroinit|type_flag_blockref,
                    sizeof(bytes_type_metadata)), m_alignment(alignment)
{
    if (alignment != 1 && alignment != 2 && alignment != 4 && alignment != 8 && alignment != 16) {
        std::stringstream ss;
        ss << "Cannot make a dynd bytes type with alignment " << alignment << ", it must be a small power of two";
        throw std::runtime_error(ss.str());
    }
}

bytes_type::~bytes_type()
{
}

void bytes_type::get_bytes_range(const char **out_begin, const char**out_end,
                const char *DYND_UNUSED(metadata), const char *data) const
{
    *out_begin = reinterpret_cast<const bytes_type_data *>(data)->begin;
    *out_end = reinterpret_cast<const bytes_type_data *>(data)->end;
}

void bytes_type::set_bytes_data(const char *metadata, char *data,
                const char* bytes_begin, const char *bytes_end) const
{
    const bytes_type_metadata *md = reinterpret_cast<const bytes_type_metadata *>(metadata);
    if (md->blockref == NULL ||
                md->blockref->m_type != pod_memory_block_type) {
        throw runtime_error("assigning to a bytes data element requires that it have a pod memory block");
    }
    bytes_type_data *d = reinterpret_cast<bytes_type_data *>(data);
    if (d->begin != NULL) {
        throw runtime_error("assigning to a bytes data element requires that it be initialized to NULL");
    }
    memory_block_pod_allocator_api *allocator =
                    get_memory_block_pod_allocator_api(md->blockref);

    // Allocate the output array data, then copy it
    allocator->allocate(md->blockref, bytes_end - bytes_begin,
                get_target_alignment(), &d->begin, &d->end);
    memcpy(d->begin, bytes_begin, bytes_end - bytes_begin);
}

void bytes_type::print_data(std::ostream& o, const char *DYND_UNUSED(metadata), const char *data) const
{
    const char *begin = reinterpret_cast<const char * const *>(data)[0];
    const char *end = reinterpret_cast<const char * const *>(data)[1];

    // Print as hexadecimal
    o << "0x";
    hexadecimal_print(o, begin, end - begin);
}

void bytes_type::print_type(std::ostream& o) const
{
    o << "bytes";
    if (m_alignment != 1) {
        o << "[align=" << m_alignment << "]";
    }
}

bool bytes_type::is_unique_data_owner(const char *metadata) const
{
    const bytes_type_metadata *md = reinterpret_cast<const bytes_type_metadata *>(metadata);
    if (md->blockref != NULL &&
            (md->blockref->m_use_count != 1 ||
             md->blockref->m_type != pod_memory_block_type)) {
        return false;
    }
    return true;
}

ndt::type bytes_type::get_canonical_type() const
{
    return ndt::type(this, true);
}


void bytes_type::get_shape(intptr_t ndim, intptr_t i,
                intptr_t *out_shape, const char *DYND_UNUSED(metadata),
                const char *data) const
{
    if (data == NULL) {
        out_shape[i] = -1;
    } else {
        const bytes_type_data *d = reinterpret_cast<const bytes_type_data *>(data);
        out_shape[i] = d->end - d->begin;
    }
    if (i+1 < ndim) {
        stringstream ss;
        ss << "requested too many dimensions from type " << ndt::type(this, true);
        throw runtime_error(ss.str());
    }
}

bool bytes_type::is_lossless_assignment(const ndt::type& dst_tp, const ndt::type& src_tp) const
{
    if (dst_tp.extended() == this) {
        if (src_tp.get_kind() == bytes_kind) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

size_t bytes_type::make_assignment_kernel(
                ckernel_builder *out, size_t offset_out,
                const ndt::type& dst_tp, const char *dst_metadata,
                const ndt::type& src_tp, const char *src_metadata,
                kernel_request_t kernreq, assign_error_mode errmode,
                const eval::eval_context *ectx) const
{
    if (this == dst_tp.extended()) {
        switch (src_tp.get_type_id()) {
            case bytes_type_id: {
                return make_blockref_bytes_assignment_kernel(out, offset_out,
                                get_data_alignment(), dst_metadata,
                                src_tp.get_data_alignment(), src_metadata,
                                kernreq, ectx);
            }
            case fixedbytes_type_id: {
                return make_fixedbytes_to_blockref_bytes_assignment_kernel(out, offset_out,
                                get_data_alignment(), dst_metadata,
                                src_tp.get_data_size(), src_tp.get_data_alignment(),
                                kernreq, ectx);
            }
            default: {
                if (!src_tp.is_builtin()) {
                    src_tp.extended()->make_assignment_kernel(out, offset_out,
                                    dst_tp, dst_metadata,
                                    src_tp, src_metadata,
                                    kernreq, errmode, ectx);
                }
                break;
            }
        }
    }

    stringstream ss;
    ss << "Cannot assign from " << src_tp << " to " << dst_tp;
    throw runtime_error(ss.str());
}


bool bytes_type::operator==(const base_type& rhs) const
{
    if (this == &rhs) {
        return true;
    } else if (rhs.get_type_id() != bytes_type_id) {
        return false;
    } else {
        const bytes_type *dt = static_cast<const bytes_type*>(&rhs);
        return m_alignment == dt->m_alignment;
    }
}

void bytes_type::metadata_default_construct(char *metadata, intptr_t DYND_UNUSED(ndim), const intptr_t* DYND_UNUSED(shape)) const
{
    // Simply allocate a POD memory block
    bytes_type_metadata *md = reinterpret_cast<bytes_type_metadata *>(metadata);
    md->blockref = make_pod_memory_block().release();
}

void bytes_type::metadata_copy_construct(char *dst_metadata, const char *src_metadata, memory_block_data *embedded_reference) const
{
    // Copy the blockref, switching it to the embedded_reference if necessary
    const bytes_type_metadata *src_md = reinterpret_cast<const bytes_type_metadata *>(src_metadata);
    bytes_type_metadata *dst_md = reinterpret_cast<bytes_type_metadata *>(dst_metadata);
    dst_md->blockref = src_md->blockref ? src_md->blockref : embedded_reference;
    if (dst_md->blockref) {
        memory_block_incref(dst_md->blockref);
    }
}

void bytes_type::metadata_reset_buffers(char *DYND_UNUSED(metadata)) const
{
    throw runtime_error("TODO implement bytes_type::metadata_reset_buffers");
}

void bytes_type::metadata_finalize_buffers(char *metadata) const
{
    bytes_type_metadata *md = reinterpret_cast<bytes_type_metadata *>(metadata);
    if (md->blockref != NULL) {
        // Finalize the memory block
        memory_block_pod_allocator_api *allocator = get_memory_block_pod_allocator_api(md->blockref);
        if (allocator != NULL) {
            allocator->finalize(md->blockref);
        }
    }
}

void bytes_type::metadata_destruct(char *metadata) const
{
    bytes_type_metadata *md = reinterpret_cast<bytes_type_metadata *>(metadata);
    if (md->blockref) {
        memory_block_decref(md->blockref);
    }
}

void bytes_type::metadata_debug_print(const char *metadata, std::ostream& o, const std::string& indent) const
{
    const bytes_type_metadata *md = reinterpret_cast<const bytes_type_metadata *>(metadata);
    o << indent << "bytes metadata\n";
    memory_block_debug_print(md->blockref, o, indent + " ");
}

static size_t property_get_target_alignment(const ndt::type& dt) {
    const bytes_type *pd = static_cast<const bytes_type *>(dt.extended());
    return pd->get_target_alignment();
}

static pair<string, gfunc::callable> type_properties[] = {
    pair<string, gfunc::callable>("target_alignment", gfunc::make_callable(&property_get_target_alignment, "self"))
};

void bytes_type::get_dynamic_type_properties(
                const std::pair<std::string, gfunc::callable> **out_properties,
                size_t *out_count) const
{
    *out_properties = type_properties;
    *out_count = sizeof(type_properties) / sizeof(type_properties[0]);
}
