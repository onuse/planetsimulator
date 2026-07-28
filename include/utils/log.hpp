#pragma once

#include <iostream>

namespace util {

// Verbose output is off by default. Everything that happens once per mesh
// rebuild or once per frame goes through vlog(), so the console stays readable
// while flying around; -verbose brings it all back for debugging.
//
// This is not only about noise. Mesh rebuilds happen on the render thread, and
// console writes on Windows are synchronous - printing twenty lines every
// rebuild was adding milliseconds to a frame that was already the expensive
// one.
inline bool verbose = false;

inline std::ostream& vlog() {
    static std::ostream discard(nullptr);
    return verbose ? std::cout : discard;
}

} // namespace util
