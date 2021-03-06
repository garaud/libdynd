//
// Copyright (C) 2011-14 Mark Wiebe, DyND Developers
// BSD 2-Clause License, see LICENSE.txt
//

#include <iostream>
#include <sstream>
#include <stdexcept>
#include "inc_gtest.hpp"

#include <dynd/array.hpp>
#include <dynd/types/datetime_type.hpp>
#include <dynd/types/property_type.hpp>
#include <dynd/types/strided_dim_type.hpp>
#include <dynd/types/fixedstring_type.hpp>
#include <dynd/types/string_type.hpp>
#include <dynd/types/convert_type.hpp>
#include <dynd/types/cstruct_type.hpp>
#include <dynd/types/struct_type.hpp>
#include <dynd/gfunc/callable.hpp>
#include <dynd/gfunc/call_callable.hpp>

using namespace std;
using namespace dynd;

TEST(DateTimeDType, Create) {
    ndt::type d;
    const datetime_type *dd;

    d = ndt::make_datetime(datetime_unit_minute, tz_abstract);
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(8u, d.get_data_size());
    EXPECT_EQ((size_t)scalar_align_of<int64_t>::value, d.get_data_alignment());
    EXPECT_EQ(d, ndt::make_datetime(datetime_unit_minute, tz_abstract));
    EXPECT_EQ(datetime_unit_minute, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    d = ndt::make_datetime(datetime_unit_msecond, tz_utc);
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(d, ndt::make_datetime(datetime_unit_msecond, tz_utc));
    EXPECT_EQ(datetime_unit_msecond, dd->get_unit());
    EXPECT_EQ(tz_utc, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));
}

TEST(DateTimeDType, CreateFromString) {
    ndt::type d;
    const datetime_type *dd;

    d = ndt::type("datetime['hour']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_hour, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    d = ndt::type("datetime['min']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_minute, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    d = ndt::type("datetime['sec']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_second, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    d = ndt::type("datetime['msec']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_msecond, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    d = ndt::type("datetime['usec']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_usecond, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    d = ndt::type("datetime['nsec']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_nsecond, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    // Explicit abstract timezone
    d = ndt::type("datetime['hour', tz='abstract']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_hour, dd->get_unit());
    EXPECT_EQ(tz_abstract, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));

    // UTC timezone
    d = ndt::type("datetime['hour', tz='UTC']");
    ASSERT_EQ(datetime_type_id, d.get_type_id());
    dd = static_cast<const datetime_type *>(d.extended());
    EXPECT_EQ(datetime_unit_hour, dd->get_unit());
    EXPECT_EQ(tz_utc, dd->get_timezone());
    // Roundtripping through a string
    EXPECT_EQ(d, ndt::type(d.str()));
}

TEST(DateTimeDType, ValueCreationAbstractMinutes) {
    ndt::type d = ndt::make_datetime(datetime_unit_minute, tz_abstract), di = ndt::make_type<int64_t>();

    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3 - 365) * 1440LL + 4 * 60 + 16,
                    nd::array("1599-01-01T04:16").ucast(d).view_scalars(di).as<int64_t>());
    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3) * 1440LL + 15 * 60 + 45,
                    nd::array("1600-01-01 15:45").ucast(d).view_scalars(di).as<int64_t>());
    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3 + 366) * 1440LL,
                    nd::array("1601-01-01T00").ucast(d).view_scalars(di).as<int64_t>());

    // Parsing Zulu timezone as abstract raises an error
    EXPECT_THROW(nd::array("2000-01-01T03:00Z").ucast(d).eval(), runtime_error);
    // Parsing specified timezone as abstract raises an error
    EXPECT_THROW(nd::array("2000-01-01T03:00+0300").ucast(d).eval(), runtime_error);

    // Parsing Zulu timezone as abstract with no error checking works though
    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3) * 1440LL + 15 * 60 + 45,
                    nd::array("1600-01-01 15:45Z").ucast(d, 0, assign_error_none).view_scalars(di).as<int64_t>());
    // Parsing specified timezone as abstract with no error checking throws away the time zone
    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3) * 1440LL + 15 * 60 + 45,
                    nd::array("1600-01-01 15:45+0600").ucast(d, 0, assign_error_none).view_scalars(di).as<int64_t>());
}


