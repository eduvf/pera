#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG
#define STACK_SIZE 256

typedef enum
{
  TYPE_NIL,
  TYPE_BOOL,
  TYPE_NUMBER,
} value_type_t;

typedef struct
{
  value_type_t type;
  union
  {
    bool boolean;
    double number;
  } as;
} value_t;

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
} vm_t;

/* GLOBALS */

scan_t scan;
vm_t vm;

/* ARRAY FUNCTIONS */

void
array_new (array_t *array)
{
  array->length = 0;
  array->capacity = 8 * sizeof (value_t);
  array->values = malloc (8 * sizeof (value_t));
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
  block->capacity = 8 /* bytes */;
  block->code = malloc (8 /* bytes */);
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

int
block_add_constant (block_t *block, value_t value)
{
  array_push (&block->constants, value);
  return block->constants.length - 1;
}

void
block_push_constant (block_t *block, value_t value)
{
  int constant = block_add_constant (block, value);
  if (constant > UINT8_MAX)
    {
      fprintf (stderr, "Too many constants in block.\n");
      exit (1);
    }

  block_push (block, OP_CONSTANT);
  block_push (block, constant);
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
      printf ("CONSTANT %02x %g\n", constant, value.as.number);
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

value_t
value_from_number (double n)
{
  value_t v = { TYPE_NUMBER, { .number = n } };
  return v;
}

#define BINARY_OP(o)                                                          \
  do                                                                          \
    {                                                                         \
      double b = pop (vm).as.number;                                          \
      double a = pop (vm).as.number;                                          \
      push (vm, value_from_number (a o b));                                   \
    }                                                                         \
  while (0)

static result_t
run (vm_t *vm)
{
  uint8_t op;
  value_t v;

  while (1)
    {
      puts ("hi");
#ifdef DEBUG
      for (value_t *v = vm->stack; v < vm->top; v++)
        printf ("[%g]", v->as.number);
      printf ("\n");
      disassemble_operation (&vm->block, (int)(vm->pc - vm->block.code));
#endif
      switch (op = *vm->pc++)
        {
        case OP_CONSTANT:
          v = vm->block.constants.values[*vm->pc++];
          printf ("%g\n", v.as.number);
          push (vm, v);
          break;
        case OP_NEG:
          push (vm, value_from_number (-pop (vm).as.number));
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
            double b = pop (vm).as.number;
            double a = pop (vm).as.number;
            push (vm, value_from_number (fmod (a, b)));
            break;
          }
        case OP_RETURN:
          printf ("%g\n", pop (vm).as.number);
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
  // #ifdef DEBUG
  //   printf ("%d '%.*s'\n", token.type, token.length, token.start);
  // #endif
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

  while (is_word (*scan.current))
    scan.current++;

  if (is_number (scan.start, scan.current))
    return token_create (TOKEN_NUMBER);

  return token_create (TOKEN_WORD);
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
        }
    }
  return OP_ERROR;
}

bool
emit_word (token_t token, block_t *block)
{
  opcode_t op = is_token_op (token);
  if (op == OP_ERROR)
    {
      fprintf (stderr, "Unrecognized word '%.*s'\n", token.length,
               token.start);
      return false;
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

  block_push_constant (block, value_from_number (n));
}

bool
expression (token_t token, block_t *block)
{
  if (token.type == TOKEN_RPAREN)
    {
      fprintf (stderr, "Unexpected ')'\n");
      return false;
    }
  if (token.type == TOKEN_LPAREN)
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
          if (!expression (token, block))
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
  if (token.type == TOKEN_WORD)
    {
      if (!emit_word (token, block))
        return false;
      return true;
    }
  if (token.type == TOKEN_NUMBER)
    {
      emit_number (token, block);
      return true;
    }
  if (token.type == TOKEN_END)
    return true;

  fprintf (stderr, "Unrecognized token\n");
  return false;
}

bool
compile (const char *source, block_t *block)
{
  scan_new (source);
  while (1)
    {
      token_t token = scan_token ();

      if (!expression (token, block))
        return false;

      block_push (block, OP_RETURN);

      if (token.type == TOKEN_END)
        break;
    }
  return true;
}

result_t
interpret (char *source)
{
  result_t result;

  if (!compile (source, &vm.block))
    return RESULT_COMPILE_ERROR;

#ifdef DEBUG
  disassemble (&vm.block);
#endif

  vm.pc = vm.block.code;
  result = run (&vm);

  return result;
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
