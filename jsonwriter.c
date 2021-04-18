#include <stdio.h>
#include <string.h>
#include <jsonwriter.h>

#ifdef INCLUDE_UTILS
#include "utils.c"
#endif

#define JSONWRITER_MAX_NESTING 256
struct jsonwriter_data {
  unsigned int depth;
  char *close_brackets; // length = JSONWRITER_MAX_NESTING
  int *counts; // at each level, the current number of items
  size_t (*write)(const void *, size_t, size_t, void *);
  void *write_arg;
  char tmp[128]; // number buffer
  char just_wrote_key;
};

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
}

/*
static int jsonwriter_in_array(struct jsonwriter_data *data) {
  return data->close_brackets[data->depth] == ']';
}
*/

static int jsonwriter_indent(struct jsonwriter_data *data, char closing) {
  if(data->just_wrote_key) {
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

  data->write("\n", 1, 1, data->write_arg);
  for(int d = data->depth; d > 0; d--)
    data->write("  ", 1, 2, data->write_arg);
  return 0;
}

int jsonwriter_end(jsonwriter_handle h) { // return 0 on success
  struct jsonwriter_data *data = h;
  if(data->depth > 0) {
    data->depth--;
    if(data->depth < JSONWRITER_MAX_NESTING - 1) {
      jsonwriter_indent(data, 1);
      data->write(data->close_brackets + data->depth, 1, 1, data->write_arg);
    }
    return 0;
  }
  return 1; // error: nothing to close
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

int jsonwriter_str(jsonwriter_handle h, const char *s, size_t len) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    jsonwriter_str1(h, s, len);
    return 0;
  }
  return 1;
}

int jsonwriter_object_key(jsonwriter_handle h, const char *key, size_t len_or_zero) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    jsonwriter_str1(h, key, len_or_zero == 0 ? strlen(key) : len_or_zero == 0);
    data->just_wrote_key = 1;
    return 0;
  }
  return 1;
}

int jsonwriter_dbl(jsonwriter_handle h, long double d) {
  struct jsonwriter_data *data = h;
  if(data->depth < JSONWRITER_MAX_NESTING) {
    jsonwriter_indent(data, 0);
    int len = snprintf(data->tmp, sizeof(data->tmp), "%Lf", d);
    data->write(data->tmp, 1, len, data->write_arg);
    return 0;
  }
  return 1;
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
