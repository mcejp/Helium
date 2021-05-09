#ifndef HELIUM_ASSERT_HPP
#define HELIUM_ASSERT_HPP

// Always enabled (also in release) -- use sparingly
#define helium_abort_expression(expr_) do { helium_assert_fail(#expr_, __FILE__, __LINE__); } while(false)
#define helium_assert(expr_) do { if (!(expr_)) helium_assert_fail(#expr_, __FILE__, __LINE__); } while(false)

// Enabled only in debug builds
#ifdef NDEBUG
#define helium_assert_debug(expr_) do {} while(false)
#define helium_unreachable() __builtin_unreachable()
#else
#define helium_assert_debug(expr_) do { if (!(expr_)) helium_assert_fail(#expr_, __FILE__, __LINE__); } while(false)
#define helium_unreachable() helium_assert_fail("This code should not be reachable", __FILE__, __LINE__)
#endif

// This must be only used as a temporary solution (malformed user data must not cause a crash)
#define helium_assert_userdata(expr_) do { if (!(expr_)) helium_assert_fail(#expr_, __FILE__, __LINE__); } while(false)

// TODO: a better/standard way to achieve this?
[[noreturn]] void helium_assert_fail(const char* expression, const char* file, int line);

#endif
