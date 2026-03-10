#include "akasha.hpp"

#include <boost/interprocess/offset_ptr.hpp>
#include <flatbuffers/flexbuffers.h>

namespace {

using AkashaInterprocessTag [[maybe_unused]] = boost::interprocess::offset_ptr<void>;
using AkashaFlexBuilder [[maybe_unused]] = flexbuffers::Builder;

}  // namespace

namespace akasha {

std::string_view version() noexcept {
    return "0.1.0";
}

int add(int a, int b) noexcept {
    return a + b;
}

}  // namespace akasha
