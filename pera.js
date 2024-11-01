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

function run(code) {
	console.dir(parse(lex(code)), { depth: null });
}

run(`

on (f n)
  if = n 0
    1
    * n (f - n 1)

(f 5)

`);
