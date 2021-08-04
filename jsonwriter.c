#include <stdio.h>
#include <string.h>
#include <jsonwriter.h>

#ifdef INCLUDE_UTILS
#include "utils.c"
#else
unsigned int json_esc1(const char *s, unsigned int slen,
                       unsigned int *replacelen,
                       char replace[], // replace buff should be at least 8 bytes long
                       const char **new_s,
                       size_t max_output_size);
#endif

#define JSONWRITER_MAX_NESTING 256
struct jsonwriter_data {
  unsigned int depth;
  char *close_brackets; // length = JSONWRITER_MAX_NESTING
  int *counts; // at each level, the current number of items
  size_t (*write)(const void *, size_t, size_t, void *);
  void *write_arg;
  char tmp[128]; // number buffer

  struct jsonwriter_variant (*to_jsw_variant)(void *);
  void (*after_to_jsw_variant)(void *, struct jsonwriter_variant *);

  unsigned char just_wrote_key:1;
  unsigned char compact:1;
  unsigned char started:1;
  unsigned char dummy:5;
};

void jsonwriter_set_option(jsonwriter_handle h, enum jsonwriter_option opt) {
  struct jsonwriter_data *data = h;
  switch(opt) {
  case jsonwriter_option_pretty:
    data->compact = 0;
    break;
  case jsonwriter_option_compact:
    data->compact = 1;
    break;
  }
}

static size_t fwrite2(const void *p, size_t n, size_t size, void *f) {
  return (size_t) fwrite(p, n, size, f);
}

jsonwriter_handle jsonwriter_new_file(FILE *f) {
  return jsonwriter_new(fwrite2, f);
}


jsonwriter_handle jsonwriter_new(size_t (*write)(const void *, size_t, size_t, void *),
                                 void *write_arg) {
  struct jsonwriter_data *data = calloc(1, sizeof(*data));
  data->write = write;
  data->write_arg = write_arg;
  data->close_brackets = malloc(JSONWRITER_MAX_NESTING * sizeof(*data->close_brackets));
  data->counts = calloc(JSONWRITER_MAX_NESTING, sizeof(*data->counts));
  return data;
}

void jsonwriter_delete(jsonwriter_handle h) {
  struct jsonwriter_data *data = h;
  if(data->close_brackets)
    free(data->close_brackets);
  if(data->counts)
    free(data->counts);
  free(h);
}

static size_t jsonwriter_writeln(struct jsonwriter_data *data) {
  if(data->started)
    return data->write("\n", 1, 1, data->write_arg);
  return 0;
}

static int jsonwriter_indent(struct jsonwriter_data *data, char closing) {
  if(data->just_wrote_key) {
    if(data->compact)
      data->write(":", 1, 1, data->write_arg);
    else
      data->write(": ", 1, 2, data->write_arg);
    data->just_wrote_key = 0;
    return 0;
  }

  if(data->depth) {
    if(!closing) { // add a value to the current list
      if(data->counts[data->depth - 1])
        data->write(",", 1, 1, data->write_arg);
      data->counts[data->depth - 1]++;
    }
  }

  if(!data->compact) {
    jsonwriter_writeln(data);
    for(int d = data->depth; d > 0; d--)
      data->write("  ", 1, 2, data->write_arg);
  }
  return 0;
}

static enum jsonwriter_status jsonwriter_end_aux(jsonwriter_handle h, char close_bracket) { // return 0 on success
  struct jsonwriter_data *data = h;
  if(data->depth > 0) {
    if(close_bracket && data->close_brackets[data->depth-1] != close_bracket)
      return jsonwriter_status_invalid_end;

    data->depth--;

    if(data->depth < JSONWRITER_MAX_NESTING - 1) {
      jsonwriter_indent(data, 1);
      data->write(data->close_brackets + data->depth, 1, 1, data->write_arg);
    }
    if(data->depth == 0)
      jsonwriter_writeln(data);
    return jsonwriter_status_ok;
  }
  return jsonwriter_status_invalid_end; // error: nothing to close
}

enum jsonwriter_status jsonwriter_end(jsonwriter_handle h) {
  return jsonwriter_end_aux(h, 0);
}

enum jsonwriter_status jsonwriter_end_array(jsonwriter_handle h) {
  return jsonwriter_end_aux(h, ']');
}

enum jsonwriter_status jsonwriter_end_object(jsonwriter_handle h) {
  return jsonwriter_end_aux(h, '}');
}

int jsonwriter_end_all(jsonwriter_handle h) {
  while(jsonwriter_end(h) == 0)
    ;
  return 0;
}

static int write_json_str(size_t (*write_func)(const void *, size_t, size_t, void *),
                          void *write_arg,
                          const char *s, size_t len,
                          char no_quotes) {
  unsigned int offset = 0;
  unsigned int replacelen;
  char replace[10];
  const char *end = s + len;
  const char *new_s;
  size_t written = 0;
  if(!no_quotes)
    write_func("\"", 1, 1, write_arg), written++;

  while(s < end) {
    replacelen = 0;
    unsigned int no_esc = json_esc1((const char *)(s + offset), len - offset, &replacelen, replace, (const char **)&new_s, len);
    if(no_esc)
      write_func(s, 1, no_esc, write_arg), written += no_esc;
    if(replacelen)
      write_func(replace, 1, replacelen, write_arg), written += replacelen;
    if(new_s > s)
      s = new_s;
    else
      break;
  }
  if(!no_quotes)
    write_func("\"", 1, 1, write_arg), written += 1;

  return written;
}

