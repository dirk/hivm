#ifndef HVM_EXCEPTION_H
#define HVM_EXCEPTION_H
/// @file exception.h

typedef struct hvm_exception {
  hvm_obj_ref *message;
#ifdef GLIB_MAJOR_VERSION
  /// Array of frames.
  GArray *backtrace;
#else
  /// @cond
  void *backtrace;
  /// @endcond
#endif
} hvm_exception;

hvm_exception *hvm_new_exception();

#endif
