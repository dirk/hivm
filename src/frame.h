#ifndef HVM_FRAME_H
#define HVM_FRAME_H
/// @file frame.h

/// Stack frame.
typedef struct hvm_frame {
  uint64_t       return_addr;
  unsigned char  return_register;
  hvm_obj_struct *locals;
} hvm_frame;

/// @memberof hvm_frame
hvm_frame *hvm_new_frame();

#endif
