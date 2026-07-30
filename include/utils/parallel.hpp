#pragma once

#include <cstddef>
#include <functional>

namespace util {

// Run a loop across a persistent pool of worker threads.
//
// The body is given a range rather than an index, deliberately. The loops this
// exists for run over hundreds of thousands of crust parcels, and a call per
// element would spend more time in std::function dispatch than in the work -
// the point of splitting the loop is to do less total work per element, not
// more.
//
// The pool is created on first use and lives until the process ends. Creating
// threads per call is not viable here: a thread costs tens of microseconds to
// start, the simulation calls this several times per step, and steps happen
// many times a second.
//
// Callers must not write to the same memory from two ranges. Where the natural
// algorithm does - projecting parcels onto cells, where many parcels land on
// one cell - the fix is to compute in parallel and accumulate afterwards,
// rather than to lock.
void parallelFor(size_t count, const std::function<void(size_t begin, size_t end)>& body);

// How many workers the pool has. Zero before first use.
size_t workerCount();

} // namespace util
