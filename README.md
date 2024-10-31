# 🍐 pera

## examples

```
on (f n)
  if = n 0
    1
    * n (f - n 1)

on (f n)
  to
    r 1
    i 2
  do
    while < i n
      set r * r inc i

on (fib n)
  to
    a 0
    b 1
  do
    times n
      be
        set a b
        set b + ^ b
        b
      .
```
