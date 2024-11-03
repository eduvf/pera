#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum
{
  OP_RET,
} opcode_t;

typedef struct
{
  int length;
  int capacity;
  uint8_t *data;
} array_t;

/* ARRAY FUNCTIONS */

void
array_new (array_t *array)
{
  array->length = 0;
  array->capacity = 8;
  array->data = malloc (8);
}

void
array_push (array_t *array, uint8_t byte)
{
  if (array->capacity < array->length + 1)
    {
      array->capacity *= 2;
      array->data = realloc (array->data, array->capacity);
      if (array->data == NULL)
        exit (1);
    }

  array->data[array->length] = byte;
  array->length++;
}

void
array_free (array_t *array)
{
  free (array->data);
}

/* DEBUG */

int
disassemble_operation (array_t *array, size_t offset)
{
  opcode_t op = array->data[offset];

  switch (op)
    {
    case OP_RET:
      printf ("RETURN\n");
      return 1;
    default:
      printf ("unknown op %02x", op);
      return 1;
    }
}

void
disassemble (array_t *array)
{
  for (size_t offset = 0; offset < array->length;)
    {
      printf ("%04zx ", offset);
      offset += disassemble_operation (array, offset);
    }
}

int
main (void)
{
  array_t a;
  array_new (&a);

  array_push (&a, OP_RET);
  disassemble (&a);

  array_free (&a);
  return 0;
}
