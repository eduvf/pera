function lex(s) {
    const re = /[^()\s]+|\S/g;
    return ('do (' + s + ')').match(re);
}

let test = `
on sum (n acc)
  if = n 0
    acc
    (sum (- n 1) (+ n acc))

print (sum 1000000 0)

set i 3
while < 0 i
  print dec i
`;

console.log(lex(test));
