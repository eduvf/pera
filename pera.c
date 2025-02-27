#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG
#define FRAMES_MAX 64
#define STACK_SIZE (FRAMES_MAX * 256)
#define UINT8_OVER 256
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
  OBJECT_FUNCTION,
  OBJECT_CLOSURE,
} object_type_t;

typedef struct object
{
  object_type_t type;
  struct object *next;
} object_t;

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
  object_t object;
  int length;
  uint32_t hash;
  char *chars;
} string_t;

typedef enum
{
  FUNCTION_TOP_LEVEL,
  FUNCTION_USER_DEFINED,
} function_type_t;

typedef struct
{
  object_t object;
  int arity;
  block_t block;
  string_t *name;
} function_t;

typedef struct
{
  object_t object;
  function_t *function;
} closure_t;

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
  OP_SET_LOCAL,
  OP_GET_LOCAL,
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
  OP_POP,
  OP_LOOP,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_END_SCOPE,
  OP_CLOSURE,
  OP_CALL,
  OP_RETURN,
  OP_NOT_BUILTIN,
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
  token_t name;
  int depth;
} local_t;

typedef struct compiler
{
  struct compiler *outer;
  function_t *function;
  function_type_t type;
  local_t locals[UINT8_OVER];
  int local_count;
  int scope_depth;
} compiler_t;

typedef struct
{
  closure_t *closure;
  uint8_t *pc;
  value_t *slots;
} call_t;

typedef struct
{
  call_t calls[FRAMES_MAX];
  int call_count;
  // uint8_t *pc;
  value_t stack[STACK_SIZE];
  value_t *top;
  table_t strings;
  table_t globals;
  object_t *objects;
} vm_t;

/* GLOBALS */

compiler_t *current;
scan_t scan;
vm_t vm;

/* COMPARE VALUES */

bool
value_objects_are_equal (value_t v1, value_t v2)
{
  object_t *o1 = v1.as.object;
  object_t *o2 = v2.as.object;

  return o1->type == o2->type && o1 == o2;
}

bool
value_are_equal (value_t v1, value_t v2)
{
  if (v1.type != v2.type)
    return false;

  switch (v1.type)
    {
    case TYPE_NIL:
      return true;
    case TYPE_BOOL:
      return v1.as.boolean == v2.as.boolean;
    case TYPE_NUMBER:
      return v1.as.number == v2.as.number;
    case TYPE_OBJECT:
      return value_objects_are_equal (v1, v2);
    }
}

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

