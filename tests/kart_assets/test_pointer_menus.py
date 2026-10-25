"""Native pointer menu route. Default: offline checks; --run: muted game session.

python tests/kart_assets/test_pointer_menus.py --run --backend OpenGL
No compilation, deployment, desktop input, or window capture is performed.
"""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import time
import unittest
from unittest.mock import patch

from game_client import ROOT, launch, read, run, send

MAIN, CHARACTER, COURSE, RACING = 11, 12, 13, 4
A_BUTTON, B_BUTTON, START_BUTTON = 0x8000, 0x4000, 0x1000
MUTE_CVARS = ('gGameMasterVolume', 'gMainMusicVolume',
              'gSFXMusicVolume', 'gEnvironmentVolume')


def target_center(menu, context, target_id):
    matches = [t for t in menu['targets'] if (t['context'], t['id']) == (context, target_id)]
    if len(matches) != 1:
        raise AssertionError(f'Expected one target {context}:{target_id}, got {matches}')
    target = matches[0]
    x, y, w, h = (target[k] for k in ('x', 'y', 'w', 'h'))
    if not all(math.isfinite(n) for n in (x, y, w, h)) or w <= 0 or h <= 0:
        raise AssertionError(f'Invalid target rectangle: {target}')
    point = [x + w / 2, y + h / 2]
    # The native hit tester scans back to front. Detect an obscured target.
    hit = next(t for t in reversed(menu['targets'])
               if t['x'] <= point[0] < t['x'] + t['w']
               and t['y'] <= point[1] < t['y'] + t['h'])
    if (hit['context'], hit['id']) != (context, target_id):
        raise AssertionError(f'Target {target} is covered by {hit}')
    return point


def click_steps(point, hold=2):
    return [{'frames': hold, 'pointer': [*point, 1]},
            {'frames': 6, 'pointer': [*point, 0]}, {'frames': 4}]


def image_relative(point, aspect):
    if not math.isfinite(aspect) or aspect <= 0:
        raise AssertionError(f'Invalid rendered aspect: {aspect}')
    return [.5 + (point[0] - 160) / (240 * aspect), point[1] / 240]


def require(condition, detail):
    if not condition:
        raise AssertionError(detail)


