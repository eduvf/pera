#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef double value_t;

typedef enum
{
  OP_RETURN,
} opcode_t;

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

int
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

/* DEBUG */

int
disassemble_operation (block_t *block, size_t offset)
{
  opcode_t op = block->code[offset];

  switch (op)
    {
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

int
main (void)
{
  block_t block;
  block_new (&block);

  block_push (&block, OP_RETURN);
  disassemble (&block);

  block_free (&block);
  return 0;
}
