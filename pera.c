#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG
#define STACK_SIZE 256
#define TABLE_LOAD 0.75

typedef enum
{
  TYPE_NIL,
  TYPE_BOOL,
  TYPE_NUMBER,
  TYPE_OBJECT,
} value_type_t;

typedef enum
{
  OBJECT_STRING,
} object_type_t;

typedef struct object
{
  object_type_t type;
  struct object *next;
} object_t;

typedef struct
{
  object_t object;
  int length;
  uint32_t hash;
  char *chars;
} string_t;

typedef struct
{
  value_type_t type;
  union
  {
    bool boolean;
    double number;
    object_t *object;
  } as;
} value_t;

typedef struct
{
  string_t *key;
  value_t value;
} pair_t;

typedef struct
{
  object_t object;
  int count;
  int capacity;
  pair_t *pairs;
} table_t;

typedef enum
{
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_CONSTANT,
  OP_SET_GLOBAL,
  OP_GET_GLOBAL,
  OP_NEG,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_NOT,
  OP_EQ,
  OP_CONCAT,
  OP_PRINT,
  OP_RETURN,
  OP_ERROR,
} opcode_t;

typedef enum
{
  RESULT_OK,
  RESULT_COMPILE_ERROR,
  RESULT_RUNTIME_ERROR,
} result_t;

typedef enum
{
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_WORD,
  TOKEN_NUMBER,
  TOKEN_STRING,
  TOKEN_END,
} token_type_t;

typedef struct
{
  token_type_t type;
  const char *start;
  int length;
} token_t;

typedef struct
{
  const char *start;
  const char *current;
} scan_t;

typedef struct
{
  int length;
  int capacity;
  value_t *values;
} array_t;

typedef struct
{
  int length;
  int capacity;
  uint8_t *code;
  array_t constants;
} block_t;

typedef struct
{
  block_t block;
  uint8_t *pc;
  value_t stack[STACK_SIZE];
  value_t *top;
  table_t strings;
  table_t globals;
  object_t *objects;
} vm_t;

/* GLOBALS */

scan_t scan;
vm_t vm;

/* ARRAY FUNCTIONS */

void
array_new (array_t *array)
{
  array->length = 0;
  array->capacity = 8;
  array->values = malloc (8 * sizeof (value_t));
}

void
array_push (array_t *array, value_t value)
{
  if (array->capacity < array->length + 1)
    {
      array->capacity *= 2;
      array->values
          = realloc (array->values, array->capacity * sizeof (value_t));
      if (array->values == NULL)
        exit (1);
    }

  array->values[array->length] = value;
  array->length++;
}

void
array_free (array_t *array)
{
  free (array->values);
}

/* BLOCK FUNCTIONS */

void
block_new (block_t *block)
{
  block->length = 0;
  block->capacity = 8;
  block->code = malloc (8 * sizeof (uint8_t));
  array_new (&block->constants);
}

void
block_push (block_t *block, uint8_t byte)
{
  if (block->capacity < block->length + 1)
    {
      block->capacity *= 2;
      block->code = realloc (block->code, block->capacity * sizeof (uint8_t));
      if (block->code == NULL)
        exit (1);
    }

  block->code[block->length] = byte;
  block->length++;
}

int
block_add_constant (block_t *block, value_t value)
{
  array_push (&block->constants, value);
  return block->constants.length - 1;
}

void
block_push_constant (block_t *block, value_t value, opcode_t op)
{
  int constant = block_add_constant (block, value);
  if (constant > UINT8_MAX)
    {
      fprintf (stderr, "Too many constants in block.\n");
      exit (1);
    }

  block_push (block, op);
  block_push (block, constant);
}

void
block_free (block_t *block)
{
  free (block->code);
  array_free (&block->constants);
}

/* TABLE FUNCTIONS */

void
table_fill_null_pairs (pair_t *pairs, int capacity)
{
  for (int i = 0; i < capacity; i++)
    {
      pairs[i].key = NULL;
      pairs[i].value = (value_t){ .type = TYPE_NIL };
    }
}

void
table_new (table_t *table)
{
  table->count = 0;
  table->capacity = 8;
  table->pairs = malloc (8 * sizeof (pair_t));
  table_fill_null_pairs (table->pairs, table->capacity);
}