class MenuSession:
    def __init__(self, root, process, pointer_ui=False):
        self.root, self.process = root, process
        self.pointer_ui = pointer_ui
        self.sequence = 0
        self.checkpoints = {}

    def wait(self, predicate, timeout=45):
        deadline = time.monotonic() + timeout
        last = None
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError(f'Game exited: {self.process.returncode}; logs: {self.root}')
            last = read(self.root / 'state.json') or last
            if last and predicate(last):
                return last
            time.sleep(.05)
        raise TimeoutError(f'Native menu wait: {json.dumps(last)}')

    def plan(self, name, steps):
        self.sequence += 1
        if self.pointer_ui and any('pointer' in step for step in steps):
            aspect = self.wait(lambda r: 'aspect_ratio' in r['state'])['state']['aspect_ratio']
            steps = [dict(step) for step in steps]
            for step in steps:
                if 'pointer' in step:
                    x, y, down = step.pop('pointer')
                    step['pointerUi'] = [*image_relative([x, y], aspect), down]
        return run(self.root, {'id': f'{self.sequence:03d}-{name}', 'steps': steps}, timeout=90)

    def menu(self, selection, field=None, expected=None):
        return self.wait(lambda r: r['state']['gamestate'] != RACING
                         and r['state']['menu']['selection'] == selection
                         and not r['state']['menu']['fading']
                         and (field is None or r['state']['menu'][field] == expected))

    def point(self, context, target_id):
        report = self.wait(lambda r: not r['state']['menu']['fading'] and any(
            (t['context'], t['id']) == (context, target_id) for t in r['state']['menu']['targets']))
        return target_center(report['state']['menu'], context, target_id)

    def click(self, name, context, target_id, hold=2):
        return self.plan(name, click_steps(self.point(context, target_id), hold))

    def options_page(self, page):
        return self.wait(lambda r: r['state']['menu'].get('options', {}).get('open')
                         and r['state']['menu']['options']['page'] == page
                         and not r['state']['menu']['fading'])['state']['menu']['options']

    def option_click(self, name, page, label, direction=0):
        for _ in range(40):
            options = self.options_page(page)
            row = next((r for r in options['rows'] if r['label'] == label), None)
            require(row and row['enabled'], {'page': page, 'label': label, 'options': options})
            if row['visible']:
                return self.click(name, options['context'], row['id'] + (1 if direction < 0 else 2 if direction > 0 else 0))
            first = next(r['index'] for r in options['rows'] if r['visible'])
            self.click(name+'-page', options['context'], 1002 if row['index'] < first else 1003)
        raise AssertionError('Option did not become visible: '+label)

    def options_back(self, page):
        options = self.options_page(page)
        return self.click('options-back', options['context'], 1000)

    def checkpoint(self, name):
        report = self.plan(name, [{'frames': 4, 'capture': name}])
        require(len(report['captures']) == 1, report)
        self.checkpoints[name] = report['state']
        return report

    def still_player_menu(self, report):
        state = report['state']
        require(state['menu']['selection'] == MAIN and state['menu']['main_selection'] == 3, report)
        require(state['pointer'][2] == 0 and state['queued'] == 0, report)
        require(state.get('pointer_ui') is None, report)

    def cancellation_checks(self):
        point = self.point(103, 0)
        report = self.plan('drag-off', [
            {'frames': 12, 'pointer': [*point, 1]},
            {'frames': 6, 'pointer': [-10000, -10000, 0]}, {'frames': 4}])
        self.still_player_menu(report)
        self.still_player_menu(self.plan('unfinished-hold', [{'frames': 8, 'pointer': [*point, 1]}]))
        self.still_player_menu(self.plan('omitted-pointer', [
            {'frames': 8, 'pointer': [*point, 1]}, {'frames': 2},
            {'frames': 4, 'pointer': [*point, 0]}]))
        for name in ('explicit-stop', 'heartbeat-expiry'):
            (self.root / 'heartbeat').touch()
            send(self.root, {'id': name, 'steps': [{'frames': 1800, 'pointer': [*point, 1]}]})
            self.wait(lambda r: r['id'] == name and r['status'] == 'running'
                      and r['state']['pointer'][2] == 1)
            if name == 'explicit-stop':
                send(self.root, {'op': 'stop'})
            terminal = 'stopped' if name == 'explicit-stop' else 'heartbeat_expired'
            report = self.wait(lambda r: r['id'] == name and r['status'] == terminal, timeout=12)
            self.still_player_menu(report)
            (self.root / f'{name}-report.json').write_text(json.dumps(report, indent=2))
            self.still_player_menu(self.plan(name + '-release', [{'frames': 4, 'pointer': [*point, 0]}]))
        invalid = [None, [], [1, 2], [1, 2, 0, 4], ['1', 2, 0], [True, 2, 0],
                   [1, 2, True], [1, 2, .5], [1, 2, -1], [1, 2, 2], [1e100, 2, 0]]
        for index, pointer in enumerate(invalid):
            name = f'invalid-pointer-{index}'
            (self.root / 'heartbeat').touch()
            send(self.root, {'id': name, 'steps': [
                {'frames': 4, 'pointer': [*point, 1]}, {'frames': 1, 'pointer': pointer}]})
            report = self.wait(lambda r: r['id'] == name and r['status'] == 'error')
            require('Pointer' in report['error'], report)
            self.still_player_menu(report)
        self.plan('clear-error', [{'frames': 2}])

    def stale_context_check(self):
        # Unlock while a transmission target is held. Its release must not
        # confirm a different screen or relock the character.
        point = self.point(251, 100)
        report = self.plan('stale-context', [
            {'frames': 4, 'pointer': [*point, 1]},
            {'frames': 1, 'pointer': [*point, 1], 'buttons': B_BUTTON | 0x80},
            {'frames': 8, 'pointer': [*point, 1]},
            {'frames': 4, 'pointer': [*point, 0]}, {'frames': 4}])
        require(not report['state']['menu']['players'][0]['locked'], report)
        require(not report['events'][2]['end']['menu']['players'][0]['locked'], report)
        require(any(t['context'] == 201 for t in report['state']['menu']['targets']), report)
        self.click('character-relock', 201, 2)

    def route(self):
        self.menu(MAIN, 'main_selection', 3)
        self.checkpoint('main')
        self.cancellation_checks()
        report = self.click('one-player', 103, 0, hold=30)
        require(report['events'][0]['end']['menu']['main_selection'] == 3, report)
        require(report['events'][1]['end']['menu']['main_selection'] == 4, report)
        self.menu(MAIN, 'main_selection', 4)
        self.checkpoint('player')
        self.click('grand-prix', 104, 100)
        self.menu(MAIN, 'main_selection', 5)
        self.checkpoint('mode')
        self.click('100cc', 105, 201)
        self.menu(MAIN, 'main_selection', 6)
        self.checkpoint('cc')
        self.click('main-ok', 106, 300)
        report = self.menu(CHARACTER, 'player_selection', 1)
        require(report['state']['menu']['player_count'] == 1, report)
        require(report['state']['menu']['mode'] == 0 and report['state']['menu']['cc'] == 1, report)
        self.click('luigi', 201, 2)
        report = self.checkpoint('character')
        require(report['state']['menu']['players'][0]['locked'], report)
        self.stale_context_check()
        self.click('automatic', 251, 100)
        report = self.menu(CHARACTER, 'player_selection', 2)
        player = report['state']['menu']['players'][0]
        require(player['character'] == 1 and player['confirmed'] and player['transmission'] == 1, report)
        self.checkpoint('transmission')
        self.click('character-ok', 252, 300)
        self.menu(COURSE, 'sub_selection', 1)
        self.click('mushroom-cup', 401, 0)
        # GP commits a cup; individual course rows belong to TT/Versus.
        report = self.menu(COURSE, 'sub_selection', 3)
        require(report['state']['menu']['cup'] == 0 and report['state']['menu']['course'] == 0, report)
        self.checkpoint('course')
        self.click('course-ok', 403, 300)
        report = self.wait(lambda r: r['state']['gamestate'] == RACING
                           and r['state']['race_state'] == 3, timeout=90)
        require(report['state']['character'] == 1 and report['state']['automatic'], report)
        self.checkpoint('race')
        self.plan('pause-input', [{'frames': 1, 'buttons': START_BUTTON}, {'frames': 8}])
        self.options_page('PAUSED')
        self.checkpoint('pause')
        self.option_click('continue', 'PAUSED', 'RESUME RACE')
        self.wait(lambda r: r['state']['gamestate'] == RACING and r['state']['menu']['paused'] == 0)
        self.checkpoint('resume')
        report = self.plan('drive-after-resume', [{'frames': 90, 'buttons': A_BUTTON}, {'frames': 4}])
        require(math.dist(report['events'][0]['start']['position'],
                          report['events'][0]['end']['position']) > 10, report)
        require(report['state']['pointer'][2] == 0, report)
        return {'status': 'passed', 'root': str(self.root), 'checkpoints': self.checkpoints,
                'route_input': 'pointerUi' if self.pointer_ui else 'pointer',
                'checks': ['drag-off', 'held-no-repeat', 'stale-context', 'unfinished-hold',
                           'omitted-pointer', 'explicit-stop', 'heartbeat-expiry', 'invalid-pointer',
                           'menu-route', 'resume-movement'], 'paid_service_credits': 0}


