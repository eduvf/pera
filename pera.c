#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
  int length;
  int capacity;
  uint8_t *data;
} array_t;

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

int
main (void)
{
  return 0;
}