pair_t *
table_get (table_t *table, string_t *key)
{
  uint32_t i = key->hash % table->capacity;
  pair_t *dead = NULL;

  while (1)
    {
      pair_t *pair = &table->pairs[i];
      if (pair->key == NULL)
        {
          /* check for empty pair */
          if (pair->value.type == TYPE_NIL)
            return dead == NULL ? pair : dead;
          /* else it's a dead pair */
          else if (dead == NULL)
            dead = pair;
        }
      else if (pair->key == key)
        {
          return pair;
        }
      i = (i + 1) % table->capacity;
    }
}

bool
table_key_equals_string (pair_t *p, string_t *s)
{
  return p->key->length == s->length && p->key->hash == s->hash
         && memcmp (p->key->chars, s->chars, s->length) == 0;
}

string_t *
table_find_string (table_t *table, string_t *string)
{
  if (table->count == 0)
    return NULL;

  uint32_t i = string->hash % table->capacity;
  while (1)
    {
      pair_t *pair = &table->pairs[i];
      if (pair->key == NULL)
        {
          /* check for empty pair */
          if (pair->value.type == TYPE_NIL)
            return NULL;
        }
      else if (table_key_equals_string (pair, string))
        {
          return pair->key;
        }
      i = (i + 1) % table->capacity;
    }
}

void
table_grow (table_t *table)
{
  int new_capacity = table->capacity * 2;
  pair_t *new_pairs = malloc (new_capacity * sizeof (pair_t));
  table_fill_null_pairs (new_pairs, new_capacity);

  table->count = 0;
  for (int i = 0; i < table->capacity; i++)
    {
      pair_t *pair = &table->pairs[i];
      if (pair->key == NULL)
        continue;

      pair_t *dest_pair = table_get (table, pair->key);
      dest_pair->key = pair->key;
      dest_pair->value = pair->value;

      table->count++;
    }

  free (table->pairs);

  table->capacity = new_capacity;
  table->pairs = new_pairs;
}

bool
table_set (table_t *table, string_t *key, value_t value)
{
  if (table->count + 1 > table->capacity * TABLE_LOAD)
    table_grow (table);
  pair_t *pair = table_get (table, key);
  bool is_new = pair->key == NULL;
  if (is_new && pair->value.type == TYPE_NIL)
    table->count++;

  pair->key = key;
  pair->value = value;
  return is_new;
}

bool
table_remove (table_t *table, string_t *key)
{
  if (table->count == 0)
    return false;

  pair_t *pair = table_get (table, key);
  if (pair->key == NULL)
    return false;

  pair->key = NULL;
  pair->value = (value_t){ .type = TYPE_BOOL, .as.boolean = true };
  return true;
}

void
table_free (table_t *table)
{
  free (table->pairs);
}

uint32_t
hash_from_string (const char *string, int length)
{
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++)
    hash = (hash ^ (uint8_t)string[i]) * 16777619;
  return hash;
}

/* STRING FUNCTIONS */

void
string_free (string_t *string)
{
  // this doesn't remove the string from vm.objects for GC!
  free (string->chars);
  free (string);
}

string_t *
string_new (char *chars, int length)
{
  string_t *s = malloc (sizeof (string_t));
  object_t *o = (object_t *)s;

  s->length = length;
  s->chars = chars;
  s->hash = hash_from_string (s->chars, length);

  string_t *interned = table_find_string (&vm.strings, s);
  if (interned != NULL)
    {
      string_free (s);
      return interned;
    }

  o->type = OBJECT_STRING;
  o->next = vm.objects;
  vm.objects = o;

  table_set (&vm.strings, s, (value_t){ .type = TYPE_NIL });
  return s;
}

string_t *
string_allocate (char *chars, int length)
{
  char *dest_chars = malloc ((length + 1) * sizeof (char));

  memcpy (dest_chars, chars, length);
  dest_chars[length] = '\0';

  return string_new (dest_chars, length);
}

string_t *
string_concat_and_allocate (char *chars_a, int len_a, char *chars_b, int len_b)
{
  int length = len_a + len_b;
  char *dest_chars = malloc ((length + 1) * sizeof (char));

  memcpy (dest_chars, chars_a, len_a);
  memcpy (dest_chars + len_a, chars_b, len_b);
  dest_chars[length] = '\0';

  return string_new (dest_chars, length);
}

