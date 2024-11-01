const arity = {
	on: 2,
	if: 3,
	'=': 2,
	'<': 2,
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

function parse(tk) {
	let t = tk.shift();
	if (t == '(')
		return scan(tk, ')');
	if (t == 'be')
		return [t].concat(scan(tk, '.'));
	if (t in arity)
		return [t, ...Array(arity[t]).fill().map(() => parse(tk))];
	return isNaN(t) ? t : +t;
}

const env = {};

const lib = {
	be: (...x) => x.at(-1),
	'=': (x, y) => x == y,
	'<': (x, y) => x < y,
	'+': (x, y) => x + y,
	'-': (x, y) => x - y,
	'*': (x, y) => x * y,
	'/': (x, y) => x / y,
	'%': (x, y) => x % y,
};

const pre = {
	on: ([[f, ...arg], exp]) => env[f] = { arg: arg, exp: exp },
	if: ([cond, yes, no]) => exec(cond) ? yes : no,
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

(f 5)

`);
