const arity = {
    do: 1, print: 1, not: 1, inc: 1, dec: 1,
    set: 2, while: 2, '=': 2, '<': 2,
    on: 3, if: 3,
};

const lib = {
    "'": (xs, _) => xs,
    print: ([v], e) => (v = ev(v, e), console.log(print(v)), v),
    not: ([x], e) => !ev(x, e),
    '=': ([x, y], e) => ev(x, e) === ev(y, e),
    '<': ([x, y], e) => ev(x, e) < ev(y, e),
    '+': (xs, e) => xs.map(x => ev(x, e)).reduce((p, c) => p + c),
    '-': (xs, e) => xs.map(x => ev(x, e)).reduce((p, c) => p - c),
    do: ([l], e) => (l.slice(0, -1).map(x => ev(x, e)), l[l.length - 1]),
    inc: ([k], e) => e[k]++,
    dec: ([k], e) => e[k]--,
    set: ([k, v], e) => e[k] = ev(v, e),
    on: ([k, p, b], e) => e[k] = new proc(p, b, Object.assign({}, e)),
    if: ([c, t, f], e) => ev(c, e) ? t : f,
    while: ([c, t], e) => { let r; while (ev(c, e)) r = ev(t, e); return r; },
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
    if (t == 'nil') return;
    if (t == ')') return;
    if (t == '(') return scan(tk);
    if (t in arity)
        return [t, ...Array(arity[t]).fill().map(() => parse(tk))];
    return isNaN(t) ? t : +t;
}

class proc { constructor(arg, body, env) { this.arg = arg, this.body = body, this.env = env; } }

function ev(o, e = {}) {
    while (Array.isArray(o) && o.length) {
        let [f, ...arg] = o;
        if (f in lib)
            o = lib[f](arg, e);
        else if ((f = ev(f, e)) instanceof proc) {
            e = Object.assign(f.env, e);
            o = f.body;
            arg.map(x => ev(x, e)).forEach((x, i) => e[f.arg[i]] = x);
        } else break;
    }
    return typeof o == 'string' ? e[o] : o;
}

function print(o) {
    if (Array.isArray(o))
        return `( ${o.map(print).join(' ')} )`;
    if (o instanceof proc)
        return 'function';
    if (typeof o === 'undefined')
        return 'nil';
    return `${o}`;
}

let test = `
on sum (n acc)
  if = n 0
    acc
    (sum (- n 1) (+ n acc))

print (sum 1000000 0)
print acc

on make_gen (i)
  on _ () print inc i

set gen (make_gen 1)
while < (gen) 3 nil

set i 3
while < 0 i
  print dec i

print nil
(' hello world !)
`;

console.log('>', print(ev(parse(lex(test)))));