int
array_find (array_t *array, value_t value)
{
  for (int i = 0; i < array->length; i++)
    {
      value_t array_value = array->values[i];
      if (value_are_equal (value, array_value))
        return i;
    }
  return -1;
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

block_t *
get_block ()
{
  return &current->function->block;
}

void
block_push (uint8_t byte)
{
  block_t *block = get_block ();
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
block_add_constant (value_t value)
{
  block_t *block = get_block ();
  int i = array_find (&block->constants, value);
  if (i >= 0)
    return i;

  array_push (&block->constants, value);
  return block->constants.length - 1;
}

void
block_push_constant (value_t value, opcode_t op)
{
  int constant = block_add_constant (value);
  if (constant > UINT8_MAX)
    {
      fprintf (stderr, "Too many constants in block.\n");
      exit (1);
    }

  block_push (op);
  block_push (constant);
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

/* OBJECT FUNCTIONS */

size_t
object_sizeof (object_type_t type)
{
  switch (type)
    {
    case OBJECT_STRING:
      return sizeof (string_t);
    case OBJECT_FUNCTION:
      return sizeof (function_t);
    case OBJECT_CLOSURE:
      return sizeof (closure_t);
    }
}

object_t *
object_new (object_type_t type)
{
  size_t size = object_sizeof (type);

  object_t *o = malloc (size);
  o->type = type;
  o->next = vm.objects;
  vm.objects = o;
  return o;
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
  string_t *s = (string_t *)object_new (OBJECT_STRING);

  s->length = length;
  s->chars = chars;
  s->hash = hash_from_string (s->chars, length);

  string_t *interned = table_find_string (&vm.strings, s);
  if (interned != NULL)
    {
      string_free (s);
      return interned;
    }

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

/* FUNCTION FUNCTIONS */

void
function_free (function_t *f)
{
  block_free (&f->block);
  free (f);
}

function_t *
function_new ()
{
  function_t *f = (function_t *)object_new (OBJECT_FUNCTION);

  f->arity = 0;
  f->name = NULL;
  block_new (&f->block);

  return f;
}

/* CLOSURE FUNCTIONS */

closure_t *
closure_new (function_t *function)
{
  closure_t *closure = (closure_t *)object_new (OBJECT_CLOSURE);
  closure->function = function;
  return closure;
}

void
closure_free (closure_t *closure)
{
  free (closure);
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
    case OBJECT_FUNCTION:
      {
        function_t *function = (function_t *)object;
        function_free (function);
        break;
      }
    case OBJECT_CLOSURE:
      {
        closure_t *closure = (closure_t *)object;
        closure_free (closure);
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

/* COMPILER FUNCTIONS */

void
compiler_new (compiler_t *compiler, function_type_t type)
{
  compiler->outer = current;
  compiler->function = function_new ();
  compiler->type = type;
  compiler->local_count = 0;
  compiler->scope_depth = 0;

  local_t *local = &compiler->locals[compiler->local_count++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;

  current = compiler;
}

function_t *
compiler_end ()
{
  block_push (OP_RETURN);

  function_t *f = current->function;
  current = current->outer;
  return f;
}

void
compiler_scope_create ()
{
  current->scope_depth++;
}

void
compiler_scope_delete ()
{
  current->scope_depth--;

  uint8_t n = current->local_count;

  while (current->local_count > 0
         && current->locals[current->local_count - 1].depth
                > current->scope_depth)
    current->local_count--;

  n -= current->local_count;

  if (n > 1)
    {
      block_push (OP_END_SCOPE);
      block_push (n);
    }
}

/* VM FUNCTIONS */

void
vm_new ()
{
  vm.top = vm.stack;
  vm.objects = NULL;
  vm.call_count = 0;
  table_new (&vm.strings);
  table_new (&vm.globals);
}

void
vm_free ()
{
  table_free (&vm.strings);
  table_free (&vm.globals);
  gc_free_all ();
}

void
vm_reset ()
{
  current->function->block.length = 0;
  vm.objects = NULL;
  vm.top = vm.stack;
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

value_t
vm_peek ()
{
  return *(vm.top - 1);
}

/* DEBUG */

int
dbg_disassemble_operation (size_t offset)
{
  block_t *block = get_block ();
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
    case OP_SET_LOCAL:
      printf ("SET LOCAL %d\n", block->code[offset + 1]);
      return 2;
    case OP_GET_LOCAL:
      printf ("GET LOCAL %d\n", block->code[offset + 1]);
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
    case OP_POP:
      printf ("POP\n");
      return 1;
    case OP_LOOP:
      printf ("LOOP\n");
      return 3;
    case OP_JUMP:
      printf ("JUMP\n");
      return 3;
    case OP_JUMP_IF_FALSE:
      printf ("JUMP IF FALSE\n");
      return 3;
    case OP_END_SCOPE:
      printf ("END SCOPE %d\n", block->code[offset + 1]);
      return 2;
    case OP_CLOSURE:
      printf ("CLOSURE %d\n", block->code[offset + 1]);
      return 2;
    case OP_CALL:
      printf ("CALL\n");
      return 2;
    case OP_RETURN:
      printf ("RETURN\n");
      return 1;
    default:
      printf ("unknown op %02x", op);
      return 1;
    }
}

void
dbg_disassemble_all ()
{
  block_t *block = get_block ();
  for (size_t offset = 0; offset < block->length;)
    {
      printf ("%04zx ", offset);
      offset += dbg_disassemble_operation (offset);
    }
}

void print_value (value_t v);

void
dbg_print_stack ()
{
  for (value_t *v = vm.stack; v < vm.top; v++)
    {
      printf ("[");
      print_value (*v);
      printf ("]");
    }
  printf ("\n");
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
        case OBJECT_FUNCTION:
          {
            function_t *f = (function_t *)v.as.object;
            if (f->name == NULL)
              printf ("<main>");
            else
              printf ("<fn %s>", f->name->chars);
            break;
          }
        case OBJECT_CLOSURE:
          {
            closure_t *c = (closure_t *)v.as.object;
            value_t cf = (value_t){ .type = TYPE_OBJECT,
                                    .as.object = (object_t *)c->function };
            print_value (cf);
            break;
          }
        }
      break;
    }
}

bool
call_value (value_t callee, int arg_num)
{
  if (callee.type != TYPE_OBJECT || callee.as.object->type != OBJECT_CLOSURE)
    {
      printf ("Can't call '");
      print_value (callee);
      printf ("' because it's not a function\n");
      return false;
    }

  call_t *call = &vm.calls[vm.call_count++];
  closure_t *c = (closure_t *)callee.as.object;
  int arity = c->function->arity;
  call->closure = c;
  call->pc = c->function->block.code;
  call->slots = vm.top - arg_num - 1;

  if (arity != arg_num)
    {
      fprintf (stderr, "Expected %d arguments, got %d\n", arity, arg_num);
      return false;
    }

  if (vm.call_count == FRAMES_MAX)
    {
      fprintf (stderr, "Stack overflow\n");
      return false;
    }

  return true;
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
  call_t *call = &vm.calls[vm.call_count - 1];
  uint8_t op;

  while (1)
    {
#ifdef DEBUG
      dbg_print_stack ();
      dbg_disassemble_operation ((int)(call->pc - get_block ()->code));
#endif

      switch (op = *call->pc++)
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
          {
            value_t v = get_block ()->constants.values[*call->pc++];
            printf ("%g\n", v.as.number);
            vm_push (v);
            break;
          }
        case OP_SET_GLOBAL:
          {
            value_t v = get_block ()->constants.values[*call->pc++];
            string_t *k = (string_t *)v.as.object;
            table_set (&vm.globals, k, vm_pop ());
            break;
          }
        case OP_GET_GLOBAL:
          {
            value_t v = get_block ()->constants.values[*call->pc++];
            string_t *k = (string_t *)v.as.object;
            vm_push (table_get (&vm.globals, k)->value);
            break;
          }
        case OP_SET_LOCAL:
          {
            uint8_t offset = *call->pc++;
            call->slots[offset] = vm_peek ();
            break;
          }
        case OP_GET_LOCAL:
          {
            uint8_t offset = *call->pc++;
            vm_push (call->slots[offset]);
            break;
          }
        case OP_NEG:
          {
            if (!check_top_type (TYPE_NUMBER))
              return RESULT_RUNTIME_ERROR;
            vm_push (value_from_number (-vm_pop ().as.number));
            break;
          }
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
            value_t b = vm_pop ();
            value_t a = vm_pop ();
            bool result = value_are_equal (a, b);
            vm_push ((value_t){ .type = TYPE_BOOL, .as = result });
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
        case OP_POP:
          {
            vm_pop ();
            break;
          }
        case OP_LOOP:
          {
            call->pc += 2;
            uint16_t offset = (call->pc[-2] << 8) | call->pc[-1];
            call->pc -= offset;
            break;
          }
        case OP_JUMP:
          {
            call->pc += 2;
            uint16_t offset = (call->pc[-2] << 8) | call->pc[-1];
            call->pc += offset;
            break;
          }
        case OP_JUMP_IF_FALSE:
          {
            call->pc += 2;
            uint16_t offset = (call->pc[-2] << 8) | call->pc[-1];
            if (!value_to_boolean (vm_peek ()))
              call->pc += offset;
            break;
          }
        case OP_END_SCOPE:
          {
            uint8_t n = *call->pc++;
            *(vm.top - n - 1) = vm_peek ();
            vm.top -= n;
            break;
          }
        case OP_CLOSURE:
          {
            value_t v = get_block ()->constants.values[*call->pc++];
            function_t *f = (function_t *)v.as.object;
            closure_t *c = closure_new (f);
            object_t *o = (object_t *)c;
            vm_push ((value_t){ .type = TYPE_OBJECT, .as.object = o });
            break;
          }
        case OP_CALL:
          {
            uint8_t arg_num = *call->pc++;
            if (!call_value (vm_pop (), arg_num))
              return RESULT_RUNTIME_ERROR;
            call = &vm.calls[vm.call_count - 1];
            break;
          }
        case OP_RETURN:
          {
            value_t v = vm_pop ();
            vm.call_count--;
            if (vm.call_count == 0)
              return RESULT_OK;

            vm.top = call->slots;
            vm_push (v);
            call = &vm.calls[vm.call_count - 1];
            break;
          }
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
is_token_equal_to (token_t *a, token_t *b)
{
  if (a->length != b->length)
    return false;
  return memcmp (a->start, b->start, a->length) == 0;
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

  return OP_NOT_BUILTIN;
}

void
local_set_new (token_t token)
{
  local_t *local = &current->locals[current->local_count++];
  local->name = token;
  local->depth = current->scope_depth;
}

void
emit_set_local (token_t token)
{
  if (current->local_count == UINT8_OVER)
    {
      fprintf (stderr, "Too many locals\n");
      return;
    }

  for (int i = current->local_count - 1; i >= 0; i--)
    {
      local_t *local = &current->locals[i];
      if (local->depth != -1 && local->depth < current->scope_depth)
        break;

      if (is_token_equal_to (&token, &local->name))
        {
          block_push (OP_SET_LOCAL);
          block_push (i);
          block_push (OP_POP);
          return;
        }
    }

  local_set_new (token);
  block_push (OP_SET_LOCAL);
  block_push (current->local_count - 1);
}

int
find_local (token_t *token)
{
  for (int i = current->local_count - 1; i >= 0; i--)
    {
      local_t *local = &current->locals[i];
      if (is_token_equal_to (token, &local->name))
        return i;
    }
  return -1;
}

bool
emit_get_local (token_t token)
{
  int n = find_local (&token);
  if (n == -1)
    {
      fprintf (stderr, "Couldn't find '%.*s'\n", token.length, token.start);
      return false;
    }

  block_push (OP_GET_LOCAL);
  block_push (n);
  return true;
}

void
emit_set_global (token_t token)
{
  string_t *s = string_copy ((char *)token.start, token.length);
  value_t k = { .type = TYPE_OBJECT, .as.object = (object_t *)s };
  block_push_constant (k, OP_SET_GLOBAL);
}

bool
emit_get_global (token_t token)
{
  string_t *s = string_copy ((char *)token.start, token.length);
  value_t k = { .type = TYPE_OBJECT, .as.object = (object_t *)s };

  pair_t *p = table_get (&vm.globals, s);
  if (p->key == NULL)
    {
      fprintf (stderr, "Couldn't find '%.*s'\n", token.length, token.start);
      return false;
    }

  block_push_constant (k, OP_GET_GLOBAL);
  return true;
}

bool
emit_word (token_t token)
{
  bool found = (*token.start == '_') ? emit_get_global (token)
                                     : emit_get_local (token);

#ifdef DEBUG
  printf ("emit word '%.*s'\n", token.length, token.start);
#endif
  return found;
}

bool
emit_op (token_t token, int arg_num)
{
  opcode_t op = is_token_op (token);
  if (op == OP_NOT_BUILTIN)
    {
      bool found = emit_word (token);
      if (!found)
        return false;

      if (arg_num > 255)
        {
          fprintf (stderr, "Functions cannot have >255 parameters\n");
          return false;
        }

      block_push (OP_CALL);
      block_push (arg_num);
      return true;
    }

  block_push (op);

#ifdef DEBUG
  printf ("emit op '%.*s'\n", token.length, token.start);
#endif
  return true;
}

void
emit_number (token_t token)
{
  double n = strtod (token.start, NULL);

  block_push_constant (value_from_number (n), OP_CONSTANT);
}

void
emit_string (token_t token)
{
  value_t v = { .type = TYPE_OBJECT,
                .as.object = (object_t *)string_copy ((char *)token.start,
                                                      token.length) };

  block_push_constant (v, OP_CONSTANT);

#ifdef DEBUG
  printf ("string '%.*s'\n", token.length, token.start);
#endif
}

bool parse_expression (token_t token);

bool
parse_multiple_expressions (token_t token, int *counter)
{
  do
    {
      token = scan_token ();
      if (token.type == TOKEN_RPAREN || token.type == TOKEN_END)
        break;
      if (!parse_expression (token))
        return false;
      (*counter)++;
    }
  while (1);

  if (token.type != TOKEN_RPAREN)
    {
      fprintf (stderr, "Missing ')'\n");
      return false;
    }

  return true;
}

bool
parse_do_form (token_t token)
{
  int arg_num = 0;
  compiler_scope_create ();

  if (!parse_multiple_expressions (token, &arg_num))
    return false;

  compiler_scope_delete ();
  return true;
}

bool
parse_on_form ()
{
  token_t next_token = scan_token ();
  token_t name;
  int body_expr_num;

  compiler_t compiler;
  compiler_new (&compiler, FUNCTION_USER_DEFINED);
  compiler_scope_create ();

  if (next_token.type != TOKEN_LPAREN)
    {
      fprintf (stderr, "Expected '(' to begin function declaration\n");
      return false;
    }

  next_token = scan_token ();
  if (next_token.type != TOKEN_WORD)
    {
      fprintf (stderr, "Expected name within function declaration\n");
      return false;
    }

  name = next_token;

  while ((next_token = scan_token ()).type == TOKEN_WORD)
    {
      current->function->arity++;
      if (current->function->arity > 255)
        {
          fprintf (stderr, "Functions cannot have >255 parameters\n");
          return false;
        }

      local_set_new (next_token);
    }

  if (next_token.type != TOKEN_RPAREN)
    {
      fprintf (stderr, "Expected ')' to end function declaration\n");
      return false;
    }

  parse_multiple_expressions (next_token, &body_expr_num);

  function_t *f = compiler_end ();
  f->name = string_copy ((char *)name.start, name.length);

  value_t v = { .type = TYPE_OBJECT, .as.object = (object_t *)f };
  block_push_constant (v, OP_CLOSURE);
  emit_set_local (name);

  return true;
}

bool
parse_put_form ()
{
  token_t key = scan_token ();
  token_t next_token = scan_token ();

  if (key.type != TOKEN_WORD)
    {
      fprintf (stderr, "First argument to 'put' must be a word\n");
      return false;
    }

  if (next_token.type == TOKEN_END)
    {
      fprintf (stderr, "Unexpected EOF\n");
      return false;
    }

  if (next_token.type == TOKEN_RPAREN)
    block_push (OP_NIL);
  else if (!parse_expression (next_token))
    return false;

  next_token = scan_token ();
  if (next_token.type != TOKEN_RPAREN)
    {
      fprintf (stderr, "Missing ')' or too much arguments for 'put'\n");
      return false;
    }

  // if key starts with '_', make it global
  if (*key.start == '_')
    emit_set_global (key);
  else
    emit_set_local (key);

  return true;
}

int
emit_jump (opcode_t op)
{
  block_t *block = get_block ();
  block_push (op);
  block_push (0);
  block_push (0);
  return block->length - 2;
}

void
patch_jump (int offset)
{
  block_t *block = get_block ();
  int jump = block->length - offset - 2;

  if (jump > UINT16_MAX)
    {
      fprintf (stderr, "'if' jump is too large\n");
      exit (1);
    }

  block->code[offset] = (jump >> 8) & 0xff;
  block->code[offset + 1] = jump & 0xff;
}

bool
parse_if_form ()
{
  token_t token = scan_token ();
  if (!parse_expression (token))
    return false;

  int then_offset = emit_jump (OP_JUMP_IF_FALSE);

  block_push (OP_POP);

  token = scan_token ();
  if (!parse_expression (token))
    return false;

  int else_offset = emit_jump (OP_JUMP);

  patch_jump (then_offset);

  token = scan_token ();
  if (token.type == TOKEN_RPAREN)
    return true;

  block_push (OP_POP);

  if (!parse_expression (token))
    return false;

  patch_jump (else_offset);

  token = scan_token ();
  if (token.type != TOKEN_RPAREN)
    {
      fprintf (stderr, "Missing ')' or too much arguments for 'if'\n");
      return false;
    }

  return true;
}

void
emit_loop (int start)
{
  block_t *block = get_block ();
  block_push (OP_LOOP);

  int offset = block->length - start + 2;
  if (offset > UINT16_MAX)
    {
      fprintf (stderr, "'while' jump is too large\n");
      exit (1);
    }

  block_push ((offset >> 8) & 0xff);
  block_push (offset & 0xff);
}

bool
parse_while_form ()
{
  block_t *block = get_block ();
  int start_offset = block->length;

  token_t token = scan_token ();
  if (!parse_expression (token))
    return false;

  int end_loop_offset = emit_jump (OP_JUMP_IF_FALSE);

  block_push (OP_POP);

  token = scan_token ();
  if (!parse_expression (token))
    return false;

  emit_loop (start_offset);

  patch_jump (end_loop_offset);

  block_push (OP_POP);

  token = scan_token ();
  if (token.type != TOKEN_RPAREN)
    {
      fprintf (stderr, "Missing ')' or too much arguments for 'if'\n");
      return false;
    }

  return true;
}

bool
parse_expression (token_t token)
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
        int arg_num = 0;

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

        if (is_token_string (first_token, "do"))
          return parse_do_form (token);

        if (is_token_string (first_token, "on"))
          return parse_on_form ();

        if (is_token_string (first_token, "put"))
          return parse_put_form ();

        if (is_token_string (first_token, "if"))
          return parse_if_form ();

        if (is_token_string (first_token, "while"))
          return parse_while_form ();

        if (!parse_multiple_expressions (token, &arg_num))
          return false;

        if (!emit_op (first_token, arg_num))
          return false;

        return true;
      }
    case TOKEN_WORD:
      {
        if (!emit_word (token))
          return false;
        return true;
      }
    case TOKEN_NUMBER:
      {
        emit_number (token);
        return true;
      }
    case TOKEN_STRING:
      {
        emit_string (token);
        return true;
      }
    case TOKEN_END:
      {
        block_push (OP_RETURN);
        return true;
      }
    }

  fprintf (stderr, "Unrecognized token\n");
  return false;
}

function_t *
compile_block (const char *source)
{
  scan_new (source);
  while (1)
    {
      token_t token = scan_token ();

      if (!parse_expression (token))
        return NULL;

      if (token.type == TOKEN_END)
        break;
    }
  return current->function;
}

result_t
interpret (char *source)
{
  function_t *f = compile_block (source);
  if (f == NULL)
    return RESULT_COMPILE_ERROR;

  closure_t *c = closure_new (f);
  vm_push ((value_t){ .type = TYPE_OBJECT, .as.object = (object_t *)c });

  call_t *call = &vm.calls[vm.call_count++];
  call->closure = c;
  call->pc = c->function->block.code;
  call->slots = vm.stack;

#ifdef DEBUG
  dbg_disassemble_all ();
#endif

  return run ();
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

  if (size == 0)
    {
      fprintf (stderr, "Empty file\n");
      exit (1);
    }

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
  compiler_t compiler;

  vm_new ();
  compiler_new (&compiler, FUNCTION_TOP_LEVEL);

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