static int jsonwriter_str1(jsonwriter_handle h, const char *s, size_t len) {
  struct jsonwriter_data *data = h;
  write_json_str(data->write, data->write_arg, s, len, 0);
  return 0;
}

int jsonwriter_null(jsonwriter_handle h) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    data->write("null", 1, 4, data->write_arg);
    return 0;
  }
  return 1;
}

int jsonwriter_bool(jsonwriter_handle h, unsigned char value) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    if(value)
      data->write("true", 1, 4, data->write_arg);
    else
      data->write("false", 1, 5, data->write_arg);
    return 0;
  }
  return 1;
}

int jsonwriter_str(jsonwriter_handle h, const char *s) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    if(s)
      jsonwriter_str1(h, s, strlen(s));
    else
      data->write("null", 1, 4, data->write_arg);
    return 0;
  }
  return 1;
}

int jsonwriter_strn(jsonwriter_handle h, const char *s, size_t len) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    jsonwriter_str1(h, s, len);
    return 0;
  }
  return 1;
}

static int jsonwriter_object_keyn(jsonwriter_handle h, const char *key, size_t len_or_zero) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    jsonwriter_str1(h, key, len_or_zero == 0 ? strlen(key) : len_or_zero);
    data->just_wrote_key = 1;
    return 0;
  }
  return 1;
}

int jsonwriter_object_key(jsonwriter_handle h, const char *key) {
  return jsonwriter_object_keyn(h, key, strlen(key));
}

int jsonwriter_dblf(jsonwriter_handle h, long double d, const char *format_string, char compact) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    format_string = format_string ? format_string : "%Lf";
    int len = snprintf(data->tmp, sizeof(data->tmp), format_string, d);
    if(len && compact && memchr(data->tmp, '.', len)) {
      while(len && data->tmp[len-1] == '0')
        len--;
      if(len && data->tmp[len-1] == '.')
        len--;
      if(!len) {
        *data->tmp = '0';
        len = 1;
      }
    }
    data->write(data->tmp, 1, len, data->write_arg);
    return 0;
  }
  return 1;
}

int jsonwriter_dbl(jsonwriter_handle h, long double d) {
  return jsonwriter_dblf(h, d, NULL, 1);
}

int jsonwriter_int(jsonwriter_handle h, long int i) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    int len = snprintf(data->tmp, sizeof(data->tmp), "%li", i);
    data->write(data->tmp, 1, len, data->write_arg);
    return 0;
  }
  return 1;
}

static int jsonwriter_go_deeper(struct jsonwriter_data *data, char open, char close) {
  if(data->depth < JSONWRITER_MAX_NESTING - 1) {
    jsonwriter_indent(data, 0);
    data->started = 1;
    data->write(&open, 1, 1, data->write_arg);
    data->close_brackets[data->depth] = close;
    data->depth++;
    data->counts[data->depth-1] = 0;
    return 0;
  }
  return 1;
}

int jsonwriter_start_object(jsonwriter_handle h) {
  return jsonwriter_go_deeper((struct jsonwriter_data *)h, '{', '}');
}

int jsonwriter_start_array(jsonwriter_handle h) {
  return jsonwriter_go_deeper((struct jsonwriter_data *)h, '[', ']');
}

enum jsonwriter_status jsonwriter_set_variant_handler(
    jsonwriter_handle h,
    struct jsonwriter_variant (*to_jsw_variant)(void *),
    void (*cleanup)(void *, struct jsonwriter_variant *)
) {
  struct jsonwriter_data *d = h;
  d->after_to_jsw_variant = cleanup;
  if(!(d->to_jsw_variant = to_jsw_variant))
    return jsonwriter_status_invalid_value;

  return jsonwriter_status_ok;
}

enum jsonwriter_status jsonwriter_variant(jsonwriter_handle h, void *data) {
  struct jsonwriter_data *d = h;
  if(!d->to_jsw_variant)
    return jsonwriter_status_misconfiguration;
  struct jsonwriter_variant jv = d->to_jsw_variant(data);

  int rc = jsonwriter_status_unrecognized_variant_type;
  switch(jv.type) {
  case jsonwriter_datatype_null:
    rc = jsonwriter_null(h);
    break;
  case jsonwriter_datatype_string:
    rc = jsonwriter_str(h, jv.value.str);
    break;
  case jsonwriter_datatype_integer:
    rc = jsonwriter_int(h, jv.value.i);
    break;
  case jsonwriter_datatype_float:
    rc = jsonwriter_dbl(h, jv.value.dbl);
    break;
  case jsonwriter_datatype_bool:
    rc = jsonwriter_bool(h, jv.value.i ? 1 : 0);
    break;
  }
  if(d->after_to_jsw_variant)
    d->after_to_jsw_variant(data, &jv);
  return rc;
}
