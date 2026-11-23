from pathlib import Path
import json, re
root=Path(__file__).resolve().parents[1]
mods=json.loads((root/'docs/MODULES.json').read_text())
assert len(mods)>40
for key,item in mods.items():
    p=root/'tex'/f'on1x-{key}.sty'
    text=p.read_text()
    assert p.is_file()
    assert r'\ProvidesPackage{' in text
    assert r'\endinput' in text
    for dep in item['depends_on_modules']:
        assert dep in mods and dep!=key, (key,dep)
    assert any(word in text for word in ('newcommand','NewDocumentCommand','newenvironment','NewDocumentEnvironment','newtcolorbox','lstnewenvironment','lstdefinestyle','lstdefinelanguage','newcounter','newtheorem','newlist','newcolumntype','definecolor','colorlet','DeclareMathOperator','renewcommand','tcbset','forestset','tikzset','geometry','hypersetup','captionsetup','titleformat','fancyhead','setlist','lstinputlisting')), key
print(f'{len(mods)} independently named, non-empty style modules, dependencies and manifest: OK')
