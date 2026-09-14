import io, os, sys
bad = []
files = ['main.c'] + ['modules/' + n for n in sorted(os.listdir('modules')) if n.endswith('.h')] \
        + ['route_http/' + n for n in sorted(os.listdir('route_http')) if n.endswith('.h')]
for p in files:
    s = io.open(p, encoding='utf-8').read()
    out, i, n = [], 0, len(s)
    while i < n:
        c = s[i]
        if c == '"':
            i += 1
            while i < n and s[i] != '"':
                i += 2 if s[i] == chr(92) else 1
        elif c == "'":
            i += 1
            while i < n and s[i] != "'":
                i += 2 if s[i] == chr(92) else 1
        elif c == '/' and i + 1 < n and s[i+1] == '/':
            while i < n and s[i] != '\n': i += 1
        elif c == '/' and i + 1 < n and s[i+1] == '*':
            j = s.find('*/', i)
            i = n if j < 0 else j + 2
            continue
        else:
            out.append(c)
        i += 1
    t = ''.join(out)
    for op, cl in (('(', ')'), ('{', '}')):
        d = t.count(op) - t.count(cl)
        if d: bad.append('%s %s%s delta %d' % (p, op, cl, d))
print('\n'.join(bad) if bad else 'balance OK (%d files)' % len(files))
