#pragma once

namespace on1x {
// Only called after parsing a complete zero-valued shared elevation clause.
// A missing native fault match must not bypass the parser's numeric check.
const char* zero_elevation_fault();
}