class PointerClientTests(unittest.TestCase):
    def test_centered_native_coordinates_include_wide_x(self):
        menu = {'targets': [{'context': 103, 'id': 0, 'x': -80, 'y': 10, 'w': 40, 'h': 20}]}
        self.assertEqual(target_center(menu, 103, 0), [-60, 20])

    def test_context_and_geometry_checks(self):
        target = {'context': 103, 'id': 0, 'x': 10, 'y': 10, 'w': 40, 'h': 20}
        for targets, context in (([target], 104), ([target, target], 103),
                                 ([dict(target, w=0)], 103), ([dict(target, x=math.inf)], 103),
                                 ([target, dict(target, id=1)], 103)):
            with self.subTest(targets=targets, context=context), self.assertRaises(AssertionError):
                target_center({'targets': targets}, context, 0)

    def test_click_has_distinct_press_and_release(self):
        steps = click_steps([12, 34], hold=30)
        self.assertEqual(steps[0], {'frames': 30, 'pointer': [12, 34, 1]})
        self.assertEqual(steps[1], {'frames': 6, 'pointer': [12, 34, 0]})
        self.assertNotIn('pointer', steps[2])
        self.assertTrue(all('buttons' not in step for step in steps))

    def test_image_relative_mapping(self):
        self.assertEqual(image_relative([160, 120], 16 / 9), [.5, .5])
        self.assertEqual(image_relative([0, 0], 4 / 3), [0, 0])
        self.assertEqual(image_relative([320, 240], 4 / 3), [1, 1])
        self.assertAlmostEqual(image_relative([0, 0], 16 / 9)[0], .125)
        self.assertLess(image_relative([-10000, -10000], 16 / 9)[0], 0)

    def test_image_relative_requires_valid_aspect(self):
        for aspect in (0, -1, math.inf, math.nan):
            with self.subTest(aspect=aspect), self.assertRaises(AssertionError):
                image_relative([160, 120], aspect)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', help='Launch a muted native test instance')
    parser.add_argument('--backend', choices=('OpenGL', 'DirectX11'), default='OpenGL')
    parser.add_argument('--root', type=Path)
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/x64/Release/Spaghettify.exe')
    parser.add_argument('--pointer-ui', action='store_true', help='Route clicks through the image overlay transform')
    parser.add_argument('--size', type=int, nargs=2, default=(960, 720), metavar=('WIDTH', 'HEIGHT'))
    args = parser.parse_args()
    if not args.run:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(PointerClientTests)
        raise SystemExit(not unittest.TextTestRunner(verbosity=2).run(suite).wasSuccessful())
    root = (args.root or ROOT / 'build/pointer-menu-test' /
            (args.backend + '-' + time.strftime('%Y%m%d-%H%M%S'))).resolve()
    cvars = {'gSkipIntro': 3, 'gArcadeKart.DebugQuickBootHudRace': 0, 'gEnableDebugMode': 0}
    cvars.update({name: 0.0 for name in MUTE_CVARS})
    # Keep inherited audio diagnostics out of the menu-only test instance.
    with patch.dict(os.environ):
        os.environ.pop('SK_AUDIO_PROBE', None)
        process = launch(root, args.backend, cvars=cvars, size=tuple(args.size), executable=args.binary)
    try:
        evidence = MenuSession(root, process, pointer_ui=args.pointer_ui).route()
        config = read(root / 'spaghettify.cfg.json')
        require(all(config['CVars'][name] == 0.0 for name in MUTE_CVARS), config)
        (root / 'verification.json').write_text(json.dumps(evidence, indent=2))
        print(json.dumps({'status': evidence['status'], 'root': str(root)}))
    except Exception as exc:
        (root / 'failure.json').write_text(json.dumps({'error': str(exc), 'last': read(root / 'state.json')}, indent=2))
        raise
    finally:
        if process.poll() is None:
            try:
                if not (root / 'command.json').exists():
                    send(root, {'op': 'stop'})
            except (OSError, RuntimeError):
                pass
            process.terminate()
        try:
            process.wait(timeout=15)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=15)


if __name__ == '__main__':
    main()
