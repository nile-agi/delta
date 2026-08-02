#ifndef DELTA_AGENT_TIME_COMPAT_H
#define DELTA_AGENT_TIME_COMPAT_H

#include <ctime>

namespace delta {
namespace agent {

// MSVC has no localtime_r/gmtime_r and its _s variants take the arguments reversed.
// Both fill the caller's tm, unlike localtime()/gmtime(), which share a static buffer
// across the server's threads.
inline void local_time(const time_t* t, struct tm* out) {
#ifdef _WIN32
    localtime_s(out, t);
#else
    localtime_r(t, out);
#endif
}

inline void utc_time(const time_t* t, struct tm* out) {
#ifdef _WIN32
    gmtime_s(out, t);
#else
    gmtime_r(t, out);
#endif
}

} // namespace agent
} // namespace delta

#endif // DELTA_AGENT_TIME_COMPAT_H
