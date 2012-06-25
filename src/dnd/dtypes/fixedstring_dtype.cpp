//
// Copyright (C) 2011-12, Dynamic NDArray Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include <dnd/dtypes/fixedstring_dtype.hpp>
#include <dnd/kernels/string_encoding_kernels.hpp>

using namespace std;
using namespace dnd;

dnd::fixedstring_dtype::fixedstring_dtype(string_encoding_t encoding, intptr_t stringsize)
    : m_stringsize(stringsize), m_encoding(encoding)
{
    switch (encoding) {
        case string_encoding_ascii:
        case string_encoding_utf8:
            m_element_size = m_stringsize;
            m_alignment = 1;
            break;
        case string_encoding_utf16:
            m_element_size = m_stringsize * 2;
            m_alignment = 2;
            break;
        case string_encoding_utf32:
            m_element_size = m_stringsize * 4;
            m_alignment = 4;
            break;
        default:
            throw runtime_error("Unrecognized string encoding in fixedstring dtype constructor");
    }
}

void dnd::fixedstring_dtype::print_element(std::ostream& o, const char *data) const
{
    uint32_t cp;
    next_unicode_codepoint_t next_fn;
    next_fn = get_next_unicode_codepoint_function(m_encoding, assign_error_none);
    const char *data_end = data + m_element_size;

    // Print as an escaped string
    o << "\"";
    while (data < data_end) {
        cp = next_fn(data, data_end);
        if (cp == 0) {
            break;
        } else if (cp < 0x80) {
            switch (cp) {
                case '\n':
                    o << "\\n";
                    break;
                case '\r':
                    o << "\\r";
                    break;
                case '\t':
                    o << "\\t";
                    break;
                case '\\':
                    o << "\\\\";
                    break;
                case '\"':
                    o << "\\\"";
                    break;
                default:
                    if (cp < 32) {
                        o << "\\x";
                        hexadecimal_print(o, static_cast<char>(cp));
                    } else {
                        o << static_cast<char>(cp);
                    }
                    break;
            }
        } else if (cp < 0x10000) {
            o << "\\u";
            hexadecimal_print(o, static_cast<uint16_t>(cp));
        } else {
            o << "\\U";
            hexadecimal_print(o, static_cast<uint32_t>(cp));
        }
    }
    o << "\"";
}

void dnd::fixedstring_dtype::print_dtype(std::ostream& o) const
{
    o << "fixedstring<" << m_stringsize << ",encoding=" << m_encoding << ">";
}

bool dnd::fixedstring_dtype::is_lossless_assignment(const dtype& dst_dt, const dtype& src_dt) const
{
    if (dst_dt.extended() == src_dt.extended()) {
        return true;
    } else if (dst_dt.type_id() != fixedstring_type_id || src_dt.type_id() != fixedstring_type_id) {
        return false;
    } else {
        const fixedstring_dtype *dst_fs = static_cast<const fixedstring_dtype*>(dst_dt.extended());
        const fixedstring_dtype *src_fs = static_cast<const fixedstring_dtype*>(src_dt.extended());
        if (dst_fs->m_encoding == src_fs->m_encoding) {
            return dst_fs->m_stringsize >= src_fs->m_stringsize;
        } else if (dst_fs->m_stringsize < src_fs->m_stringsize) {
            return false;
        } else {
            switch (src_fs->m_encoding) {
                case string_encoding_ascii:
                    return true;
                case string_encoding_utf8:
                    return dst_fs->m_encoding == string_encoding_utf16 ||
                            dst_fs->m_encoding == string_encoding_utf32;
                case string_encoding_utf16:
                    return dst_fs->m_encoding == string_encoding_utf32;
                default:
                    return false;
            }
        }
    }
}

void dnd::fixedstring_dtype::get_dtype_assignment_kernel(const dtype& dst_dt, const dtype& src_dt,
                assign_error_mode errmode,
                unary_specialization_kernel_instance& out_kernel) const
{
    if (this == dst_dt.extended()) {
        if (src_dt.type_id() == fixedstring_type_id) {
            const fixedstring_dtype *src_fs = static_cast<const fixedstring_dtype *>(src_dt.extended());
            get_fixedstring_encoding_kernel(m_element_size, m_encoding, src_fs->m_element_size, src_fs->m_encoding,
                                    errmode, out_kernel);
        } else {
            throw runtime_error("conversions from non-string to string are not implemented");
        }
    } else {
        throw runtime_error("conversions from string to non-string are not implemented");
    }
}


bool dnd::fixedstring_dtype::operator==(const extended_dtype& rhs) const
{
    if (this == &rhs) {
        return true;
    } else if (rhs.type_id() != fixedstring_type_id) {
        return false;
    } else {
        const fixedstring_dtype *dt = static_cast<const fixedstring_dtype*>(&rhs);
        return m_encoding == dt->m_encoding && m_stringsize == dt->m_stringsize;
    }
}