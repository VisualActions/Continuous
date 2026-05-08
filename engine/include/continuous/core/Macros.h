// Continuous Engine - macros: API export, FORCEINLINE, noncopyable, debug break.
#pragma once

#if defined(_MSC_VER)
    #define CN_FORCEINLINE __forceinline
    #define CN_NOINLINE    __declspec(noinline)
    #define CN_RESTRICT    __restrict
    #define CN_DEBUG_BREAK() __debugbreak()
    #define CN_ALIGN(N)    __declspec(align(N))
#else
    #define CN_FORCEINLINE inline __attribute__((always_inline))
    #define CN_NOINLINE    __attribute__((noinline))
    #define CN_RESTRICT    __restrict__
    #define CN_DEBUG_BREAK() __builtin_trap()
    #define CN_ALIGN(N)    __attribute__((aligned(N)))
#endif

// The engine compiles either as a static library or a DLL. We use the static
// library configuration for everything except hot-reloadable gameplay DLLs,
// which export their own symbols via CN_GAMEPLAY_API.
#if defined(CN_ENGINE_SHARED)
    #if defined(CN_ENGINE_BUILD)
        #define CN_API __declspec(dllexport)
    #else
        #define CN_API __declspec(dllimport)
    #endif
#else
    #define CN_API
#endif

#if defined(CN_GAMEPLAY_BUILD)
    #define CN_GAMEPLAY_API __declspec(dllexport)
#else
    #define CN_GAMEPLAY_API __declspec(dllimport)
#endif

#define CN_NONCOPYABLE(T)         \
    T(const T&) = delete;         \
    T& operator=(const T&) = delete

#define CN_NONMOVABLE(T)          \
    T(T&&) = delete;              \
    T& operator=(T&&) = delete

#define CN_DEFAULT_COPY(T)        \
    T(const T&) = default;        \
    T& operator=(const T&) = default

#define CN_DEFAULT_MOVE(T)        \
    T(T&&) noexcept = default;    \
    T& operator=(T&&) noexcept = default

// Concatenation helpers for unique identifiers in macros.
#define CN_CAT_(a, b) a##b
#define CN_CAT(a, b)  CN_CAT_(a, b)
#define CN_UNIQUE(name) CN_CAT(name, __LINE__)

// Loop unrolling pragma.
#if defined(_MSC_VER)
    #define CN_UNROLL
#else
    #define CN_UNROLL _Pragma("GCC unroll 8")
#endif

#define CN_UNUSED(x) ((void)(x))
