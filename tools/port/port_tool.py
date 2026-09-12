#!/usr/bin/env python3
"""
Actor-port extractor. Mechanically lifts a fork actor's ALBW code into a mod TU
so nothing is hand-transcribed.

Given a fork .cpp + the matching stock .cpp, it:
  1. Parses BOTH into top-level declarations (functions, classes, enums,
     #defines, file-static vars, typedefs).
  2. Classifies each fork function: NEW (absent in stock) or MODIFIED (body
     differs after whitespace-normalisation) or UNCHANGED.
  3. For the functions you ask to port, transitively collects every fork-LOCAL
     symbol they reference (l_HIO, anm_init, kAlbw*, ANM_*, s_gm*, the HIO class
     ...) and emits those declarations first, VERBATIM.
  4. Applies a rename map + sets listed DIAG #defines to 0 (so scaffolding
     compiles out without deleting a line) + applies literal substitutions
     (e.g. the reveal-asset shim).
  5. Verifies brace balance and reports every referenced symbol it could NOT
     resolve from the fork file (⇒ engine header, or a manual dep).

Output is a .inc of verbatim, dependency-ordered code + a report. Hook wiring
and seam placement stay human — but not one character is retyped.
"""
"""
KNOWN LIMITATION: a file-local class and its out-of-line constructor/methods
share a bare name (Foo / Foo::Foo) and collide on the decl dict key — the
method wins, so the class DECLARATION is not auto-pulled. Provide such a class
decl manually (it is small), or list it as an engine_symbol. Everything else
(statics, constants, enums, free functions, the method bodies) extracts cleanly.
"""
import io, re, sys, json

def rd(p): return io.open(p, encoding='utf-8', errors='replace').read()

IDENT = re.compile(r'\b([A-Za-z_]\w*)\b')
KEYWORDS = set('if for while switch return sizeof do else case break continue '
               'static const void int char bool float double unsigned signed '
               'struct class enum true false NULL nullptr this new delete u8 u16 '
               'u32 s8 s16 s32 f32 f64 BOOL TRUE FALSE inline typedef union'.split())

def strip_comments(t):
    t = re.sub(r"//[^\n]*", "", t)
    return re.sub(r"/\*.*?\*/", "", t, flags=re.S)

def brace_end(src, open_idx):
    depth = 0
    k = open_idx
    while k < len(src):
        c = src[k]
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return k
        k += 1
    return -1

def parse_decls(src):
    """Return dict name -> {'kind','text','refs'} for top-level decls."""
    decls = {}
    # functions / classes / structs / enums with a body { ... }
    for m in re.finditer(r'^([A-Za-z_][\w:<>*&\s]*?\b)(\w+)\s*(\([^;{}]*\))?\s*'
                         r'(?:const\s*)?(?::[^;{]*)?\{', src, re.M):
        name = m.group(2)
        if name in ('if','for','while','switch','do','else','return'): continue
        s = m.start(); ob = src.index('{', m.start()); e = brace_end(src, ob)
        if e < 0: continue
        # class/struct/enum end with '};'
        tail = src[e:e+2]
        text = src[s:e+ (2 if tail=='};' else 1)]
        kind = 'type' if re.match(r'^\s*(class|struct|enum|union)\b', text) else 'func'
        decls[name] = {'kind':kind,'text':text}
    # simple file-static vars / constants:  static <type> NAME ... ;   (single line-ish)
    for m in re.finditer(r'^(?:static\s+|constexpr\s+|const\s+)+[\w:<>*&\s]*?\b(\w+)\s*(?:=\s*[^;]*)?;',
                         src, re.M):
        name = m.group(1)
        if name in decls: continue
        decls.setdefault(name, {'kind':'var','text':m.group(0)})
    # enum members:  enum [name] { A, B = 3, C, ... };
    for m in re.finditer(r'enum\s*(?:\w+\s*)?(?::[^{]*)?\{([^}]*)\}', src):
        for mem in re.finditer(r'([A-Za-z_]\w*)\s*(?:=|,|\})', m.group(1)+'}'):
            decls.setdefault(mem.group(1), {'kind':'enummember','text':''})
    # #define NAME ...
    for m in re.finditer(r'^#define\s+(\w+)\b[^\n]*', src, re.M):
        decls.setdefault(m.group(1), {'kind':'define','text':m.group(0)})
    for v in decls.values():
        v['refs'] = set(x for x in IDENT.findall(strip_comments(v['text'])) if x not in KEYWORDS)
    return decls