string_t *
string_copy (char *chars, int length)
{
  return string_allocate (chars, length);
}

/* GC FUNCTIONS */

void
gc_free_object (object_t *object)
{
  switch (object->type)
    {
    case OBJECT_STRING:
      {
        string_t *string = (string_t *)object;
        string_free (string);
        break;
      }
    }
}

void
gc_free_all ()
{
  object_t *o = vm.objects;
  while (o != NULL)
    {
      object_t *next = o->next;
      gc_free_object (o);
      o = next;
    }
}

/* VM FUNCTIONS */

void
vm_new ()
{
  vm.top = vm.stack;
  vm.objects = NULL;
  table_new (&vm.strings);
  table_new (&vm.globals);
  block_new (&vm.block);
}

void
vm_free ()
{
  table_free (&vm.strings);
  table_free (&vm.globals);
  block_free (&vm.block);
  gc_free_all ();
}

void
vm_reset ()
{
  vm_free ();
  vm_new ();
}

void
vm_push (value_t value)
{
  *vm.top = value;
  vm.top++;
}

value_t
vm_pop ()
{
  vm.top--;
  return *vm.top;
}

/* DEBUG */

int
dbg_disassemble_operation (block_t *block, size_t offset)
{
  uint8_t constant;
  value_t value;

  opcode_t op = block->code[offset];

  switch (op)
    {
    case OP_NIL:
      printf ("NIL\n");
      return 1;
    case OP_TRUE:
      printf ("TRUE\n");
      return 1;
    case OP_FALSE:
      printf ("FALSE\n");
      return 1;
    case OP_CONSTANT:
      constant = block->code[offset + 1];
      value = block->constants.values[constant];
      printf ("CONSTANT %02x %g\n", constant, value.as.number);
      return 2;
    case OP_SET_GLOBAL:
      printf ("SET GLOBAL\n");
      return 2;
    case OP_GET_GLOBAL:
      printf ("GET GLOBAL\n");
      return 2;
    case OP_NEG:
      printf ("NEG\n");
      return 1;
    case OP_ADD:
      printf ("ADD\n");
      return 1;
    case OP_SUB:
      printf ("SUB\n");
      return 1;
    case OP_MUL:
      printf ("MUL\n");
      return 1;
    case OP_DIV:
      printf ("DIV\n");
      return 1;
    case OP_MOD:
      printf ("MOD\n");
      return 1;
    case OP_NOT:
      printf ("NOT\n");
      return 1;
    case OP_EQ:
      printf ("EQ\n");
      return 1;
    case OP_CONCAT:
      printf ("CONCAT\n");
      return 1;
    case OP_PRINT:
      printf ("PRINT\n");
      return 1;
    case OP_RETURN:
      printf ("RETURN\n");
      return 1;
    default:
      printf ("unknown op %02x", op);
      return 1;
    }
}

void
dbg_disassemble_all (block_t *block)
{
  for (size_t offset = 0; offset < block->length;)
    {
      printf ("%04zx ", offset);
      offset += dbg_disassemble_operation (block, offset);
    }
}

/* INTERPRET */

value_t
value_from_number (double n)
{
  value_t v = { TYPE_NUMBER, { .number = n } };
  return v;
}

bool
value_to_boolean (value_t v)
{
  switch (v.type)
    {
    case TYPE_NIL:
      return false;
    case TYPE_BOOL:
      return v.as.boolean;
    case TYPE_NUMBER:
      return v.as.number != 0;
    case TYPE_OBJECT:
      return true;
    }
}

bool
check_top_type (value_type_t type)
{
  return vm.top[-1].type == type;
}

bool
check_top_2_type (value_type_t type)
{
  return vm.top[-2].type == type && vm.top[-1].type == type;
}

bool
check_top_2_object_type (object_type_t type)
{
  return vm.top[-2].as.object->type == type
         && vm.top[-1].as.object->type == type;
}

