#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define DEBUG
#define STACK_SIZE 256

typedef double value_t;

typedef enum
{
  OP_CONSTANT,
  OP_RETURN,
} opcode_t;

typedef enum
{
  RESULT_OK,
  RESULT_COMPILE_ERROR,
  RESULT_RUNTIME_ERROR,
} result_t;

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
  int *lines;
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
  block->lines = malloc (8);
  block->code = malloc (8);
  array_new (&block->constants);
}

void
block_push (block_t *block, uint8_t byte, int line)
{
  if (block->capacity < block->length + 1)
    {
      block->capacity *= 2;
      block->lines = realloc (block->lines, block->capacity);
      block->code = realloc (block->code, block->capacity);
      if (block->lines == NULL || block->code == NULL)
        exit (1);
    }

  block->lines[block->length] = line;
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
  free (block->lines);
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

      if (offset == 0 || block->lines[offset - 1] != block->lines[offset])
        printf ("%4d ", block->lines[offset]);
      else
        printf ("     ");

      offset += disassemble_operation (block, offset);
    }
}

/* INTERPRET */

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
        case OP_RETURN:
          printf ("%g\n", pop (vm));
          return RESULT_OK;
        }
    }
}

result_t
interpret (vm_t *vm, block_t *block)
{
  vm->block = *block;
  vm->pc = vm->block.code;
  return run (vm);
}

/* MAIN */

int
main (void)
{
  vm_t vm;
  block_t *block;

  vm_new (&vm);
  block = &vm.block;

  block_push (block, OP_CONSTANT, 1);
  block_push (block, block_add_constant (block, 1), 1);
  block_push (block, OP_CONSTANT, 1);
  block_push (block, block_add_constant (block, 2.3), 1);
  block_push (block, OP_RETURN, 2);
  disassemble (block);

  interpret (&vm, block);

  vm_free (&vm);
  return 0;
}
