//
// Copyright (C) 2011-13 Mark Wiebe, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include <algorithm>

#include <dynd/types/fixedbytes_type.hpp>
#include <dynd/kernels/assignment_kernels.hpp>
#include <dynd/shape_tools.hpp>
#include <dynd/exceptions.hpp>

using namespace std;
using namespace dynd;

fixedbytes_type::fixedbytes_type(intptr_t data_size, intptr_t data_alignment)
    : base_bytes_type(fixedbytes_type_id, bytes_kind, data_size,
                    data_alignment, type_flag_scalar, 0)
{
    if (data_alignment > data_size) {
        std::stringstream ss;
        ss << "Cannot make a fixedbytes<" << data_size << ",";
        ss << data_alignment << "> type, its alignment is greater than its size";
        throw std::runtime_error(ss.str());
    }
    if (data_alignment != 1 && data_alignment != 2 && data_alignment != 4 && data_alignment != 8 && data_alignment != 16) {
        std::stringstream ss;
        ss << "Cannot make a fixedbytes<" << data_size << ",";
        ss << data_alignment << "> type, its alignment is not a small power of two";
        throw std::runtime_error(ss.str());
    }
    if ((data_size&(data_alignment-1)) != 0) {
        std::stringstream ss;
        ss << "Cannot make a fixedbytes<" << data_size << ",";
        ss<< data_alignment << "> type, its alignment does not divide into its element size";
        throw std::runtime_error(ss.str());
    }
}

fixedbytes_type::~fixedbytes_type()
{
}

void fixedbytes_type::get_bytes_range(const char **out_begin, const char**out_end,
                const char *DYND_UNUSED(metadata), const char *data) const
{
    *out_begin = data;
    *out_end = data + get_data_size();
}

void fixedbytes_type::print_data(std::ostream& o, const char *DYND_UNUSED(metadata), const char *data) const
{
    o << "0x";
    hexadecimal_print(o, data, get_data_size());
}

void fixedbytes_type::print_type(std::ostream& o) const
{
    o << "fixedbytes<" << get_data_size() << "," << get_data_alignment() << ">";
}

bool fixedbytes_type::is_lossless_assignment(const ndt::type& dst_tp, const ndt::type& src_tp) const
{
    if (dst_tp.extended() == this) {
        if (src_tp.extended() == this) {
            return true;
        } else if (src_tp.get_type_id() == fixedbytes_type_id) {
            const fixedbytes_type *src_fs = static_cast<const fixedbytes_type*>(src_tp.extended());
            return get_data_size() == src_fs->get_data_size();
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool fixedbytes_type::operator==(const base_type& rhs) const
{
    if (this == &rhs) {
        return true;
    } else if (rhs.get_type_id() != fixedbytes_type_id) {
        return false;
    } else {
        const fixedbytes_type *dt = static_cast<const fixedbytes_type*>(&rhs);
        return get_data_size() == dt->get_data_size() && get_data_alignment() == dt->get_data_alignment();
    }
}

size_t fixedbytes_type::make_assignment_kernel(
                hierarchical_kernel *out, size_t offset_out,
                const ndt::type& dst_tp, const char *dst_metadata,
                const ndt::type& src_tp, const char *src_metadata,
                kernel_request_t kernreq, assign_error_mode errmode,
                const eval::eval_context *ectx) const
{
    if (this == dst_tp.extended()) {
        switch (src_tp.get_type_id()) {
            case fixedbytes_type_id: {
                const fixedbytes_type *src_fs = static_cast<const fixedbytes_type *>(src_tp.extended());
                if (get_data_size() != src_fs->get_data_size()) {
                    throw runtime_error("cannot assign to a fixedbytes type of a different size");
                }
                return ::make_pod_typed_data_assignment_kernel(out, offset_out,
                                get_data_size(), std::min(get_data_alignment(), src_fs->get_data_alignment()),
                                kernreq);
            }
            default: {
                return src_tp.extended()->make_assignment_kernel(out, offset_out,
                                dst_tp, dst_metadata,
                                src_tp, src_metadata,
                                kernreq, errmode, ectx);
            }
        }
    } else {
        stringstream ss;
        ss << "Cannot assign from " << src_tp << " to " << dst_tp;
        throw runtime_error(ss.str());
    }
}