void
print_value (value_t v)
{
  switch (v.type)
    {
    case TYPE_NIL:
      printf ("nil");
      break;
    case TYPE_BOOL:
      printf (v.as.boolean ? "true" : "false");
      break;
    case TYPE_NUMBER:
      printf ("%g", v.as.number);
      break;
    case TYPE_OBJECT:
      switch (v.as.object->type)
        {
        case OBJECT_STRING:
          {
            string_t *s = (string_t *)v.as.object;
            printf ("\"%s\"", s->chars);
            break;
          }
        }
      break;
    }
}

#define BINARY_OP(o)                                                          \
  do                                                                          \
    {                                                                         \
      if (!check_top_2_type (TYPE_NUMBER))                                    \
        return RESULT_RUNTIME_ERROR;                                          \
      double b = vm_pop ().as.number;                                         \
      double a = vm_pop ().as.number;                                         \
      vm_push (value_from_number (a o b));                                    \
    }                                                                         \
  while (0)

result_t
run ()
{
  uint8_t op;
  value_t v;
  string_t *k;

  while (1)
    {
#ifdef DEBUG
      for (value_t *v = vm.stack; v < vm.top; v++)
        {
          printf ("[");
          print_value (*v);
          printf ("]");
        }
      printf ("\n");
      dbg_disassemble_operation (&vm.block, (int)(vm.pc - vm.block.code));
#endif
      switch (op = *vm.pc++)
        {
        case OP_NIL:
          vm_push ((value_t){ .type = TYPE_NIL });
          break;
        case OP_TRUE:
          vm_push ((value_t){ .type = TYPE_BOOL, .as = true });
          break;
        case OP_FALSE:
          vm_push ((value_t){ .type = TYPE_BOOL, .as = false });
          break;
        case OP_CONSTANT:
          v = vm.block.constants.values[*vm.pc++];
          printf ("%g\n", v.as.number);
          vm_push (v);
          break;
        case OP_SET_GLOBAL:
          v = vm.block.constants.values[*vm.pc++];
          k = (string_t *)v.as.object;
          table_set (&vm.globals, k, vm_pop ());
          break;
        case OP_GET_GLOBAL:
          v = vm.block.constants.values[*vm.pc++];
          k = (string_t *)v.as.object;
          vm_push (table_get (&vm.globals, k)->value);
          break;
        case OP_NEG:
          if (!check_top_type (TYPE_NUMBER))
            return RESULT_RUNTIME_ERROR;
          vm_push (value_from_number (-vm_pop ().as.number));
          break;
        case OP_ADD:
          BINARY_OP (+);
          break;
        case OP_SUB:
          BINARY_OP (-);
          break;
        case OP_MUL:
          BINARY_OP (*);
          break;
        case OP_DIV:
          BINARY_OP (/);
          break;
        case OP_MOD:
          {
            if (!check_top_2_type (TYPE_NUMBER))
              return RESULT_RUNTIME_ERROR;
            double b = vm_pop ().as.number;
            double a = vm_pop ().as.number;
            vm_push (value_from_number (fmod (a, b)));
            break;
          }
        case OP_NOT:
          {
            vm_push ((value_t){ .type = TYPE_BOOL,
                                .as = !value_to_boolean (vm_pop ()) });
            break;
          }
        case OP_EQ:
          {
            if (!check_top_2_type (TYPE_NUMBER))
              return RESULT_RUNTIME_ERROR;
            double b = vm_pop ().as.number;
            double a = vm_pop ().as.number;
            vm_push ((value_t){ .type = TYPE_BOOL, .as = a == b });
            break;
          }
        case OP_CONCAT:
          {
            if (!check_top_2_type (TYPE_OBJECT))
              return RESULT_RUNTIME_ERROR;
            if (!check_top_2_object_type (OBJECT_STRING))
              return RESULT_RUNTIME_ERROR;

            string_t *b = (string_t *)vm_pop ().as.object;
            string_t *a = (string_t *)vm_pop ().as.object;

            object_t *o = (object_t *)string_concat_and_allocate (
                a->chars, a->length, b->chars, b->length);
            vm_push ((value_t){ .type = TYPE_OBJECT, .as.object = o });
            break;
          }
        case OP_PRINT:
          {
            print_value (vm_pop ());
            printf ("\n");
            break;
          }
        case OP_RETURN:
          // print_value (pop (vm));
          // printf ("\n");
          return RESULT_OK;
        }
    }
}

