#ifndef HVM_FRAME_H
#define HVM_FRAME_H
/// @file frame.h

/// Default value for the exception catch destination for a frame.
#define HVM_FRAME_EMPTY_CATCH 0xFFFFFFFFFFFFFFFF

/// Stack frame.
typedef struct hvm_frame {
  /// Current address in this frame.
  uint64_t       current_addr;
  /// Address to return to (set by caller).
  uint64_t       return_addr;
  /// Register to be written to when returning (set by caller).
  unsigned char  return_register;
  /// Exception catch destination
  uint64_t       catch_addr;
  /// Register for exception to be written to
  unsigned char  catch_register;
  /// Local variables of the frame.
  hvm_obj_struct *locals;
} hvm_frame;

typedef struct hvm_location {
  /// Frame this location belongs to.
  hvm_frame *frame;

  char *name;
  char *file;
  unsigned int line;
} hvm_location;

/// @memberof hvm_frame
hvm_frame *hvm_new_frame();
void hvm_frame_initialize(hvm_frame *frame);

hvm_location *hvm_new_location();

#endif
