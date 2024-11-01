const arity = {
	on: 2,
	if: 3,
	while: 2,
	set: 2,
	inc: 1,
	dec: 1,
	'=': 2,
	'<': 2,
	'<=': 2,
	'+': 2,
	'-': 2,
	'*': 2,
	'/': 2,
	'%': 2,
};

function lex(s) {
	const re = /[^()\s]+|\S/gs;
	return ('be ' + s + ' .').match(re);
}

function scan(tk, end) {
	let e = [];
	while (tk[0] != end && tk.length)
		e.push(parse(tk));
	tk.shift();
	return e;
}

const pairs = (p, c, i, ls) => i % 2 ? [...p, [ls[i - 1], c]] : p;

function parse(tk) {
	let t = tk.shift();
	if (t == '(')
		return scan(tk, ')');
	if (t == 'be')
		return [t].concat(scan(tk, '.'));
	if (t == 'to')
		return [t].concat([scan(tk, 'do').reduce(pairs, [])], [parse(tk)]);
	if (t in arity)
		return [t, ...Array(arity[t]).fill().map(() => parse(tk))];
	return isNaN(t) ? t : +t;
}

const lib = {
	be: (...x) => x.at(-1),
	'=': (x, y) => x == y,
	'<': (x, y) => x < y,
	'<=': (x, y) => x <= y,
	'+': (x, y) => x + y,
	'-': (x, y) => x - y,
	'*': (x, y) => x * y,
	'/': (x, y) => x / y,
	'%': (x, y) => x % y,
};

const pre = {
	to: ([kv, exp], e) => (kv.forEach(([k, v]) => e[k] = exec(v, e)), exec(exp, e)),
	on: ([[f, ...arg], exp], e) => e[f] = { arg: arg, exp: exp, env: structuredClone(e) },
	if: ([cond, yes, no], e) => exec(cond, e) ? yes : no,
	while: ([cond, exp], e) => { let x; while (exec(cond, e)) x = exec(exp, e); return x; },
	set: ([k, exp], e) => e[k] = exec(exp, e),
	inc: ([k], e) => e[k]++,
	dec: ([k], e) => e[k]--,
};

function exec(o, e = {}) {
	while (Array.isArray(o) && o.length) {
		const [f, ...arg] = o;
		if (f in lib)
			return lib[f](...arg.map(a => exec(a, e)));
		if (f in pre)
			o = pre[f](arg, e);
		else {
			const userf = e[f];
			e = Object.assign(userf.env, e);
			arg.forEach((a, i) => e[userf.arg[i]] = exec(a, e));
			o = userf.exp;
		}
	}
	return typeof o == 'string' ? e[o] : o;
}

function run(code) {
	console.dir(exec(parse(lex(code))), { depth: null });
}

run(`

on (f n)
  if = n 0
    1
    * n (f - n 1)

on (f n)
  to
    r 1
    i 1
  do
    while <= i n
      set r * r inc i

(f 5)

`);
