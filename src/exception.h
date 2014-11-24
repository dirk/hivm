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

hvm_obj_ref *hvm_exception_new(hvm_vm*, hvm_obj_ref *message);

void hvm_exception_push_location(hvm_vm *vm, hvm_obj_ref *exc, hvm_location *loc);
void hvm_exception_build_backtrace(hvm_obj_ref *exc, hvm_vm *vm);
void hvm_exception_print(hvm_vm *vm, hvm_obj_ref *exc);

// void hvm_print_backtrace(void*);// Argument should be a GArray*
void hvm_print_backtrace_array(hvm_obj_ref *backtrace);

hvm_obj_ref *hvm_obj_for_exception(hvm_vm *vm, hvm_exception *exc);

#endif
