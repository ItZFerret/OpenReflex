# cs_imports.py <Assets/Scripts>: the plugin entry points the C# code uses, one per line, sorted. Used by check_dll_windows.sh.
# Both forms: [DllImport("reflexworld" / Lib / Library / ReflexWorldNative.Library)] externs (EntryPoint honoured), and the
# temp-copy loader's delegates, ReflexNativeLoader.Fn<T>("name") / TrySymbol("name"). Comments are skipped.
import re, sys, pathlib
names = set()
dll = re.compile(r'\[DllImport\(\s*(?:"(?:lib)?reflexworld"|Lib\b|Library\b|ReflexWorldNative\.Library\b)([^\]]*)\]\s*(?:\[[^\]]*\]\s*)*'
                 r'(?:(?:public|internal|private|protected|static|unsafe)\s+)*extern\s+[\w<>\[\],. ]+?\s+(\w+)\s*\(')
fn = re.compile(r'\bFn<[^>]+>\(\s*"(\w+)"\s*\)|\bTrySymbol\(\s*"(\w+)"\s*\)')
for p in pathlib.Path(sys.argv[1]).rglob('*.cs'):
    s = p.read_text(errors='replace')
    s = re.sub(r'/\*.*?\*/', ' ', s, flags=re.S)
    s = re.sub(r'//[^\n]*', ' ', s)
    for m in dll.finditer(s):
        ep = re.search(r'EntryPoint\s*=\s*"(\w+)"', m.group(1))
        names.add(ep.group(1) if ep else m.group(2))
    for m in fn.finditer(s):
        names.add(m.group(1) or m.group(2))
print("\n".join(sorted(names)))
