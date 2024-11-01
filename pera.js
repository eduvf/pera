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

const env = {};

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
	to: ([kv, exp]) => (kv.forEach(([k, v]) => env[k] = exec(v)), exec(exp)),
	on: ([[f, ...arg], exp]) => env[f] = { arg: arg, exp: exp },
	if: ([cond, yes, no]) => exec(cond) ? yes : no,
	while: ([cond, exp]) => { let x; while (exec(cond)) x = exec(exp); return x; },
	set: ([k, exp]) => env[k] = exec(exp),
	inc: ([k]) => env[k]++,
	dec: ([k]) => env[k]--,
};

function exec(o) {
	while (Array.isArray(o) && o.length) {
		const [f, ...arg] = o;
		if (f in lib)
			return lib[f](...arg.map(exec));
		if (f in pre)
			o = pre[f](arg);
		else {
			const userf = env[f];
			arg.forEach((a, i) => env[userf.arg[i]] = exec(a));
			o = userf.exp;
		}
	}
	return typeof o == 'string' ? env[o] : o;
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
