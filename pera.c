#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
  OP_RETURN,
} opcode_t;

typedef struct
{
  int length;
  int capacity;
  uint8_t *code;
} block_t;

/* BLOCK FUNCTIONS */

void
block_new (block_t *block)
{
  block->length = 0;
  block->capacity = 8;
  block->code = malloc (8);
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

void
block_free (block_t *block)
{
  free (block->code);
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
