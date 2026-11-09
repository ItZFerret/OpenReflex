#!/usr/bin/env python3
"""PROTOTYPE (_re/notes/client-platform.md, "Temp-copy loader"): rewrites every [DllImport("reflexworld")] extern of the Unity scripts into a delegate bound through
ReflexNativeLoader (the temp-copy loader), keeping its marshalling. All-or-nothing: a build must never mix the two (a DllImport'ed
copy and the loader's copy are two images with two sets of statics; a world of one passed to the other corrupts it).

    convert_dllimports.py <Assets/Scripts folder> [--check]

--check lists what would change and exits 1 if anything is left unconverted. Each extern

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl[, EntryPoint = "x"][, CharSet = ...])]
    [return: MarshalAs(...)] <modifiers> static extern R name(<params>);

becomes

    [UnmanagedFunctionPointer(CallingConvention.Cdecl[, CharSet = ...])] [return: ...] delegate R name__fn(<params>);
    static readonly ReflexNativeLoader.Fn<name__fn> name__p = new ReflexNativeLoader.Fn<name__fn>("x");
    <modifiers> static R name(<params>) => name__p.Get()(<ref/out + names>);
"""
import re
import sys
from pathlib import Path

LIBS = {'"reflexworld"', 'Library', 'Lib', 'ReflexWorldNative.Library'}
LOADER = 'Reflex.Port.Platform.ReflexNativeLoader'

EXTERN = re.compile(
    r'(?P<indent>[ \t]*)\[(?:System\.Runtime\.InteropServices\.)?DllImport\((?P<args>[^\]]*)\)\]\s*'
    r'(?P<ret_attr>\[return:[^\]]*\]\s*)?'
    r'(?P<mods>(?:(?:public|internal|private|protected|static|extern|unsafe)\s+)+)'
    r'(?P<ret>[\w.<>\[\]]+)\s+(?P<name>\w+)\s*\((?P<params>.*?)\)\s*;',
    re.S)


def split_top(s):
    out, depth, cur = [], 0, ''
    for ch in s:
        if ch in '([<':
            depth += 1
        elif ch in ')]>':
            depth -= 1
        if ch == ',' and depth == 0:
            out.append(cur)
            cur = ''
        else:
            cur += ch
    if cur.strip():
        out.append(cur)
    return [p.strip() for p in out]


def arg_of(param):
    p = re.sub(r'\[[^\]]*\]', '', param).strip()       # attributes: [MarshalAs(...)], [Out], [In]
    p = p.split('=')[0].strip()                         # defaults
    words = p.split()
    mod = words[0] + ' ' if words[0] in ('ref', 'out', 'in') else ''
    return mod + words[-1]


def convert(text):
    changed = 0

    def repl(m):
        nonlocal changed
        args = split_top(m['args'])
        lib = args[0]
        if lib not in LIBS:
            return m[0]
        named = dict(a.split('=', 1) for a in args[1:] if '=' in a)
        named = {k.strip(): v.strip() for k, v in named.items()}
        entry = named.get('EntryPoint', '"%s"' % m['name'])
        ns = 'System.Runtime.InteropServices.'
        ufp = [ns + 'CallingConvention.' + named.get('CallingConvention', 'CallingConvention.Cdecl').split('.')[-1]]
        if 'CharSet' in named:
            ufp.append('CharSet = ' + ns + 'CharSet.' + named['CharSet'].split('.')[-1])
        mods = ' '.join(w for w in m['mods'].split() if w != 'extern')
        params = ' '.join(m['params'].split())
        call = ', '.join(arg_of(p) for p in split_top(params)) if params.strip() else ''
        n, ind = m['name'], m['indent']
        ret_attr = (m['ret_attr'] or '').strip()
        changed += 1
        return (f'{ind}[System.Runtime.InteropServices.UnmanagedFunctionPointer({", ".join(ufp)})] {ret_attr + " " if ret_attr else ""}'
                f'delegate {m["ret"]} {n}__fn({params});\n'
                f'{ind}static readonly {LOADER}.Fn<{n}__fn> {n}__p = new {LOADER}.Fn<{n}__fn>({entry});\n'
                f'{ind}{ret_attr + " " if ret_attr else ""}{mods} {m["ret"]} {n}({params}) => {n}__p.Get()({call});')

    out = EXTERN.sub(repl, text)
    # [return: X] is not valid on a method declared with =>? It is (it applies to the method); kept on both.
    return out, changed


def main():
    root = Path(sys.argv[1])
    check = '--check' in sys.argv
    total, left = 0, []
    for f in sorted(root.rglob('*.cs')):
        text = f.read_text(encoding='utf-8')
        if 'DllImport' not in text:
            continue
        out, n = convert(text)
        rest = [m['name'] for m in EXTERN.finditer(out) if split_top(m['args'])[0] in LIBS]
        left += [f'{f}: {r}' for r in rest]
        if n:
            total += n
            print(f'{f.relative_to(root)}: {n} externs')
            if not check:
                f.write_text(out, encoding='utf-8')
    print(f'{total} externs {"to convert" if check else "converted"}; {len(left)} left')
    for l in left:
        print('  LEFT', l)
    sys.exit(1 if check and (total or left) else 0)


if __name__ == '__main__':
    main()
