#!/usr/bin/env python3
"""Inspect, bundle and verify MuLIB kernels with their library/ fixtures."""
import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HOME = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('mulib_housekeeping', HOME/'stdlib/mulib/housekeeping.py')
housekeeping = importlib.util.module_from_spec(spec)
spec.loader.exec_module(housekeeping)
c = housekeeping.contracts


def bundle(root, recipe, output):
    if output.exists() or output.is_symlink():raise ValueError('output already exists')
    housekeeping.check_generated(root, recipe)
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.on1x-mulib-', dir=output.parent))
    try:
        housekeeping.materialize(root, recipe, stage)
        (stage/'build-plan.json').write_text(c.dumps(recipe))
        inputs = dict(recipe['inputs'])
        inputs[str(Path(__file__).resolve())]=c.digest(__file__)
        for item in recipe['modules']:
            for field in ['header','manual']:
                path=housekeeping.header_path(root,item) if field=='header' else housekeeping.local(root,item[field])
                inputs[str(path)]=item[field+'_sha256']
        for record in recipe['files']:inputs[str(housekeeping.local(root,record['path']))]=record['sha256']
        artifacts = {p.relative_to(stage).as_posix():c.digest(p) for p in sorted(stage.rglob('*')) if p.is_file()}
        (stage/'lock.json').write_text(c.dumps(dict(schema='on1x.mulib-bundle/v1',inputs=inputs,artifacts=artifacts,safe_export=False)))
        for path, digest in inputs.items():
            if c.digest(path)!=digest:raise ValueError('bundle input drift: '+path)
        c.publish_new(stage, output)
    finally:
        if stage.exists():shutil.rmtree(stage)


def verify(directory, sources=False):
    lock = c.load(directory/'lock.json')
    if lock['schema']!='on1x.mulib-bundle/v1' or lock['safe_export'] is not False:raise ValueError('unsupported bundle lock')
    expected = set(lock['artifacts'])|{'lock.json'}
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob('*') if p.is_file()}
    if expected!=actual:raise ValueError('bundle inventory drift')
    for name, digest in lock['artifacts'].items():
        if c.digest(housekeeping.local(directory, name))!=digest:raise ValueError('bundle artifact drift: '+name)
    if sources:
        for name, digest in lock['inputs'].items():
            if c.digest(name)!=digest:raise ValueError('bundle source drift: '+name)


def main():
    cli=argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--root',type=Path)
    cli.add_argument('--mulib-path',action='append',default=[],metavar='DIR:DIR')
    sub=cli.add_subparsers(dest='mode',required=True)
    sub.add_parser('list')
    for mode in ['plan','bundle','audit']:
        p=sub.add_parser(mode);p.add_argument('libraries',nargs='*')
        p.add_argument('--fixture',action='append',default=[])
        if mode=='bundle':p.add_argument('--output',type=Path,required=True)
        if mode=='audit':p.add_argument('--all',action='store_true',help='verify every registered fixture group and embedding')
    p=sub.add_parser('verify');p.add_argument('directory',type=Path);p.add_argument('--sources',action='store_true')
    a=cli.parse_args()
    try:
        paths=housekeeping.search_paths(a.mulib_path)
        root=housekeeping.kernel_root(a.root,HOME.parent,paths)
        if a.mode=='verify':verify(a.directory.resolve(),a.sources);print('MuLIB bundle verified');return 0
        if a.mode=='list':
            modules=housekeeping.inventory(root);config=housekeeping.catalog(root,modules)
            for group in config['fixtures'].values():group['matched_files']=housekeeping.fixture_files(root,group)
            print(c.dumps(dict(config,schema='on1x.mulib-inventory/v1',modules=modules)),end='');return 0
        if a.mode=='audit' and a.all:
            config=housekeeping.catalog(root,housekeeping.inventory(root))
            a.fixture=[name for name,group in config['fixtures'].items() if group['kind']!='empty']
        recipe=housekeeping.plan(root,a.libraries,a.fixture,paths)
        if a.mode=='bundle':bundle(root,recipe,a.output.absolute());print(a.output)
        elif a.mode=='audit':housekeeping.check_generated(root,recipe);print('MuLIB fixture/kernel matches verified')
        else:print(c.dumps(recipe),end='')
    except (ValueError, OSError, KeyError, subprocess.TimeoutExpired) as error:
        print('on1x-mulib: '+str(error),file=sys.stderr);return 1
    return 0


if __name__=='__main__':sys.exit(main())
