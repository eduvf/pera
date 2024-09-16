const arity = {
    print: 1,
    '#': 1,
    not: 1, and: 2, or: 2,
    '=': 2, '<': 2,
    '+': 2, '-': 2, '*': 2, '/': 2, '%': 2,
    inc: 1, dec: 1,
    on: 2, to: 2, if: 3, while: 2,
    put: 3,
};

const lib = {
    print: ([v], e) => (v = ev(v, e), console.log(print(v)), v),
    not: ([x], e) => !ev(x, e),
    and: ([x, y], e) => ev(x, e) && ev(y, e),
    or: ([x, y], e) => ev(x, e) || ev(y, e),
    '=': ([x, y], e) => ev(x, e) === ev(y, e),
    '<': ([x, y], e) => ev(x, e) < ev(y, e),
    '+': ([x, y], e) => ev(x, e) + ev(y, e),
    '-': ([x, y], e) => ev(x, e) - ev(y, e),
    '*': ([x, y], e) => ev(x, e) * ev(y, e),
    '/': ([x, y], e) => ev(x, e) / ev(y, e),
    '%': ([x, y], e) => ev(x, e) % ev(y, e),
    do: (xs, e) => (xs.slice(0, -1).map(x => ev(x, e)), xs[xs.length - 1]),
    inc: ([k], e) => e[k]++,
    dec: ([k], e) => e[k]--,
    to: ([k, v], e) => e[k] = ev(v, e),
    on: ([f, b], e) => e[f[0]] = { is_fn: 1, arg: f.slice(1), body: b, env: structuredClone(e) },
    if: ([c, t, f], e) => ev(c, e) ? t : f,
    while: ([c, t], e) => { let r; while (ev(c, e)) r = ev(t, e); return r; },
    table: (kv, e) => Object.fromEntries(kv.map(p => [p[0], ev(p[1], e)])),
    list: (xs, e) => xs.map(x => ev(x, e)),
    '.': ([t, ...ks], e) => [ev(t, e), ...ks].reduce((p, c) => p[c]),
    put: ([t, k, v], e) => ev(t, e)[k] = ev(v, e),
    '#': ([t], e) => Object.keys(ev(t, e)).length,
};

function lex(s) {
    const re = /[^()\s]+|\S/g;
    return ('(do ' + s + ')').match(re);
}

function scan(tk) {
    let e = [];
    while (tk.length && tk[0] != ')')
        e.push(parse(tk));
    tk.shift();
    return e.length == 1 && e[0][0] in arity ? e[0] : e;
}

function parse(tk) {
    let t = tk.shift();
    if (t == 'nil') return;
    if (t == ')') return;
    if (t == '(') return scan(tk);
    if (t in arity)
        return [t, ...Array(arity[t]).fill().map(() => parse(tk))];
    return isNaN(t) ? t : +t;
}

function ev(o, e = {}) {
    while (Array.isArray(o) && o.length) {
        let [f, ...arg] = o;
        if (f in lib)
            o = lib[f](arg, e);
        else if ((f = ev(f, e))?.is_fn) {
            e = Object.assign(f.env, e);
            o = f.body;
            arg.map(x => ev(x, e)).forEach((x, i) => e[f.arg[i]] = x);
        } else break;
    }
    return typeof o == 'string' ? e[o] : o;
}

function print(o) {
    if (typeof o === 'object')
        return `( table ${Object.keys(o).map(k => `( ${k} ${print(o[k])} )`).join(' ')} )`;
    if (typeof o === 'undefined')
        return 'nil';
    return `${o}`;
}

let test = `
on (sum n acc)
  if = n 0
    acc
    (sum (- n 1) (+ n acc))

print (sum 1000000 0)
print acc

on (make_gen i)
  on (_) print inc i

to gen (make_gen 1)
while < (gen) 3 nil

to i 3
while < 0 i
  print dec i

on (fact n)
  if < n 1
    1
    * n (fact - n 1)
(fact 5)

to t (table (one 1) (two 3))
print (. t one)
put t two 2
print t
print # t

to l (list 1 2 3)
# l
`;

console.log('>', print(ev(parse(lex(test)))));
