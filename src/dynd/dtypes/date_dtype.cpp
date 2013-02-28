//
// Copyright (C) 2011-13, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include <time.h>

#include <cerrno>
#include <algorithm>

#include <dynd/dtypes/date_dtype.hpp>
#include <dynd/dtypes/property_dtype.hpp>
#include <dynd/dtypes/fixedstruct_dtype.hpp>
#include <dynd/dtypes/string_dtype.hpp>
#include <dynd/kernels/string_assignment_kernels.hpp>
#include <dynd/kernels/assignment_kernels.hpp>
#include <dynd/exceptions.hpp>
#include <dynd/gfunc/make_callable.hpp>
#include <dynd/kernels/date_assignment_kernels.hpp>
#include <dynd/ndobject_iter.hpp>

#include <datetime_strings.h>
#include <datetime_localtime.h>

using namespace std;
using namespace dynd;

date_dtype::date_dtype()
    : base_dtype(date_type_id, datetime_kind, 4, 4, dtype_flag_scalar, 0, 0)
{
}

date_dtype::~date_dtype()
{
}

const dtype date_dtype::default_struct_dtype =
        make_fixedstruct_dtype(
            make_dtype<int32_t>(), "year",
            make_dtype<int16_t>(), "month",
            make_dtype<int16_t>(), "day");
namespace {
    struct default_date_struct_t {
        int32_t year;
        int16_t month;
        int16_t day;
    };
}


void date_dtype::set_ymd(const char *DYND_UNUSED(metadata), char *data,
                assign_error_mode errmode, int32_t year, int32_t month, int32_t day) const
{
    if (errmode != assign_error_none && !datetime::is_valid_ymd(year, month, day)) {
        stringstream ss;
        ss << "invalid input year/month/day " << year << "/" << month << "/" << day;
        throw runtime_error(ss.str());
    }

    *reinterpret_cast<int32_t *>(data) = datetime::ymd_to_days(year, month, day);
}

void date_dtype::set_utf8_string(const char *DYND_UNUSED(metadata),
                char *data, assign_error_mode errmode, const std::string& utf8_str) const
{
    datetime::datetime_conversion_rule_t casting;
    switch (errmode) {
        case assign_error_fractional:
        case assign_error_inexact:
            casting = datetime::datetime_conversion_strict;
            break;
        default:
            casting = datetime::datetime_conversion_relaxed;
            break;
    }
    *reinterpret_cast<int32_t *>(data) = datetime::parse_iso_8601_date(
                            utf8_str, datetime::datetime_unit_day, casting);
}


void date_dtype::get_ymd(const char *DYND_UNUSED(metadata), const char *data,
                int32_t &out_year, int32_t &out_month, int32_t &out_day) const
{
    datetime::date_ymd ymd;
    datetime::days_to_ymd(*reinterpret_cast<const int32_t *>(data), ymd);
    out_year = ymd.year;
    out_month = ymd.month;
    out_day = ymd.day;
}

void date_dtype::print_data(std::ostream& o, const char *DYND_UNUSED(metadata), const char *data) const
{
    int32_t value = *reinterpret_cast<const int32_t *>(data);
    o << datetime::make_iso_8601_date(value, datetime::datetime_unit_day);
}

void date_dtype::print_dtype(std::ostream& o) const
{
    o << "date";
}

