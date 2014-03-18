#ifndef HVM_FRAME_H
#define HVM_FRAME_H
/// @file frame.h

/// Stack frame.
typedef struct hvm_frame {
  /// Address to return to (set by caller).
  uint64_t       return_addr;
  /// Register to be written to when returning (set by caller).
  unsigned char  return_register;
  /// Local variables of the frame.
  hvm_obj_struct *locals;
} hvm_frame;

/// @memberof hvm_frame
hvm_frame *hvm_new_frame();

#endif
