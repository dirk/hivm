#ifndef HVM_FRAME_H
#define HVM_FRAME_H
/// @file frame.h

typedef struct hvm_frame {
  uint64_t       return_addr;
  hvm_obj_struct *locals;
} hvm_frame;

#endif
