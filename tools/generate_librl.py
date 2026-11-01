#!/usr/bin/env python3
"""Generate/check the 15 bundled MuLIB Readline language profiles."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Syntax rules are lexical aids for editing, not language parsers.
LANGUAGES = {
    'c': ('C', 'auto break case const continue default do else enum extern for goto if inline register restrict return sizeof static struct switch typedef union volatile while _Alignas _Alignof _Atomic _Bool _Generic _Noreturn _Static_assert _Thread_local',
          'char double float int long short signed unsigned void size_t uint32_t uint64_t FILE',
          'printf snprintf fprintf malloc calloc realloc free memcpy memmove memset strcmp strlen fopen fclose fread fwrite', '//', 'int main(void) { $* return 0; }'),
    'cpp': ('C++', 'alignas alignof asm auto break case catch class concept const constexpr consteval constinit continue co_await co_return co_yield decltype default delete do else enum explicit export extern for friend goto if inline mutable namespace new noexcept operator private protected public register requires return sizeof static static_assert struct switch template this thread_local throw try typedef typeid typename union using virtual volatile while',
            'bool char char8_t char16_t char32_t double float int long short signed unsigned void std::string std::vector std::span',
            'std::cout std::cin std::move std::forward std::make_unique std::make_shared std::sort std::find std::begin std::end', '//', 'int main() { $* return 0; }'),
    'python': ('Python', 'and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield match case True False None',
               'bool bytes bytearray complex dict float frozenset int list memoryview object range set str tuple type',
               'abs all any enumerate filter format getattr hasattr isinstance issubclass iter len map max min next open print repr reversed sorted sum super zip', '#', 'def main(): $*'),
    'javascript': ('JavaScript', 'async await break case catch class const continue debugger default delete do else export extends false finally for from function if import in instanceof let new null of return static super switch this throw true try typeof undefined var void while with yield',
                   'Array BigInt Boolean Date Error Function Map Number Object Promise RegExp Set String Symbol WeakMap WeakSet',
                   'console.log JSON.parse JSON.stringify Math.max Math.min Math.random Object.keys Object.entries Array.from document.querySelector fetch setTimeout clearTimeout', '//', 'async function main() { $* }'),
    'typescript': ('TypeScript', 'abstract any as asserts async await break case catch class const constructor continue declare default delete do else enum export extends false finally for from function if implements import in infer instanceof interface is keyof let namespace never new null of override private protected public readonly require return satisfies static super switch this throw true try type typeof undefined unique unknown var void while yield',
                   'Array BigInt Boolean Date Error Function Map Number Object Promise Record Partial Pick Omit Readonly Set String boolean number string',
                   'console.log JSON.parse JSON.stringify Object.keys Object.entries Array.from document.querySelector fetch setTimeout', '//', 'async function main(): Promise<void> { $* }'),
    'lua': ('Lua', 'and break do else elseif end false for function goto if in local nil not or repeat return then true until while',
            '', 'assert collectgarbage error ipairs pairs next pcall print require select setmetatable getmetatable tonumber tostring type xpcall math.floor math.random string.format string.find string.gsub table.insert table.concat table.sort utf8.len coroutine.create coroutine.resume', '--', 'local function main() $* end; main()'),
    'rust': ('Rust', 'as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while',
             'bool char str i8 i16 i32 i64 i128 isize u8 u16 u32 u64 u128 usize f32 f64 String Vec Option Result Box Arc Rc HashMap',
             'println! eprintln! format! vec! assert! assert_eq! Some None Ok Err std::fs::read_to_string std::io::stdin', '//', 'fn main() { $* }'),
    'go': ('Go', 'break case chan const continue default defer else fallthrough for func go goto if import interface map package range return select struct switch type var true false nil iota',
           'any bool byte comparable complex64 complex128 error float32 float64 int int8 int16 int32 int64 rune string uint uint8 uint16 uint32 uint64 uintptr',
           'append cap clear close complex copy delete imag len make max min new panic print println real recover fmt.Println fmt.Printf json.Marshal json.Unmarshal', '//', 'func main() { $* }'),
    'java': ('Java', 'abstract assert break case catch class const continue default do else enum exports extends final finally for if implements import instanceof interface module native new non-sealed open opens package permits private protected provides public record requires return sealed static strictfp super switch synchronized this throw throws to transient transitive try uses var volatile while with yield true false null',
             'boolean byte char double float int long short void String Object Integer List Map Set Optional Stream',
             'System.out.println System.err.println String.format Integer.parseInt Arrays.sort Collections.sort List.of Map.of Optional.of', '//', 'public static void main(String[] args) { $* }'),
    'csharp': ('C#', 'abstract as async await base break case catch checked class const continue default delegate do else enum event explicit extern false finally fixed for foreach goto if implicit in interface internal is lock namespace new null operator out override params partial private protected public readonly record ref required return sealed sizeof stackalloc static struct switch this throw true try typeof unchecked unsafe using var virtual volatile while yield',
               'bool byte char decimal double dynamic float int long object sbyte short string uint ulong ushort void Task List Dictionary IEnumerable',
               'Console.WriteLine Console.ReadLine string.Format int.Parse Math.Max Math.Min Enumerable.Range File.ReadAllText JsonSerializer.Serialize', '//', 'static void Main(string[] args) { $* }'),
    'ruby': ('Ruby', 'BEGIN END alias and begin break case class def defined? do else elsif end ensure false for if in module next nil not or redo rescue retry return self super then true undef unless until when while yield',
             'Array Class Exception Float Hash Integer Module Object Range Regexp String Symbol',
             'puts print p require require_relative raise attr_reader attr_writer attr_accessor lambda proc loop rand sleep File.read JSON.parse JSON.generate', '#', 'def main; $*; end; main'),
    'php': ('PHP', 'abstract and array as break callable case catch class clone const continue declare default die do echo else elseif empty enddeclare endfor endforeach endif endswitch endwhile enum eval exit extends final finally fn for foreach function global goto if implements include include_once instanceof insteadof interface isset list match namespace new null or print private protected public readonly require require_once return static switch throw trait try unset use var while xor yield true false',
            'bool float int iterable mixed never object string void',
            'count strlen substr strpos str_replace array_map array_filter array_reduce json_encode json_decode file_get_contents fopen fclose printf sprintf', '//', 'function main() { $* } main();'),
    'bash': ('Bash', 'if then else elif fi case esac for select while until do done in function time coproc [[ ]] !',
             '', 'alias bg bind break builtin cd command compgen complete continue declare dirs disown echo enable eval exec exit export false fc fg getopts hash help history jobs kill let local logout mapfile popd printf pushd pwd read readonly return set shift shopt source test times trap true type typeset ulimit umask unalias unset wait', '#', 'main() { $*; }; main "$@"'),
    'sql': ('SQL', 'SELECT FROM WHERE INSERT INTO VALUES UPDATE SET DELETE CREATE ALTER DROP TABLE VIEW INDEX DATABASE JOIN INNER LEFT RIGHT FULL OUTER CROSS ON GROUP BY HAVING ORDER ASC DESC LIMIT OFFSET DISTINCT AS UNION ALL AND OR NOT NULL IS IN EXISTS BETWEEN LIKE CASE WHEN THEN ELSE END BEGIN COMMIT ROLLBACK PRIMARY KEY FOREIGN REFERENCES UNIQUE CHECK DEFAULT WITH RECURSIVE RETURNING OVER PARTITION',
            'INTEGER BIGINT SMALLINT DECIMAL NUMERIC REAL DOUBLE FLOAT BOOLEAN CHAR VARCHAR TEXT DATE TIME TIMESTAMP BLOB',
            'COUNT SUM AVG MIN MAX COALESCE NULLIF CAST CONCAT LOWER UPPER TRIM SUBSTRING ROUND LENGTH CURRENT_DATE CURRENT_TIMESTAMP ROW_NUMBER RANK', '--', 'SELECT $* FROM table_name;'),
    'json': ('JSON', 'true false null', '', '', '', '{"value": $*}'),
}


def profiles():
    for slug, (label, words, types, builtins, comment, template) in LANGUAGES.items():
        rules = []
        if slug == 'lua':
            rules.append(dict(kind='span', style='comment', start='--[[', end=']]'))
        if slug not in ('python', 'lua', 'ruby', 'bash', 'json'):
            rules.append(dict(kind='span', style='comment', start='/*', end='*/'))
        if comment:
            rules.append(dict(kind='line', style='comment', start=comment))
        if slug == 'python':
            for quote in ('"""', "'''"):
                rules.append(dict(kind='span', style='string', start=quote, end=quote, escape='\\'))
        for quote in ('"',) if slug == 'json' else ('"', "'"):
            rules.append(dict(kind='span', style='string', start=quote, end=quote, escape='\\'))
        if slug in ('javascript', 'typescript', 'go', 'bash', 'ruby'):
            rules.append(dict(kind='span', style='string', start='`', end='`', escape='\\'))
        rules.append(dict(kind='number', style='number'))
        for style, text in [('keyword', words), ('type', types)]:
            for word in text.split():
                rules.append(dict(kind='word', style=style, start=word))
                if slug == 'sql':
                    rules.append(dict(kind='word', style=style, start=word.lower()))
        items = [dict(text=word, description=f'{label} {kind}')
                 for kind, text in [('keyword', words), ('type', types), ('builtin', builtins)]
                 for word in text.split()]
        items += [dict(text=f':{slug}-example', description=f'Insert {label} starter'),
                  dict(text=f':{slug}-doc', description=f'Application documentation action for {label}')]
        directives = [dict(name=f'{slug}-example', expansion=template, help=f'Insert a {label} starter; $* is argument text'),
                      dict(name=f'{slug}-doc', action=f'documentation:{slug}', help=f'Ask the host for {label} documentation')]
        for extension, field, values in [('mlsyn', 'rules', rules), ('mlac', 'items', items), ('mldir', 'directives', directives)]:
            yield f'{slug}.{extension}', json.dumps(dict(version=1, language=label, **{field: values}), indent=2, ensure_ascii=True) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    directory = ROOT / 'library/librl'
    if not args.check:
        directory.mkdir(parents=True, exist_ok=True)
    for name, text in profiles():
        path = directory / name
        if args.check:
            if not path.exists() or path.read_text() != text:
                raise SystemExit(f'stale profile: {path}')
        else:
            path.write_text(text)
