"""Direct, opt-in game test client. No desktop input or window capture."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    try:
        return json.loads(path.read_text())
    except (OSError, json.JSONDecodeError):
        return None


def send(root, command):
    path = root / 'command.json'
    if path.exists():
        raise RuntimeError('Previous command is still pending')
    temp = root / 'command.pending'
    temp.write_text(json.dumps(command))
    temp.replace(path)


def run(root, command, timeout=180):
    (root / 'heartbeat').touch()
    send(root, command)
    until = time.monotonic() + timeout
    completed = False
    try:
        while time.monotonic() < until:
            (root / 'heartbeat').touch()
            state = read(root / 'state.json')
            if state and state['id'] == command['id'] and state['status'] not in ('running', 'idle'):
                if state['status'] != 'complete' or state['error']:
                    raise RuntimeError(state)
                (root / (command['id'] + '-report.json')).write_text(json.dumps(state, indent=2))
                from PIL import Image
                for capture in state['captures']:
                    image = root / capture['file']
                    Image.open(image).save(image.with_suffix('.png'))
                completed = True
                return state
            time.sleep(.1)
        raise TimeoutError('Game test timed out')
    finally:
        if not completed:
            if (root / 'command.json').exists():
                (root / 'command.json').unlink()
            send(root, {'op': 'stop'})


def launch(root, backend, vr=False, controllers=None, size=(960,720), cvars=None, executable=None):
    root.mkdir(parents=True, exist_ok=False)
    runtime = ROOT / 'build/x64/Debug'
    for name in ('mk64.o2r', 'spaghetti.o2r'):
        shutil.copy2(runtime / name, root / name)
    settings = {'Window': {'Backend': {'Id': 2 if backend == 'OpenGL' else 1, 'Name': backend},
                           'Fullscreen': {'Enabled': False}, 'Width': size[0], 'Height': size[1]},
                'CVars': {'gSkipIntro': 2, 'gArcadeKart.DebugQuickBootHudRace': 1,
                          'gFlatViewMode': 1, 'gAllowBackgroundInputs': 0,
                          'gVRViewMode': 1,
                          'gArcadeKart.ControllerRumble.Enabled': 0,
                          'gGameMasterVolume': 0.0, 'gMainMusicVolume': 0.0,
                          'gSFXMusicVolume': 0.0, 'gEnvironmentVolume': 0.0}}
    if controllers is not None:
        settings['CVars']['gControllers'] = controllers
    if cvars:
        settings['CVars'].update(cvars)
    (root / 'spaghettify.cfg.json').write_text(json.dumps(settings))
    env = os.environ.copy()
    env['SK_TEST_ROOT'] = str(root)
    env['SHIP_HOME'] = str(root)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    with (root / 'stdout.log').open('w') as out, (root / 'stderr.log').open('w') as err:
        process = subprocess.Popen([str(executable or runtime / 'Spaghettify.exe'), '--vr' if vr else '--novr'], cwd=root,
                                   env=env, startupinfo=startup, stdout=out, stderr=err)
    (root / 'process.json').write_text(json.dumps({'pid': process.pid, 'backend': backend}))
    return process


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('op', choices=['launch', 'state', 'run', 'stop'])
    parser.add_argument('root', type=Path)
    parser.add_argument('--plan', type=Path)
    parser.add_argument('--vr', action='store_true')
    parser.add_argument('--backend', default='OpenGL', choices=['OpenGL', 'DirectX11'])
    args = parser.parse_args()
    root = args.root.resolve()
    if args.op == 'launch':
        print(json.dumps({'pid': launch(root, args.backend, args.vr).pid, 'root': str(root)}))
    elif args.op == 'state':
        print(json.dumps(read(root / 'state.json'), indent=2))
    elif args.op == 'stop':
        send(root, {'op': 'stop'})
    else:
        result = run(root, read(args.plan))
        print(json.dumps({'status': result['status'], 'captures': result['captures']}, indent=2))


if __name__ == '__main__':
    main()
