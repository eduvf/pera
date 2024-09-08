const arity = {
    do: 1, print: 1, not: 1, inc: 1, dec: 1,
    set: 2, while: 2, '=': 2, '<': 2,
    on: 3, if: 3,
};

function lex(s) {
    const re = /[^()\s]+|\S/g;
    return ('do (' + s + ')').match(re);
}

function scan(tk) {
    let e = [];
    while (tk.length && tk[0] != ')')
        e.push(parse(tk));
    tk.shift();
    return e;
}

function parse(tk) {
    let t = tk.shift();
    if (t == ')') return;
    if (t == '(') return scan(tk);
    if (t in arity)
        return [t, ...Array(arity[t]).fill().map(() => parse(tk))];
    return isNaN(t) ? t : +t;
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

console.log(parse(lex(test)));