def norm(t):
    t = re.sub(r'//[^\n]*','',t); t = re.sub(r'/\*.*?\*/','',t,flags=re.S)
    return re.sub(r'\s+','',t)

def main(cfg_path):
    cfg = json.load(io.open(cfg_path))
    fork = rd(cfg['fork']); stock = rd(cfg['stock'])
    F = parse_decls(fork); S = parse_decls(stock)

    # classify functions
    new_fns  = [n for n,d in F.items() if d['kind']=='func' and n not in S]
    mod_fns  = [n for n,d in F.items() if d['kind']=='func' and n in S
                and S[n]['kind']=='func' and norm(S[n]['text'])!=norm(F[n]['text'])]

    want = cfg.get('port_funcs') or (new_fns + mod_fns)
    want = [w for w in want if w in F]

    # transitive dep closure over fork-LOCAL decls
    need = set(); frontier = list(want)
    while frontier:
        n = frontier.pop()
        if n not in F or n in need: continue
        need.add(n)
        for r in F[n]['refs']:
            if r in F and r not in need and r not in want:
                frontier.append(r)
    dep_only = [n for n in need if n not in want]

    # order: defines, types, vars, then funcs (funcs in file order)
    order = {'define':0,'enummember':0,'type':1,'var':2,'func':3}
    def emit_list(names):
        return sorted(names, key=lambda n: (order[F[n]['kind']], fork.index(F[n]['text'])))

    def transform(text):
        for k,v in cfg.get('renames',{}).items():
            text = text.replace(k,v)
        for lit_from,lit_to in cfg.get('substitutions',[]):
            text = text.replace(lit_from,lit_to)
        return text

    out = []
    out.append("// === AUTO-EXTRACTED by port_tool.py — do not hand-edit the bodies ===")
    out.append("// diag defines forced 0 so scaffolding compiles out:")
    for d in cfg.get('diag_zero',[]):
        out.append(f"#define {d} 0")
    out.append("")
    out.append("// ---- pulled fork-local dependencies (verbatim) ----")
    for n in emit_list(dep_only):
        if F[n]['kind'] in ('define','enummember'): continue
        out.append(transform(F[n]['text'])); out.append("")
    import difflib
    out.append("// ---- NEW helpers (verbatim) ----")
    for n in emit_list([w for w in want if w in new_fns]):
        out.append(f"// [NEW helper] {n}")
        out.append(transform(F[n]['text'])); out.append("")
    out.append("// ---- MODIFIED stock fns: ADDED hunks only (place at hook seams) ----")
    for n in [w for w in want if w in mod_fns]:
        sa=strip_comments(S[n]['text']).split(chr(10)); fa=transform(F[n]['text']).split(chr(10))
        # diff fork(full, transformed) vs stock(stripped) -> keep added(+) with a little context
        diff=list(difflib.unified_diff([l for l in strip_comments(S[n]['text']).split(chr(10))],
                                       [l for l in transform(F[n]['text']).split(chr(10))],
                                       lineterm='', n=2))
        out.append(f"// ===== MODIFIED {n} : fork ADDED lines (seam-place these) =====")
        out.extend(l for l in diff if l[:1] in '+@' or l.strip()=='')
        out.append("")
    text = '\n'.join(out)

    # verify: brace balance
    bal = text.count('{')-text.count('}')
    # unresolved refs: symbols referenced by ported funcs not in F and not engine-allowlisted
    allow = set(cfg.get('engine_symbols',[]))
    refd = set()
    for n in want+dep_only:
        refd |= F[n]['refs']
    unresolved = sorted(r for r in refd
                        if r not in F and r not in KEYWORDS and r not in allow
                        and not r.startswith(('dComIf','cLib','cM_','mDoExt','mDoMtx',
                                              'fopAc','fpcM','Z2','J3D','JKR','dSv','g_',
                                              'MTX','JMA','dKy','dDlst')))

    io.open(cfg['out'],'w',encoding='utf-8',newline='').write(text)
    print(f"NEW helpers: {len(new_fns)} | MODIFIED: {len(mod_fns)}")
    print(f"ported funcs: {len(want)} | pulled deps: {len(dep_only)}")
    print(f"brace balance: {bal} (0 = ok)")
    print(f"deps pulled: {', '.join(emit_list(dep_only)[:40])}")
    print(f"\nMODIFIED (need hook/seam): {', '.join(n for n in want if n in mod_fns)}")
    print(f"\nUNRESOLVED symbols (engine header or manual): {', '.join(unresolved[:60])}")
    print(f"\nwrote {cfg['out']} ({text.count(chr(10))} lines)")

if __name__=='__main__':
    main(sys.argv[1])