bool date_dtype::is_lossless_assignment(const dtype& dst_dt, const dtype& src_dt) const
{
    if (dst_dt.extended() == this) {
        if (src_dt.extended() == this) {
            return true;
        } else if (src_dt.get_type_id() == date_type_id) {
            // There is only one possibility for the date dtype (TODO: timezones!)
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

bool date_dtype::operator==(const base_dtype& rhs) const
{
    if (this == &rhs) {
        return true;
    } else if (rhs.get_type_id() != date_type_id) {
        return false;
    } else {
        // There is only one possibility for the date dtype (TODO: timezones!)
        return true;
    }
}

size_t date_dtype::make_assignment_kernel(
                assignment_kernel *out, size_t offset_out,
                const dtype& dst_dt, const char *dst_metadata,
                const dtype& src_dt, const char *src_metadata,
                kernel_request_t kernreq, assign_error_mode errmode,
                const eval::eval_context *ectx) const
{
    if (this == dst_dt.extended()) {
        if (src_dt.get_kind() == string_kind) {
            // Assignment from strings
            return make_string_to_date_assignment_kernel(out, offset_out,
                            src_dt, src_metadata,
                            kernreq, errmode, ectx);
        } else if (src_dt.get_kind() == struct_kind) {
            // Convert to struct using the "struct" property
            return ::make_assignment_kernel(out, offset_out,
                make_property_dtype(dst_dt, "struct"), dst_metadata,
                src_dt, src_metadata,
                kernreq, errmode, ectx);
        } else if (!src_dt.is_builtin()) {
            return src_dt.extended()->make_assignment_kernel(out, offset_out,
                            dst_dt, dst_metadata,
                            src_dt, src_metadata,
                            kernreq, errmode, ectx);
        }
    } else {
        if (dst_dt.get_kind() == string_kind) {
            // Assignment to strings
            return make_date_to_string_assignment_kernel(out, offset_out,
                            dst_dt, dst_metadata,
                            kernreq, errmode, ectx);
        } else if (dst_dt.get_kind() == struct_kind) {
            // Convert to struct using the "struct" property
            return ::make_assignment_kernel(out, offset_out,
                dst_dt, dst_metadata,
                make_property_dtype(src_dt, "struct"), src_metadata,
                kernreq, errmode, ectx);
        }
        // TODO
    }

    stringstream ss;
    ss << "Cannot assign from " << src_dt << " to " << dst_dt;
    throw runtime_error(ss.str());
}


///////// properties on the dtype

//static pair<string, gfunc::callable> date_dtype_properties[] = {
//};

void date_dtype::get_dynamic_dtype_properties(const std::pair<std::string, gfunc::callable> **out_properties, size_t *out_count) const
{
    *out_properties = NULL; //date_dtype_properties;
    *out_count = 0; //sizeof(date_dtype_properties) / sizeof(date_dtype_properties[0]);
}

///////// functions on the dtype

static ndobject function_dtype_today(const dtype& dt) {
    datetime::date_ymd ymd;
    datetime::fill_current_local_date(&ymd);
    ndobject result(dt);
    *reinterpret_cast<int32_t *>(result.get_readwrite_originptr()) = datetime::ymd_to_days(ymd);
    // Make the result immutable (we own the only reference to the data at this point)
    result.flag_as_immutable();
    return result;
}

static ndobject function_dtype_construct(const dtype& DYND_UNUSED(dt), const ndobject& year, const ndobject& month, const ndobject& day)
{
    // TODO proper buffering
    ndobject year_as_int = year.cast_udtype(make_dtype<int32_t>()).eval();
    ndobject month_as_int = month.cast_udtype(make_dtype<int32_t>()).eval();
    ndobject day_as_int = day.cast_udtype(make_dtype<int32_t>()).eval();
    ndobject result;

    ndobject_iter<1,3> iter(make_date_dtype(), result, year_as_int, month_as_int, day_as_int);
    if (!iter.empty()) {
        datetime::date_ymd ymd;
        do {
            ymd.year = *reinterpret_cast<const int32_t *>(iter.data<1>());
            ymd.month = *reinterpret_cast<const int32_t *>(iter.data<2>());
            ymd.day = *reinterpret_cast<const int32_t *>(iter.data<3>());
            if (!datetime::is_valid_ymd(ymd)) {
                stringstream ss;
                ss << "invalid year/month/day " << ymd.year << "/" << ymd.month << "/" << ymd.day;
                throw runtime_error(ss.str());
            }
            *reinterpret_cast<int32_t *>(iter.data<0>()) = datetime::ymd_to_days(ymd);
        } while (iter.next());
    }

    return result;
}

static pair<string, gfunc::callable> date_dtype_functions[] = {
    pair<string, gfunc::callable>("today", gfunc::make_callable(&function_dtype_today, "self")),
    pair<string, gfunc::callable>("__construct__", gfunc::make_callable(&function_dtype_construct, "self", "year", "month", "day"))
};

void date_dtype::get_dynamic_dtype_functions(const std::pair<std::string, gfunc::callable> **out_functions, size_t *out_count) const
{
    *out_functions = date_dtype_functions;
    *out_count = sizeof(date_dtype_functions) / sizeof(date_dtype_functions[0]);
}

///////// properties on the ndobject

static ndobject property_ndo_get_year(const ndobject& n) {
    return n.replace_udtype(make_property_dtype(n.get_udtype(), "year"));
}

static ndobject property_ndo_get_month(const ndobject& n) {
    return n.replace_udtype(make_property_dtype(n.get_udtype(), "month"));
}

static ndobject property_ndo_get_day(const ndobject& n) {
    return n.replace_udtype(make_property_dtype(n.get_udtype(), "day"));
}

static pair<string, gfunc::callable> date_ndobject_properties[] = {
    pair<string, gfunc::callable>("year", gfunc::make_callable(&property_ndo_get_year, "self")),
    pair<string, gfunc::callable>("month", gfunc::make_callable(&property_ndo_get_month, "self")),
    pair<string, gfunc::callable>("day", gfunc::make_callable(&property_ndo_get_day, "self"))
};

void date_dtype::get_dynamic_ndobject_properties(const std::pair<std::string, gfunc::callable> **out_properties, size_t *out_count) const
{
    *out_properties = date_ndobject_properties;
    *out_count = sizeof(date_ndobject_properties) / sizeof(date_ndobject_properties[0]);
}

///////// functions on the ndobject

static ndobject function_ndo_to_struct(const ndobject& n) {
    return n.replace_udtype(make_property_dtype(n.get_udtype(), "struct"));
}

static ndobject function_ndo_strftime(const ndobject& n, const std::string& format) {
    if (format.empty()) {
        throw runtime_error("format string for strftime should not be empty");
    }
    // TODO: lazy evaluation?
    ndobject result = empty_like(n, make_string_dtype(string_encoding_utf_8));
    ndobject_iter<1, 1> iter(result, n);
    assignment_kernel k;
    bool use_kernel = false;
    if (iter.get_uniform_dtype<1>().get_kind() == expression_kind) {
        make_assignment_kernel(&k, 0,
                        iter.get_uniform_dtype<1>().value_dtype(), NULL,
                        iter.get_uniform_dtype<1>(), iter.metadata<1>(),
                        kernel_request_single, assign_error_default, &eval::default_eval_context);
        use_kernel = true;
    }
    int32_t date;
    const base_string_dtype *esd = static_cast<const base_string_dtype *>(iter.get_uniform_dtype<0>().extended());
    struct tm tm_val;
    string str;
    if (!iter.empty()) {
        do {
            // Get the date
            if (use_kernel) {
                k.get()->get_function<unary_single_operation_t>()(
                                reinterpret_cast<char *>(&date),
                                iter.data<1>(), k.get());
            } else {
                date = *reinterpret_cast<const int32_t *>(iter.data<1>());
            }
            // Convert the date to a 'struct tm'
            datetime::date_to_struct_tm(date, datetime::datetime_unit_day, tm_val);
            // Call strftime, growing the string buffer if needed so it fits
            str.resize(format.size() + 16);
#ifdef _MSC_VER
            // Given an invalid format string strftime will abort unless an invalid
            // parameter handler is installed.
            disable_invalid_parameter_handler raii;
#endif
            for(int i = 0; i < 3; ++i) {
                // Force errno to zero
                errno = 0;
                size_t len = strftime(&str[0], str.size(), format.c_str(), &tm_val);
                if (len > 0) {
                    str.resize(len);
                    break;
                } else {
                    if (errno != 0) {
                        stringstream ss;
                        ss << "error in strftime with format string \"" << format << "\" to strftime";
                        throw runtime_error(ss.str());
                    }
                    str.resize(str.size() * 2);
                }
            }
            // Copy the string to the output
            esd->set_utf8_string(iter.metadata<0>(), iter.data<0>(), assign_error_none, str);
        } while(iter.next());
    }
    return result;
}

static ndobject function_ndo_weekday(const ndobject& n) {
    return n.replace_udtype(make_property_dtype(n.get_udtype(), "weekday"));
}

static ndobject function_ndo_replace(const ndobject& n, int32_t year, int32_t month, int32_t day) {
    // TODO: lazy evaluation?
    if (year == numeric_limits<int32_t>::max() && month == numeric_limits<int32_t>::max() &&
                    day == numeric_limits<int32_t>::max()) {
        throw std::runtime_error("no parameters provided to date.replace, should provide at least one");
    }
    // Create the result array
    ndobject result = empty_like(n);
    ndobject_iter<1, 1> iter(result, n);

    // Get a kernel to produce elements if the input is an expression
    assignment_kernel k;
    bool use_kernel = false;
    if (iter.get_uniform_dtype<1>().get_kind() == expression_kind) {
        make_assignment_kernel(&k, 0,
                        iter.get_uniform_dtype<1>().value_dtype(), NULL,
                        iter.get_uniform_dtype<1>(), iter.metadata<1>(),
                        kernel_request_single, assign_error_default, &eval::default_eval_context);
        use_kernel = true;
    }
    int32_t date;
    const date_dtype *dd = static_cast<const date_dtype *>(iter.get_uniform_dtype<1>().value_dtype().extended());
    datetime::date_ymd ymd;
    if (!iter.empty()) {
        // Loop over all the elements
        do {
            // Get the date
            if (use_kernel) {
                k.get()->get_function<unary_single_operation_t>()(
                                reinterpret_cast<char *>(&date),
                                iter.data<1>(), k.get());
            } else {
                date = *reinterpret_cast<const int32_t *>(iter.data<1>());
            }
            // Convert the date to a 'struct tm'
            datetime::days_to_ymd(date, ymd);
            // Replace the values as requested
            if (year != numeric_limits<int32_t>::max()) {
                ymd.year = year;
            }
            if (month != numeric_limits<int32_t>::max()) {
                ymd.month = month;
                if (-12 <= month && month <= -1) {
                    // Use negative months to count from the end (like Python slicing, though
                    // the standard Python datetime.date doesn't support this)
                    ymd.month = month + 13;
                } else if (1 <= month && month <= 12) {
                    ymd.month = month;
                } else {
                    stringstream ss;
                    ss << "invalid month value " << month;
                    throw runtime_error(ss.str());
                }
                // If the day isn't also being replaced, make sure the resulting date is valid
                if (day == numeric_limits<int32_t>::max()) {
                    if (!datetime::is_valid_ymd(ymd)) {
                        stringstream ss;
                        ss << "invalid replace resulting year/month/day " << year << "/" << month << "/" << day;
                        throw runtime_error(ss.str());
                    }
                }
            }
            if (day != numeric_limits<int32_t>::max()) {
                int month_size = datetime::get_month_size(ymd.year, ymd.month);
                if (1 <= day && day <= month_size) {
                    ymd.day = day;
                } else if (-month_size <= day && day <= -1) {
                    // Use negative days to count from the end (like Python slicing, though
                    // the standard Python datetime.date doesn't support this)
                    ymd.day = day + month_size + 1;
                } else {
                    stringstream ss;
                    ss << "invalid day value " << day << " for year/month " << year << "/" << month;
                    throw runtime_error(ss.str());
                }
            }
            dd->set_ymd(iter.metadata<0>(), iter.data<0>(), assign_error_none, ymd.year, ymd.month, ymd.day);
        } while(iter.next());
    }
    return result;
}

static pair<string, gfunc::callable> date_ndobject_functions[] = {
    pair<string, gfunc::callable>("to_struct", gfunc::make_callable(&function_ndo_to_struct, "self")),
    pair<string, gfunc::callable>("strftime", gfunc::make_callable(&function_ndo_strftime, "self", "format")),
    pair<string, gfunc::callable>("weekday", gfunc::make_callable(&function_ndo_weekday, "self")),
    pair<string, gfunc::callable>("replace", gfunc::make_callable_with_default(&function_ndo_replace, "self", "year", "month", "day",
                    numeric_limits<int32_t>::max(), numeric_limits<int32_t>::max(), numeric_limits<int32_t>::max()))
};

void date_dtype::get_dynamic_ndobject_functions(const std::pair<std::string, gfunc::callable> **out_functions, size_t *out_count) const
{
    *out_functions = date_ndobject_functions;
    *out_count = sizeof(date_ndobject_functions) / sizeof(date_ndobject_functions[0]);
}

///////// property accessor kernels (used by date_property_dtype)

namespace {
    void get_property_kernel_year_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        datetime::date_ymd fld;
        datetime::days_to_ymd(*reinterpret_cast<const int32_t *>(src), fld);
        *reinterpret_cast<int32_t *>(dst) = fld.year;
    }

    void get_property_kernel_month_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        datetime::date_ymd fld;
        datetime::days_to_ymd(*reinterpret_cast<const int32_t *>(src), fld);
        *reinterpret_cast<int32_t *>(dst) = fld.month;
    }

    void get_property_kernel_day_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        datetime::date_ymd fld;
        datetime::days_to_ymd(*reinterpret_cast<const int32_t *>(src), fld);
        *reinterpret_cast<int32_t *>(dst) = fld.day;
    }

    void get_property_kernel_weekday_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        datetime::date_val_t days = *reinterpret_cast<const int32_t *>(src);
        // 1970-01-05 is Monday
        int weekday = (int)((days - 4) % 7);
        if (weekday < 0) {
            weekday += 7;
        }
        *reinterpret_cast<int32_t *>(dst) = weekday;
    }

    void get_property_kernel_days_after_1970_int64_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        datetime::date_val_t days = *reinterpret_cast<const int32_t *>(src);
        if (days == DYND_DATE_NA) {
            *reinterpret_cast<int64_t *>(dst) = numeric_limits<int64_t>::min();
        } else {
            *reinterpret_cast<int64_t *>(dst) = days;
        }
    }

    void set_property_kernel_days_after_1970_int64_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        int64_t days = *reinterpret_cast<const int64_t *>(src);
        if (days == numeric_limits<int64_t>::min()) {
            *reinterpret_cast<int32_t *>(dst) = DYND_DATE_NA;
        } else {
            *reinterpret_cast<int32_t *>(dst) = static_cast<datetime::date_val_t>(days);
        }
    }

    void get_property_kernel_struct_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        datetime::date_ymd fld;
        datetime::days_to_ymd(*reinterpret_cast<const int32_t *>(src), fld);
        default_date_struct_t *dst_struct = reinterpret_cast<default_date_struct_t *>(dst);
        dst_struct->year = fld.year;
        dst_struct->month = static_cast<int8_t>(fld.month);
        dst_struct->day = static_cast<int8_t>(fld.day);
    }

    void set_property_kernel_struct_single(char *dst, const char *src,
                    kernel_data_prefix *DYND_UNUSED(extra))
    {
        datetime::date_ymd fld;
        const default_date_struct_t *src_struct = reinterpret_cast<const default_date_struct_t *>(src);
        fld.year = src_struct->year;
        fld.month = src_struct->month;
        fld.day = src_struct->day;
        *reinterpret_cast<int32_t *>(dst) = datetime::ymd_to_days(fld);
    }
} // anonymous namespace

