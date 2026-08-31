#pragma once

// Keep Kindlebrew's warning policy strict without treating warnings inside the
// pinned third-party chess-library header as project warnings. Do not patch the
// vendored source merely to satisfy local compiler diagnostics.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "../third_party/chess.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
