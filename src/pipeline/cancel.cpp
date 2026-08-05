#include <semper/cancel.hpp>

#include <atomic>

namespace Semper {
namespace pipeline {

// Its own translation unit, free of OpenCV and threads, so the host test suite
// can link the cancel contract without pulling in the whole solver.
//
// The process-global free functions delegate to the currently *active* token.
// It defaults to a process-wide token (the historical behavior — the JNI adapter
// calls request_cancel()/clear_cancel() with nothing else bound). The
// CancelToken-taking overload of run_full_field binds its own token for the
// duration of a solve via ScopedCancelToken, so the solver's unchanged
// `cancel_requested()` polls target that per-solve token instead.
//
// Relaxed ordering throughout: the solver polls once per point against a full
// ICGN solve, and a poll that reads a stale value only costs one more point.
// Nothing downstream depends on ordering with other memory.
static CancelToken g_default_token;
static std::atomic<CancelToken*> g_active_token{&g_default_token};

void request_cancel() { g_active_token.load(std::memory_order_relaxed)->request(); }

void clear_cancel() { g_active_token.load(std::memory_order_relaxed)->clear(); }

bool cancel_requested() { return g_active_token.load(std::memory_order_relaxed)->requested(); }

namespace detail {

ScopedCancelToken::ScopedCancelToken(CancelToken& token)
    : prev_(g_active_token.exchange(&token, std::memory_order_relaxed)) {}

ScopedCancelToken::~ScopedCancelToken() {
    g_active_token.store(prev_, std::memory_order_relaxed);
}

} // namespace detail

} // namespace pipeline
} // namespace Semper