namespace {
    enum date_properties_t {
        dateprop_year,
        dateprop_month,
        dateprop_day,
        dateprop_weekday,
        dateprop_days_after_1970_int64,
        dateprop_struct
    };
}

size_t date_dtype::get_elwise_property_index(const std::string& property_name) const
{
    // TODO: Use an enum here
    if (property_name == "year") {
        return dateprop_year;
    } else if (property_name == "month") {
        return dateprop_month;
    } else if (property_name == "day") {
        return dateprop_day;
    } else if (property_name == "weekday") {
        return dateprop_weekday;
    } else if (property_name == "days_after_1970_int64") {
        // A read/write property for NumPy datetime64[D] compatibility
        return dateprop_days_after_1970_int64;
    } else if (property_name == "struct") {
        // A read/write property for accessing a date as a struct
        return dateprop_struct;
    } else {
        stringstream ss;
        ss << "dynd date dtype does not have a kernel for property " << property_name;
        throw runtime_error(ss.str());
    }
}

dtype date_dtype::get_elwise_property_dtype(size_t property_index,
            bool& out_readable, bool& out_writable) const
{
    switch (property_index) {
        case dateprop_year:
        case dateprop_month:
        case dateprop_day:
        case dateprop_weekday:
            out_readable = true;
            out_writable = false;
            return make_dtype<int32_t>();
        case dateprop_days_after_1970_int64:
            out_readable = true;
            out_writable = true;
            return make_dtype<int64_t>();
        case dateprop_struct:
            out_readable = true;
            out_writable = true;
            return date_dtype::default_struct_dtype;
        default:
            out_readable = false;
            out_writable = false;
            return make_dtype<void>();
    }
}

