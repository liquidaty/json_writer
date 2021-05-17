#ifndef JSONWRITER_H
#define JSONWRITER_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

  enum jsonwriter_option {
    jsonwriter_option_pretty = 0,
    jsonwriter_option_compact = 1
  };

  enum jsonwriter_error {
    jsonwriter_error_ok = 0, // no error
    jsonwriter_error_out_of_memory = 1,
    jsonwriter_error_invalid_value,
    jsonwriter_error_misconfiguration,
    jsonwriter_error_unrecognized_variant_type
  };

  typedef void * jsonwriter_handle;
  jsonwriter_handle jsonwriter_new_file(FILE *f);
  jsonwriter_handle jsonwriter_new(size_t (*write)(const void *, size_t, size_t, void *),
                                   void *write_arg);

  void jsonwriter_set_option(jsonwriter_handle h, enum jsonwriter_option opt);
  void jsonwriter_delete(jsonwriter_handle h);

  int jsonwriter_start_object(jsonwriter_handle h);
  int jsonwriter_start_array(jsonwriter_handle h);

  int jsonwriter_end(jsonwriter_handle h);
  int jsonwriter_end_all(jsonwriter_handle h);

  // jsonwriter_object_key: if len is zero, will use strlen to calculate
  int jsonwriter_object_key(jsonwriter_handle h, const char *key, size_t len_or_zero);

  int jsonwriter_str(jsonwriter_handle h, const char *s);
  int jsonwriter_strn(jsonwriter_handle h, const char *s, size_t len);
  int jsonwriter_bool(jsonwriter_handle h, unsigned char value);
  int jsonwriter_dbl(jsonwriter_handle h, long double d);
  int jsonwriter_dblf(jsonwriter_handle h, long double d, const char *format_string, char compact);
  int jsonwriter_int(jsonwriter_handle h, long int i);
  int jsonwriter_null(jsonwriter_handle h);


  // optionally, you can configure jsonwriter to handle custom variant types
  enum jsonwriter_datatype {
    jsonwriter_datatype_null = 0,
    jsonwriter_datatype_string = 1,
    jsonwriter_datatype_integer = 2,
    jsonwriter_datatype_float = 3,
    jsonwriter_datatype_bool = 4
    // possible to do:
    //  array
    //  object
  };

  struct jsonwriter_variant {
    enum jsonwriter_datatype type;
    union {
      unsigned char b; // bool
      long int i;      // integer
      long double dbl; // double
      char *str;       // string
      // possible to do:
      //   void *array; // will require corresponding function pointers to get array size and elements
      //   void *object; // will require corresponding function pointers to get object keys and elements
    } value;
  };

  // jsonwriter_set_variant_handler(): provide jsonwriter with a custom function that converts your data to a jsonwriter_variant
  // with an optional cleanup callback
  // returns 0 on success, nonzero on error
  enum jsonwriter_error
  jsonwriter_set_variant_handler(jsonwriter_handle h,
                                 struct jsonwriter_variant (*to_jsw_variant)(void *),
                                 void (*cleanup)(void *, struct jsonwriter_variant *));

  // write a variant. will use custom to_jsw_variant() to convert data to jsonwriter_variant
  enum jsonwriter_error jsonwriter_variant(jsonwriter_handle h, void *data);

#ifdef __cplusplus
}
#endif

#endif // #ifndef JSONWRITER_H
