
#ifndef OAPV_EXPORT_H
#define OAPV_EXPORT_H

#ifdef OAPV_STATIC_DEFINE
#  define OAPV_EXPORT
#  define OAPV_NO_EXPORT
#else
#  ifndef OAPV_EXPORT
#    ifdef oapv_dynamic_EXPORTS
        /* We are building this library */
#      define OAPV_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define OAPV_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef OAPV_NO_EXPORT
#    define OAPV_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef OAPV_DEPRECATED
#  define OAPV_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef OAPV_DEPRECATED_EXPORT
#  define OAPV_DEPRECATED_EXPORT OAPV_EXPORT OAPV_DEPRECATED
#endif

#ifndef OAPV_DEPRECATED_NO_EXPORT
#  define OAPV_DEPRECATED_NO_EXPORT OAPV_NO_EXPORT OAPV_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef OAPV_NO_DEPRECATED
#    define OAPV_NO_DEPRECATED
#  endif
#endif

#endif /* OAPV_EXPORT_H */