void
scan_new (const char *source)
{
  scan.start = source;
  scan.current = source;
}

bool
is_whitespace (char c)
{
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

bool
is_word (char c)
{
  return c != '(' && c != ')' && !is_whitespace (c);
}

bool
is_digit (char c)
{
  return '0' <= c && c <= '9';
}

bool
is_number (const char *word_start, const char *word_end)
{
  const char *c = word_start;

  while (is_digit (*c) && c < word_end)
    c++;

  return c == word_end;
}

void
ignore_whitespace ()
{
  while (1)
    {
      char c = *scan.current;
      if (!is_whitespace (c))
        return;
      scan.current++;
    }
}

token_t
token_create (token_type_t type)
{
  token_t token = { .type = type,
                    .start = scan.start,
                    .length = scan.current - scan.start };
  return token;
}

token_t
token_create_string ()
{
  char p = '"';
  char c;
  token_t t;
  while (1)
    {
      if ((c = *scan.current++) == '\0')
        {
          fprintf (stderr, "Missing quote in string");
          exit (1);
        }
      if (p != '\\' && c == '"')
        break;
      p = c;
    }
  t = token_create (TOKEN_STRING);
  t.start += 1;
  t.length -= 2;
  return t;
}

token_t
scan_token ()
{
  ignore_whitespace ();
  scan.start = scan.current;

  if (*scan.current == '\0')
    return token_create (TOKEN_END);

  char c = *scan.current++;
  switch (c)
    {
    case '(':
      return token_create (TOKEN_LPAREN);
    case ')':
      return token_create (TOKEN_RPAREN);
    case '"':
      return token_create_string ();
    }

  while (is_word (*scan.current))
    scan.current++;

  if (is_number (scan.start, scan.current))
    return token_create (TOKEN_NUMBER);

  return token_create (TOKEN_WORD);
}

bool
is_token_string (token_t token, const char *str)
{
  int len = strlen (str);
  bool same_len = token.length == len;
  bool same_str = 0 == strncmp (str, token.start, len);
  return same_str && same_len;
}

bool
is_token_set_global (token_t token)
{
  return token.length > 1 && token.start[token.length - 1] == ':';
}

opcode_t
is_token_op (token_t token)
{
  if (token.length == 1)
    {
      switch (*token.start)
        {
        case '+':
          return OP_ADD;
        case '-':
          return OP_SUB;
        case '*':
          return OP_MUL;
        case '/':
          return OP_DIV;
        case '%':
          return OP_MOD;
        case '=':
          return OP_EQ;
        }
    }
  if (is_token_string (token, ".."))
    return OP_CONCAT;
  if (is_token_string (token, "print"))
    return OP_PRINT;
  if (is_token_string (token, "not"))
    return OP_NOT;
  if (is_token_string (token, "nil"))
    return OP_NIL;
  if (is_token_string (token, "true"))
    return OP_TRUE;
  if (is_token_string (token, "false"))
    return OP_FALSE;

  return OP_ERROR;
}

bool
emit_word (token_t token, block_t *block)
{
  if (is_token_set_global (token))
    {
      token.length--; // don't include the ':'
      value_t k = { .type = TYPE_OBJECT,
                    .as.object = (object_t *)string_copy ((char *)token.start,
                                                          token.length) };
      block_push_constant (block, k, OP_SET_GLOBAL);
      return true;
    }

  opcode_t op = is_token_op (token);
  if (op == OP_ERROR)
    {
      // fprintf (stderr, "Unrecognized word '%.*s'\n", token.length,
      //          token.start);
      // return false;
      value_t k = { .type = TYPE_OBJECT,
                    .as.object = (object_t *)string_copy ((char *)token.start,
                                                          token.length) };
      block_push_constant (block, k, OP_GET_GLOBAL);
      return true;
    }

  block_push (block, op);

#ifdef DEBUG
  printf ("emit byte '%.*s'\n", token.length, token.start);
#endif
  return true;
}

void
emit_number (token_t token, block_t *block)
{
  double n = strtod (token.start, NULL);

  block_push_constant (block, value_from_number (n), OP_CONSTANT);
}

void
emit_string (token_t token, block_t *block)
{
  value_t v = { .type = TYPE_OBJECT,
                .as.object = (object_t *)string_copy ((char *)token.start,
                                                      token.length) };

  block_push_constant (block, v, OP_CONSTANT);

#ifdef DEBUG
  printf ("string '%.*s'\n", token.length, token.start);
#endif
}

bool
parse_expression (token_t token, block_t *block)
{
  switch (token.type)
    {
    case TOKEN_RPAREN:
      {
        fprintf (stderr, "Unexpected ')'\n");
        return false;
      }
    case TOKEN_LPAREN:
      {
        token_t first_token = scan_token ();

        if (first_token.type == TOKEN_RPAREN)
          return true;

        if (first_token.type == TOKEN_END)
          {
            fprintf (stderr, "Missing ')'\n");
            return false;
          }

        if (first_token.type != TOKEN_WORD)
          {
            fprintf (stderr, "Expression must start with a word\n");
            return false;
          }

        do
          {
            token = scan_token ();
            if (token.type == TOKEN_RPAREN || token.type == TOKEN_END)
              break;
            if (!parse_expression (token, block))
              return false;
          }
        while (1);

        if (!emit_word (first_token, block))
          return false;

        if (token.type != TOKEN_RPAREN)
          {
            fprintf (stderr, "Missing ')'\n");
            return false;
          }
        return true;
      }
    case TOKEN_WORD:
      {
        if (!emit_word (token, block))
          return false;
        return true;
      }
    case TOKEN_NUMBER:
      {
        emit_number (token, block);
        return true;
      }
    case TOKEN_STRING:
      {
        emit_string (token, block);
        return true;
      }
    case TOKEN_END:
      {
        block_push (block, OP_RETURN);
        return true;
      }
    }

  fprintf (stderr, "Unrecognized token\n");
  return false;
}

bool
compile_block (const char *source, block_t *block)
{
  scan_new (source);
  while (1)
    {
      token_t token = scan_token ();

      if (!parse_expression (token, block))
        return false;

      if (token.type == TOKEN_END)
        break;
    }
  return true;
}

result_t
interpret (char *source)
{
  result_t result;

  if (!compile_block (source, &vm.block))
    return RESULT_COMPILE_ERROR;

#ifdef DEBUG
  dbg_disassemble_all (&vm.block);
#endif

  vm.pc = vm.block.code;
  result = run ();

  return result;
};

void
repl ()
{
  char line[1024];
  while (1)
    {
      printf (": ");

      if (!fgets (line, sizeof (line), stdin))
        {
          printf ("\n");
          break;
        }

      interpret (line);
      vm_reset ();
    }
}

char *
read (const char *path)
{
  FILE *file = fopen (path, "rb");
  if (file == NULL)
    {
      fprintf (stderr, "Couldn't open '%s'\n", path);
      exit (1);
    }

  fseek (file, 0, SEEK_END);
  size_t size = ftell (file);
  rewind (file);

  char *buffer = malloc (size - 1);
  if (buffer == NULL)
    {
      fprintf (stderr, "Out of memory\n");
      exit (1);
    }
  size_t bytes = fread (buffer, sizeof (char), size, file);
  if (bytes < size)
    {
      fprintf (stderr, "Read error\n");
      exit (1);
    }
  buffer[bytes] = '\0';

  fclose (file);
  return buffer;
}

void
run_file (const char *path)
{
  char *source = read (path);
  result_t result = interpret (source);
  free (source);

  switch (result)
    {
    case RESULT_OK:
      break;
    case RESULT_COMPILE_ERROR:
      printf ("Compile error\n");
      exit (1);
    case RESULT_RUNTIME_ERROR:
      printf ("Runtime error\n");
      exit (1);
    }
}

void
init_message ()
{
  puts ("  ,  \n"
        " / \\ \n"
        "(_\"_)\n");
}

/* MAIN */

int
main (int argc, const char *argv[])
{
  vm_new ();

  init_message ();
  if (argc == 1)
    repl ();
  else if (argc == 2)
    run_file (argv[1]);
  else
    {
      fprintf (stderr, "Usage: pera [file_path]\n");
      exit (1);
    }

  vm_free ();
  return 0;
}
