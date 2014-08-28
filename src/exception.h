#ifndef HVM_EXCEPTION_H
#define HVM_EXCEPTION_H
/// @file exception.h

typedef struct hvm_exception {
  hvm_obj_ref *message;
  hvm_obj_ref *data;
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
void hvm_exception_build_backtrace(hvm_exception *exc, hvm_vm *vm);
void hvm_exception_push_location(hvm_exception *exc, hvm_location *loc);
void hvm_exception_print(hvm_exception *exc);

void hvm_print_backtrace(void *backtrace);

hvm_obj_ref *hvm_obj_for_exception(hvm_vm *vm, hvm_exception *exc);

#endif
