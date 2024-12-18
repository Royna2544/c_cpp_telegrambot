#pragma once

#include <fmt/format.h>

#include <boost/units/io.hpp>
#include <boost/units/physical_dimensions/information.hpp>
#include <boost/units/quantity.hpp>
#include <boost/units/systems/si.hpp>
#include <boost/units/unit.hpp>
#include <cstddef>
#include <iomanip>

namespace boost::units::data {

// Base unit: Byte
struct byte_base_unit
    : public boost::units::base_unit<byte_base_unit,
                                     boost::units::information_dimension, 1> {
    static std::string name() { return "byte"; }
    static std::string symbol() { return "B"; }
};
using byte_unit = byte_base_unit::unit_type;
BOOST_UNITS_STATIC_CONSTANT(bytes, byte_unit);

// Derived units
using kilobyte_unit = boost::units::make_scaled_unit<
    byte_unit, boost::units::scale<10, boost::units::static_rational<3>>>::type;
BOOST_UNITS_STATIC_CONSTANT(kilobytes, kilobyte_unit);

using megabyte_unit = boost::units::make_scaled_unit<
    kilobyte_unit,
    boost::units::scale<10, boost::units::static_rational<3>>>::type;
BOOST_UNITS_STATIC_CONSTANT(megabytes, megabyte_unit);

using gigabyte_unit = boost::units::make_scaled_unit<
    megabyte_unit,
    boost::units::scale<10, boost::units::static_rational<3>>>::type;
}  // namespace boost::units::data

using Bytes = boost::units::quantity<boost::units::data::byte_unit>;
using KiloBytes = boost::units::quantity<boost::units::data::kilobyte_unit>;
using MegaBytes = boost::units::quantity<boost::units::data::megabyte_unit>;
using GigaBytes = boost::units::quantity<boost::units::data::gigabyte_unit>;

inline GigaBytes operator""_GB(const unsigned long long bytes) {
    return GigaBytes(bytes * boost::units::data::bytes);
}

inline MegaBytes operator""_MB(const unsigned long long bytes) {
    return MegaBytes(bytes * boost::units::data::kilobytes);
}

inline KiloBytes operator""_KB(const unsigned long long bytes) {
    return KiloBytes(bytes * boost::units::data::bytes);
}