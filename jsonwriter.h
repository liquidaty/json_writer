#ifndef JSONWRITER_H
#define JSONWRITER_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef void * jsonwriter_handle;
  jsonwriter_handle jsonwriter_new(size_t (*write)(const void *, size_t, size_t, void *),
                                   void *write_arg);

  void jsonwriter_delete(jsonwriter_handle h);

  int jsonwriter_start_object(jsonwriter_handle h);
  int jsonwriter_start_array(jsonwriter_handle h);

  int jsonwriter_end(jsonwriter_handle h);
  int jsonwriter_end_all(jsonwriter_handle h);

  // jsonwriter_object_key: if len is zero, will use strlen to calculate
  int jsonwriter_object_key(jsonwriter_handle h, const char *key, size_t len_or_zero);

  int jsonwriter_str(jsonwriter_handle h, const char *s, size_t len);
  int jsonwriter_bool(jsonwriter_handle h, unsigned char value);
  int jsonwriter_dbl(jsonwriter_handle h, long double d);
  int jsonwriter_int(jsonwriter_handle h, long int i);
  int jsonwriter_null(jsonwriter_handle h);

#ifdef __cplusplus
}
#endif

#endif // #ifndef JSONWRITER_H
