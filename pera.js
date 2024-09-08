const arity = {
    do: 1, print: 1, not: 1, inc: 1, dec: 1,
    set: 2, while: 2, '=': 2, '<': 2,
    on: 3, if: 3,
};

const env = {};

const lib = {
    "'": xs => xs,
    print: ([v]) => (v = ev(v), console.log(print(v)), v),
    not: ([x]) => !ev(x),
    '=': ([x, y]) => ev(x) === ev(y),
    '<': ([x, y]) => ev(x) < ev(y),
    '+': xs => xs.map(ev).reduce((p, c) => p + c),
    '-': xs => xs.map(ev).reduce((p, c) => p - c),
    do: ([e]) => (e.slice(0, -1).map(ev), e[e.length - 1]),
    inc: ([k]) => env[k]++,
    dec: ([k]) => env[k]--,
    set: ([k, v]) => env[k] = ev(v),
    on: ([k, p, b]) => env[k] = new proc(p, b),
    if: ([c, t, e]) => ev(c) ? t : e,
    while: ([c, t]) => { let r; while (ev(c)) r = ev(t); return r; },
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

class proc { constructor(arg, body) { this.arg = arg, this.body = body; } }

function ev(o) {
    while (Array.isArray(o) && o.length) {
        let [f, ...arg] = o;
        if (f in lib)
            o = lib[f](arg);
        else if ((f = ev(f)) instanceof proc) {
            o = f.body;
            arg.map(ev).forEach((x, i) => env[f.arg[i]] = x);
        } else break;
    }
    return typeof o == 'string' ? env[o] : o;
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

set i 3
while < 0 i
  print dec i

print nil
(' hello world !)
`;

console.log('>', print(ev(parse(lex(test)))));