TEST(DateTimeDType, ValueCreationUTCMinutes) {
    ndt::type d = ndt::make_datetime(datetime_unit_minute, tz_utc), di = ndt::make_type<int64_t>();

    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3 - 365) * 1440LL + 4 * 60 + 16,
                    nd::array("1599-01-01T04:16Z").ucast(d).view_scalars(di).as<int64_t>());
    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3) * 1440LL + 15 * 60 + 45,
                    nd::array("1600-01-01 14:45-0100").ucast(d).view_scalars(di).as<int64_t>());
    EXPECT_EQ(((1600-1970)*365 - (1972-1600)/4 + 3 + 366) * 1440LL,
                    nd::array("1601-01-01T00Z").ucast(d).view_scalars(di).as<int64_t>());
}

TEST(DateTimeDType, ConvertToString) {
    EXPECT_EQ("2013-02-16T12",
                    nd::array("2013-02-16T12").cast(ndt::type("datetime['hour']")).as<string>());
    EXPECT_EQ("2013-02-16T12Z",
                    nd::array("2013-02-16T12Z").cast(ndt::type("datetime['hour', tz='UTC']")).as<string>());

    EXPECT_EQ("2013-02-16T12:13",
                    nd::array("2013-02-16T12:13").cast(ndt::type("datetime['min']")).as<string>());
    EXPECT_EQ("2013-02-16T12:13Z",
                    nd::array("2013-02-16T12:13Z").cast(ndt::type("datetime['min', tz='UTC']")).as<string>());

    EXPECT_EQ("2013-02-16T12:13:19",
                    nd::array("2013-02-16T12:13:19").cast(ndt::type("datetime['sec']")).as<string>());
    EXPECT_EQ("2013-02-16T12:13:19Z",
                    nd::array("2013-02-16T12:13:19Z").cast(ndt::type("datetime['sec', tz='UTC']")).as<string>());

    EXPECT_EQ("2013-02-16T12:13:19.012",
                    nd::array("2013-02-16T12:13:19.012").cast(ndt::type("datetime['msec']")).as<string>());
    EXPECT_EQ("2013-02-16T12:13:19.012Z",
                    nd::array("2013-02-16T12:13:19.012Z").cast(ndt::type("datetime['msec', tz='UTC']")).as<string>());

    EXPECT_EQ("2013-02-16T12:13:19.012345",
                    nd::array("2013-02-16T12:13:19.012345").cast(ndt::type("datetime['usec']")).as<string>());
    EXPECT_EQ("2013-02-16T12:13:19.012345Z",
                    nd::array("2013-02-16T12:13:19.012345Z").cast(ndt::type("datetime['usec', tz='UTC']")).as<string>());

    EXPECT_EQ("2013-02-16T12:13:19.012345678",
                    nd::array("2013-02-16T12:13:19.012345678").cast(ndt::type("datetime['nsec']")).as<string>());
    EXPECT_EQ("2013-02-16T12:13:19.012345678Z",
                    nd::array("2013-02-16T12:13:19.012345678Z").cast(ndt::type("datetime['nsec', tz='UTC']")).as<string>());
}

TEST(DateTimeDType, Properties) {
    nd::array n;

    n = nd::array("1963-02-28T16:12:14.123654").cast(ndt::type("datetime['usec']")).eval();
    EXPECT_EQ(1963, n.p("year").as<int32_t>());
    EXPECT_EQ(2, n.p("month").as<int32_t>());
    EXPECT_EQ(28, n.p("day").as<int32_t>());
    EXPECT_EQ(16, n.p("hour").as<int32_t>());
    EXPECT_EQ(12, n.p("minute").as<int32_t>());
    EXPECT_EQ(14, n.p("second").as<int32_t>());
    EXPECT_EQ(123654, n.p("microsecond").as<int32_t>());
}
