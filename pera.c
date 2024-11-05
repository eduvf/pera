#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG
#define STACK_SIZE 256

typedef double value_t;

typedef enum
{
  OP_CONSTANT,
  OP_NEG,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD,
  OP_RETURN,
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
  TOKEN_END,
  TOKEN_ERROR,
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

scan_t scan;

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
} vm_t;

/* ARRAY FUNCTIONS */

void
array_new (array_t *array)
{
  array->length = 0;
  array->capacity = 8;
  array->values = malloc (8);
}

void
array_push (array_t *array, value_t value)
{
  if (array->capacity < array->length + 1)
    {
      array->capacity *= 2;
      array->values = realloc (array->values, array->capacity);
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
  block->code = malloc (8);
  array_new (&block->constants);
}

void
block_push (block_t *block, uint8_t byte)
{
  if (block->capacity < block->length + 1)
    {
      block->capacity *= 2;
      block->code = realloc (block->code, block->capacity);
      if (block->code == NULL)
        exit (1);
    }

  block->code[block->length] = byte;
  block->length++;
}

uint8_t
block_add_constant (block_t *block, value_t value)
{
  array_push (&block->constants, value);
  return block->constants.length - 1;
}

void
block_free (block_t *block)
{
  free (block->code);
  array_free (&block->constants);
}

/* VM FUNCTIONS */

void
vm_new (vm_t *vm)
{
  vm->top = vm->stack;
  block_new (&vm->block);
}

void
vm_free (vm_t *vm)
{
  block_free (&vm->block);
}

void
push (vm_t *vm, value_t value)
{
  *vm->top = value;
  vm->top++;
}

value_t
pop (vm_t *vm)
{
  vm->top--;
  return *vm->top;
}

/* DEBUG */

int
disassemble_operation (block_t *block, size_t offset)
{
  uint8_t constant;
  value_t value;

  opcode_t op = block->code[offset];

  switch (op)
    {
    case OP_CONSTANT:
      constant = block->code[offset + 1];
      value = block->constants.values[constant];
      printf ("CONSTANT %02x %g\n", constant, value);
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
    case OP_RETURN:
      printf ("RETURN\n");
      return 1;
    default:
      printf ("unknown op %02x", op);
      return 1;
    }
}

void
disassemble (block_t *block)
{
  for (size_t offset = 0; offset < block->length;)
    {
      printf ("%04zx ", offset);
      offset += disassemble_operation (block, offset);
    }
}

/* INTERPRET */

#define BINARY_OP(o)                                                          \
  do                                                                          \
    {                                                                         \
      double b = pop (vm);                                                    \
      double a = pop (vm);                                                    \
      push (vm, a o b);                                                       \
    }                                                                         \
  while (0)

static result_t
run (vm_t *vm)
{
  uint8_t op;
  value_t v;

  while (1)
    {
#ifdef DEBUG
      for (value_t *v = vm->stack; v < vm->top; v++)
        printf ("[%g]", *v);
      printf ("\n");
      disassemble_operation (&vm->block, (int)(vm->pc - vm->block.code));
#endif
      switch (op = *vm->pc++)
        {
        case OP_CONSTANT:
          v = vm->block.constants.values[*vm->pc++];
          printf ("%g\n", v);
          push (vm, v);
          break;
        case OP_NEG:
          push (vm, -pop (vm));
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
            double b = pop (vm);
            double a = pop (vm);
            push (vm, fmod (a, b));
            break;
          }
        case OP_RETURN:
          printf ("%g\n", pop (vm));
          return RESULT_OK;
        }
    }
}

// result_t
// interpret (vm_t *vm, block_t *block)
// {
//   vm->block = *block;
//   vm->pc = vm->block.code;
//   return run (vm);
// }

void
scan_new (const char *source)
{
  scan.start = source;
  scan.current = source;
}

void
ignore_whitespace ()
{
  while (1)
    {
      char c = *scan.current;
      switch (c)
        {
        case ' ':
        case '\r':
        case '\n':
        case '\t':
          scan.current++;
          break;
        default:
          return;
        }
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
token_error_create (const char *message)
{
  token_t token
      = { .type = TOKEN_ERROR, .start = message, .length = strlen (message) };
  return token;
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
    }

  return token_error_create ("Unexpected character");
}

void
compile (const char *source)
{
  scan_new (source);
  while (1)
    {
      token_t token = scan_token ();
      printf ("%d '%.*s'\n", token.type, token.length, token.start);

      if (token.type == TOKEN_END)
        break;
      if (token.type == TOKEN_ERROR)
        exit (1);
    }
}

result_t
interpret (char *source)
{
  compile (source);
  return RESULT_OK;
};

static void
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
    }
}

static char *
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

static void
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
  vm_t vm;
  vm_new (&vm);

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

  vm_free (&vm);
  return 0;
}
