"""Project-isolated Blender tooling. Never writes game runtime assets."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CACHE = ROOT / '.cache' / 'kart-tools'


def git(*args, cwd=None):
    return subprocess.check_output(['git', *args], cwd=cwd, text=True).strip()


def setup():
    sources = json.loads((HERE / 'sources.json').read_text())['sources']
    for source in sources:
        dest = CACHE / source['path']
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            flags = ['--filter=blob:none', '--sparse'] if source.get('sparse') else []
            git('clone', '--depth', '1', *flags, source['url'], str(dest))
        if git('remote', 'get-url', 'origin', cwd=dest) != source['url']:
            raise RuntimeError(f'Unexpected remote in {dest}')
        if git('status', '--porcelain', '--untracked-files=no', cwd=dest):
            raise RuntimeError(f'Local edits in {dest}; left intact')
        if git('rev-parse', 'HEAD', cwd=dest) != source['revision']:
            git('fetch', '--depth', '1', 'origin', source['revision'], cwd=dest)
            git('checkout', '--detach', source['revision'], cwd=dest)
        if source.get('sparse'):
            git('sparse-checkout', 'set', *source['sparse'], cwd=dest)
        print(f"Ready: {source['path']} @ {source['revision'][:12]}")


def blender_path(explicit=None):
    candidates = [explicit, os.environ.get('BLENDER_PATH'),
                  r'F:\AI\Tools\Blender\versions\blender-5.2.2-windows-x64\blender.exe',
                  shutil.which('blender')]
    for candidate in candidates:
        if candidate and Path(candidate).is_file():
            return str(Path(candidate).resolve())
    raise RuntimeError('Set BLENDER_PATH or pass --blender to an installed Blender executable')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['setup', 'doctor', 'template', 'validate', 'export', 'selftest', 'open'])
    parser.add_argument('--blend', type=Path)
    parser.add_argument('--out', type=Path)
    parser.add_argument('--blender')
    args = parser.parse_args()
    if args.command == 'setup':
        setup()
        return
    for source in json.loads((HERE / 'sources.json').read_text())['sources']:
        dest = CACHE / source['path']
        if not dest.exists() or git('rev-parse', 'HEAD', cwd=dest) != source['revision']:
            raise RuntimeError('Tool sources missing or changed; run setup')
        if git('status', '--porcelain', '--untracked-files=no', cwd=dest):
            raise RuntimeError(f'Pinned source has local edits: {dest}')
    if args.command in ('validate', 'export', 'open') and not args.blend:
        parser.error('--blend is required')
    if args.blend and not args.blend.is_file():
        parser.error('Input .blend does not exist')
    if args.command != 'open' and not args.out:
        parser.error('--out is required and must be a new directory')
    out = args.out.resolve() if args.out else None
    if out:
        out.mkdir(parents=True, exist_ok=False)
    env = {key: value for key, value in os.environ.items() if not key.upper().startswith('PYTHON')}
    env['BLENDER_USER_SCRIPTS'] = str(CACHE / 'scripts')
    env['BLENDER_USER_CONFIG'] = str(CACHE / 'config')
    # Fast64 serializes sets of material flags; keep their output order repeatable.
    env['PYTHONHASHSEED'] = '0'
    env['PYTHONNOUSERSITE'] = '1'
    cmd = [blender_path(args.blender), '--factory-startup', '--disable-autoexec', '--python-use-system-env']
    if args.command != 'open':
        cmd.append('--background')
    cmd += ['--python-exit-code', '1', '--python', str(HERE / 'blender_job.py'), '--',
            args.command, str(out or ''), str(args.blend.resolve() if args.blend else '')]
    if args.command == 'open':
        # Explicit interactive command, separate from any existing Blender session.
        subprocess.Popen(cmd, env=env, cwd=ROOT)
    else:
        (out / 'job.json').write_text(json.dumps({'command': cmd}, indent=2))
        with (out / 'stdout.log').open('w', encoding='utf-8') as log:
            result = subprocess.run(cmd, env=env, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        if result.returncode:
            print((out / 'stdout.log').read_text(encoding='utf-8'), file=sys.stderr)
            raise RuntimeError(f'Blender returned {result.returncode}; see {out}')
        print(f'Complete: {out}')


if __name__ == '__main__':
    main()