size_t date_dtype::make_elwise_property_getter_kernel(
                assignment_kernel *out, size_t offset_out,
                const char *DYND_UNUSED(dst_metadata),
                const char *DYND_UNUSED(src_metadata), size_t src_property_index,
                kernel_request_t kernreq, const eval::eval_context *DYND_UNUSED(ectx)) const
{
    offset_out = make_kernreq_to_single_kernel_adapter(out, offset_out, kernreq);
    kernel_data_prefix *e = out->get_at<kernel_data_prefix>(offset_out);
    // TODO: Use an enum for the property index
    switch (src_property_index) {
        case dateprop_year:
            e->set_function<unary_single_operation_t>(&get_property_kernel_year_single);
            return offset_out + sizeof(kernel_data_prefix);
        case dateprop_month:
            e->set_function<unary_single_operation_t>(&get_property_kernel_month_single);
            return offset_out + sizeof(kernel_data_prefix);
        case dateprop_day:
            e->set_function<unary_single_operation_t>(&get_property_kernel_day_single);
            return offset_out + sizeof(kernel_data_prefix);
        case dateprop_weekday:
            e->set_function<unary_single_operation_t>(&get_property_kernel_weekday_single);
            return offset_out + sizeof(kernel_data_prefix);
        case dateprop_days_after_1970_int64:
            e->set_function<unary_single_operation_t>(&get_property_kernel_days_after_1970_int64_single);
            return offset_out + sizeof(kernel_data_prefix);
        case dateprop_struct:
            e->set_function<unary_single_operation_t>(&get_property_kernel_struct_single);
            return offset_out + sizeof(kernel_data_prefix);
        default:
            stringstream ss;
            ss << "dynd date dtype given an invalid property index" << src_property_index;
            throw runtime_error(ss.str());
    }
}

size_t date_dtype::make_elwise_property_setter_kernel(
                assignment_kernel *out, size_t offset_out,
                const char *DYND_UNUSED(dst_metadata), size_t dst_property_index,
                const char *DYND_UNUSED(src_metadata),
                kernel_request_t kernreq, const eval::eval_context *DYND_UNUSED(ectx)) const
{
    offset_out = make_kernreq_to_single_kernel_adapter(out, offset_out, kernreq);
    kernel_data_prefix *e = out->get_at<kernel_data_prefix>(offset_out);
    // TODO: Use an enum for the property index
    switch (dst_property_index) {
        case dateprop_days_after_1970_int64:
            e->set_function<unary_single_operation_t>(&set_property_kernel_days_after_1970_int64_single);
            return offset_out + sizeof(kernel_data_prefix);
        case dateprop_struct:
            e->set_function<unary_single_operation_t>(&set_property_kernel_struct_single);
            return offset_out + sizeof(kernel_data_prefix);
        default:
            stringstream ss;
            ss << "dynd date dtype given an invalid property index" << dst_property_index;
            throw runtime_error(ss.str());
    }
}

