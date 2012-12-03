//
// Copyright (C) 2011-12, Dynamic NDArray Developers
// BSD 2-Clause License, see LICENSE.txt
//

#ifndef _DYND__FIXEDSTRUCT_DTYPE_HPP_
#define _DYND__FIXEDSTRUCT_DTYPE_HPP_

#include <vector>
#include <string>

#include <dynd/dtype.hpp>
#include <dynd/memblock/memory_block.hpp>

namespace dynd {

class fixedstruct_dtype : public extended_dtype {
    std::vector<dtype> m_field_types;
    std::vector<std::string> m_field_names;
    std::vector<size_t> m_data_offsets;
    std::vector<size_t> m_metadata_offsets;
    size_t m_element_size;
    size_t m_metadata_size;
    dtype_memory_management_t m_memory_management;
    unsigned char m_alignment;
public:
    fixedstruct_dtype(const std::vector<dtype>& field_types, const std::vector<std::string>& field_names);

    type_id_t get_type_id() const {
        return fixedstruct_type_id;
    }
    dtype_kind_t get_kind() const {
        return composite_kind;
    }
    // Expose the storage traits here
    size_t get_alignment() const {
        return m_alignment;
    }
    size_t get_element_size() const {
        return m_element_size;
    }
    size_t get_default_element_size(int DYND_UNUSED(ndim), const intptr_t *DYND_UNUSED(shape)) const {
        return m_element_size;
    }

    const std::vector<dtype>& get_field_types() const {
        return m_field_types;
    }

    const std::vector<std::string>& get_field_names() const {
        return m_field_names;
    }

    const std::vector<size_t>& get_data_offsets() const {
        return m_data_offsets;
    }

    const std::vector<size_t>& get_metadata_offsets() const {
        return m_metadata_offsets;
    }

    void print_element(std::ostream& o, const char *metadata, const char *data) const;

    void print_dtype(std::ostream& o) const;

    // This is about the native storage, buffering code needs to check whether
    // the value_dtype is an object type separately.
    dtype_memory_management_t get_memory_management() const {
        return m_memory_management;
    }

    bool is_scalar() const;
    bool is_expression() const;
    dtype with_transformed_scalar_types(dtype_transform_fn_t transform_fn, const void *extra) const;
    dtype get_canonical_dtype() const;

    dtype apply_linear_index(int nindices, const irange *indices, int current_i, const dtype& root_dt) const;
    intptr_t apply_linear_index(int nindices, const irange *indices, char *data, const char *metadata,
                    const dtype& result_dtype, char *out_metadata, int current_i, const dtype& root_dt) const;

    intptr_t get_dim_size(const char *data, const char *metadata) const;
    void get_shape(int i, intptr_t *out_shape) const;
    intptr_t get_representative_stride(const char *metadata) const;

    bool is_lossless_assignment(const dtype& dst_dt, const dtype& src_dt) const;

    void get_single_compare_kernel(single_compare_kernel_instance& out_kernel) const;

    void get_dtype_assignment_kernel(const dtype& dst_dt, const dtype& src_dt,
                    assign_error_mode errmode,
                    unary_specialization_kernel_instance& out_kernel) const;

    bool operator==(const extended_dtype& rhs) const;

    size_t get_metadata_size() const;
    void metadata_default_construct(char *metadata, int ndim, const intptr_t* shape) const;
    void metadata_copy_construct(char *dst_metadata, const char *src_metadata, memory_block_data *embedded_reference) const;
    void metadata_destruct(char *metadata) const;
    void metadata_debug_print(const char *metadata, std::ostream& o, const std::string& indent) const;

    void foreach_leading(char *data, const char *metadata, foreach_fn_t callback, void *callback_data) const;
}; // class fixedstruct_dtype

/** Makes a tuple dtype with the specified fields, using the standard layout */
inline dtype make_fixedstruct_dtype(const std::vector<dtype>& field_types, const std::vector<std::string>& field_names) {
    return dtype(new fixedstruct_dtype(field_types, field_names));
}

/** Makes a tuple dtype with the specified fields, using the standard layout */
inline dtype make_fixedstruct_dtype(const dtype& dt0, const std::string& name0)
{
    std::vector<dtype> fields;
    std::vector<std::string> field_names;
    fields.push_back(dt0);
    field_names.push_back(name0);
    return make_fixedstruct_dtype(fields, field_names);
}

/** Makes a tuple dtype with the specified fields, using the standard layout */
inline dtype make_fixedstruct_dtype(const dtype& dt0, const std::string& name0, const dtype& dt1, const std::string& name1)
{
    std::vector<dtype> fields;
    std::vector<std::string> field_names;
    fields.push_back(dt0);
    fields.push_back(dt1);
    field_names.push_back(name0);
    field_names.push_back(name1);
    return make_fixedstruct_dtype(fields, field_names);
}

/** Makes a tuple dtype with the specified fields, using the standard layout */
inline dtype make_fixedstruct_dtype(const dtype& dt0, const std::string& name0, const dtype& dt1, const std::string& name1, const dtype& dt2, const std::string& name2)
{
    std::vector<dtype> fields;
    std::vector<std::string> field_names;
    fields.push_back(dt0);
    fields.push_back(dt1);
    fields.push_back(dt2);
    field_names.push_back(name0);
    field_names.push_back(name1);
    field_names.push_back(name2);
    return make_fixedstruct_dtype(fields, field_names);
}

} // namespace dynd

#endif // _DYND__FIXEDSTRUCT_DTYPE_HPP_