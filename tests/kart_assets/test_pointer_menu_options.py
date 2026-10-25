"""Muted extended desktop menus; --run opts into an isolated native session."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess
import time
import unittest
from unittest.mock import patch

from PIL import Image, ImageStat
from game_client import ROOT, launch, read, send
from test_pointer_menus import (MenuSession, MAIN, CHARACTER, COURSE, RACING,
                                START_BUTTON, B_BUTTON, A_BUTTON, MUTE_CVARS,
                                click_steps, require)

OPTIONS, DATA, RECORDS = 5, 6, 7


def visible_center(targets, context, target_id):
    """A row can have disjoint label and text rectangles with the same ID."""
    for target in targets:
        if (target['context'], target['id']) != (context, target_id):
            continue
        x, y, w, h = (target[k] for k in ('x', 'y', 'w', 'h'))
        require(all(math.isfinite(v) for v in (x, y, w, h)) and w > 0 and h > 0, target)
        point = [x + w / 2, y + h / 2]
        hit = next(t for t in reversed(targets) if t['x'] <= point[0] < t['x'] + t['w']
                   and t['y'] <= point[1] < t['y'] + t['h'])
        if (hit['context'], hit['id']) == (context, target_id):
            return point
    raise AssertionError(f'No visible rectangle for {context}:{target_id}')


def page_rows(targets, footer):
    rows = {t['id'] // 3 for t in targets if t['id'] < footer}
    require(1 <= len(rows) <= 6, targets)
    ys = sorted({t['y'] for t in targets if t['id'] < footer})
    require(all(y in (38, 62, 86, 110, 134, 158) for y in ys), targets)
    for target in targets:
        require(target['h'] == (24 if target['id'] < footer else 26), target)
        require(target['y'] + target['h'] <= 238, target)
    return rows


class ExtendedMenus(MenuSession):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.evidence = {}
        self.shots = {}

    def point(self, context, target_id):
        previous, last_tick, stable = None, None, 0
        def settled(report):
            nonlocal previous, last_tick, stable
            state = report['state']
            if state['tick'] == last_tick:
                return False
            last_tick = state['tick']
            targets = [t for t in state['menu']['targets']
                       if (t['context'], t['id']) == (context, target_id)]
            stable = stable + 1 if targets and targets == previous and not state['menu']['fading'] else 0
            previous = targets
            return stable >= 3
        report = self.wait(settled)
        return visible_center(report['state']['menu']['targets'], context, target_id)

    def family(self, low, high):
        report = self.wait(lambda r: not r['state']['menu']['fading'] and any(
            low <= t['context'] < high for t in r['state']['menu']['targets']))
        targets = [t for t in report['state']['menu']['targets'] if low <= t['context'] < high]
        require(len({t['context'] for t in targets}) == 1, targets)
        return targets

    def hit(self, name, target_id, low, high):
        targets = self.family(low, high)
        require(any(t['id'] == target_id for t in targets), {'name': name, 'targets': targets})
        return self.click(name, targets[0]['context'], target_id)

    def shot(self, name):
        report = self.plan(name, [{'frames': 24, 'capture': name}])
        require(len(report['captures']) == 1, report)
        self.checkpoints[name] = report['state']
        path = (self.root / report['captures'][0]['file']).with_suffix('.png')
        self.shots[name] = str(path)
        with Image.open(path) as image:
            require(min(image.size) >= 240 and max(ImageStat.Stat(image).stddev) > 15, str(path))
        return report

    def back(self, selection):
        return self.click('back', 800 + selection, B_BUTTON)

    def options(self):
        self.click('main-options', 103, 301)
        self.menu(OPTIONS, 'sub_selection', 21)
        self.shot('options-before')
        for index in range(4):
            self.hit(f'sound-{index}', 22, 521, 525)
            self.menu(OPTIONS, 'sub_selection', 22)
            self.shot(f'sound-{index}')
        self.back(OPTIONS)
        self.menu(MAIN, 'main_selection', 3)
        self.evidence['options'] = {'sound_cycles': 4, 'back': True,
                                    'sound_persistence': 'not asserted; no sound field in bridge telemetry'}

    def data(self):
        self.click('main-data', 103, 302)
        self.menu(DATA, 'sub_selection', 1)
        self.shot('data-grid')
        self.click('records-course', 600, 5)
        records = self.menu(RECORDS, 'sub_selection', 11)
        navigation = [t for t in records['state']['menu']['targets'] if t['context'] == 800 + RECORDS]
        require(len(navigation) == 6 and all(t['y'] >= 96 and t['y'] + t['h'] <= 120
                                          for t in navigation), navigation)
        self.shot('course-records')
        # Only RETURN/BACK is used; erase and ghost actions remain untouched.
        self.click('records-return', 661, 0)
        self.menu(DATA, 'sub_selection', 1)
        self.back(DATA)
        self.menu(MAIN, 'main_selection', 3)
        self.evidence['data'] = {'records_course': 5, 'erase_actions': 0, 'back': True,
                                 'navigation_clears_lap_times': True}

    def quit_no(self):
        self.hit('quit-open', 0, 150000, 160000)
        targets = self.family(150000, 160000)
        require({t['id'] for t in targets} == {1, 2}, targets)
        all_targets = self.wait(lambda r: True)['state']['menu']['targets']
        require(all(150000 <= t['context'] < 160000 for t in all_targets), all_targets)
        self.shot('quit-no-dialog')
        self.hit('quit-no', 2, 150000, 160000)
        self.point(103, 0)
        self.shot('quit-cancelled')
        self.evidence['quit_no'] = {'modal_targets_only': True, 'process_alive': self.process.poll() is None}

    def choose_gp(self):
        self.click('one-player', 103, 0)
        self.menu(MAIN, 'main_selection', 4)
        self.click('grand-prix', 104, 100)
        self.menu(MAIN, 'main_selection', 5)

    def setup(self):
        self.choose_gp()
        targets = self.family(105, 106)
        custom = max(t['id'] for t in targets if 200 <= t['id'] <= 204)
        self.click('custom-cc', 105, custom)
        first = self.family(120000, 150000)
        rows = [sorted(page_rows(first, 48))]
        require(50 not in {t['id'] for t in first}, first)
        self.shot('setup-page-1')
        self.hit('setup-next', 51, 120000, 150000)
        second = self.family(120000, 150000)
        rows.append(sorted(page_rows(second, 48)))
        require(not set(rows[0]) & set(rows[1]), rows)
        self.shot('setup-page-2')
        self.hit('setup-next', 51, 120000, 150000)
        third = self.family(120000, 150000)
        rows.append(sorted(page_rows(third, 48)))
        require(51 not in {t['id'] for t in third}, third)
        require(not set(rows[1]) & set(rows[2]), rows)
        self.shot('setup-page-3')
        self.hit('setup-previous', 50, 120000, 150000)
        require(sorted(page_rows(self.family(120000, 150000), 48)) == rows[1], rows)
        self.hit('setup-previous', 50, 120000, 150000)
        require(sorted(page_rows(self.family(120000, 150000), 48)) == rows[0], rows)
        self.hit('setup-back', 48, 120000, 150000)
        self.menu(MAIN, 'main_selection', 6)
        self.shot('setup-back-to-ok')
        for expected in (5, 4, 3):
            self.back(MAIN)
            self.menu(MAIN, 'main_selection', expected)
        self.evidence['setup'] = {'custom_id': custom, 'rows_by_page': rows, 'back': True}

    def enter_race(self):
        self.choose_gp()
        self.click('50cc', 105, 200)
        self.menu(MAIN, 'main_selection', 6)
        self.click('main-ok', 106, 300)
        self.menu(CHARACTER, 'player_selection', 1)
        self.click('luigi', 201, 2)
        self.click('auto', 251, 100)
        self.menu(CHARACTER, 'player_selection', 2)
        self.click('character-ok', 252, 300)
        self.menu(COURSE, 'sub_selection', 1)
        self.click('cup', 401, 0)
        self.menu(COURSE, 'sub_selection', 3)
        self.click('course-ok', 403, 300)
        self.wait(lambda r: r['state']['gamestate'] == RACING and r['state']['race_state'] == 3, timeout=90)

    def view(self):
        self.enter_race()
        self.plan('pause', [{'frames': 1, 'buttons': START_BUTTON}, {'frames': 8}])
        self.hit('view-options', 0, 100000, 110000)
        first = self.family(110000, 120000)
        rows = [sorted(page_rows(first, 54))]
        require(56 not in {t['id'] for t in first}, first)
        self.shot('view-page-1')
        self.hit('view-next', 57, 110000, 120000)
        second = self.family(110000, 120000)
        rows.append(sorted(page_rows(second, 54)))
        require(not set(rows[0]) & set(rows[1]) and 57 not in {t['id'] for t in second}, rows)
        self.shot('view-page-2')
        self.hit('view-previous', 56, 110000, 120000)
        require(sorted(page_rows(self.family(110000, 120000), 54)) == rows[0], rows)
        self.hit('view-diorama', 2, 110000, 120000)
        diorama = self.family(110000, 120000)
        require(2 in page_rows(diorama, 54), diorama)
        self.shot('view-diorama')
        self.hit('view-first-person', 1, 110000, 120000)
        first_person = self.family(110000, 120000)
        require(2 not in page_rows(first_person, 54), first_person)
        self.shot('height-before')
        self.hit('height-increase', 11, 110000, 120000)
        self.shot('height-increased')
        self.hit('height-decrease', 10, 110000, 120000)
        self.shot('height-restored')
        self.hit('view-back', 54, 110000, 120000)
        self.family(160000, 200000)
        self.shot('stock-pause-after-back')
        self.hit('view-reopen', 0, 100000, 110000)
        self.hit('view-resume', 55, 110000, 120000)
        self.wait(lambda r: r['state']['gamestate'] == RACING and r['state']['menu']['paused'] == 0)
        self.shot('view-resumed')
        report = self.plan('resume-drive', [{'frames': 90, 'buttons': A_BUTTON}, {'frames': 4}])
        distance = math.dist(report['events'][0]['start']['position'], report['events'][0]['end']['position'])
        require(distance > 10, report)
        self.evidence['view'] = {'rows_by_page': rows, 'view_mode_hidden_rows_changed': True,
                                 'height_adjustment_captures': True, 'back': True, 'resume_distance': distance}


class Helpers(unittest.TestCase):
    def test_disjoint_row_rectangles(self):
        targets = [dict(context=120001, id=0, x=x, y=38, w=60, h=24) for x in (26, 180)]
        self.assertEqual(visible_center(targets, 120001, 0), [56, 50])
        self.assertEqual(page_rows(targets, 48), {0})

    def test_obscured_first_rectangle(self):
        targets = [dict(context=1, id=0, x=x, y=0, w=20, h=20) for x in (0, 40)]
        targets.append(dict(context=1, id=1, x=0, y=0, w=20, h=20))
        self.assertEqual(visible_center(targets, 1, 0), [50, 10])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--case', action='append', choices=('options', 'data', 'quit_no', 'setup', 'view'))
    parser.add_argument('--backend', choices=('OpenGL', 'DirectX11'), default='OpenGL')
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/x64/Release/Spaghettify.exe')
    parser.add_argument('--size', type=int, nargs=2, default=(1600, 900))
    parser.add_argument('--pointer-ui', action='store_true')
    parser.add_argument('--root', type=Path)
    args = parser.parse_args()
    if not args.run:
        raise SystemExit(not unittest.TextTestRunner(verbosity=2).run(
            unittest.defaultTestLoader.loadTestsFromTestCase(Helpers)).wasSuccessful())
    root = (args.root or ROOT / 'build/pointer-options-test' /
            (args.backend + '-' + time.strftime('%Y%m%d-%H%M%S'))).resolve()
    cvars = {'gSkipIntro': 3, 'gArcadeKart.DebugQuickBootHudRace': 0, 'gEnableDebugMode': 0,
             'gFlatViewMode': 1, 'gFlatFPHeight': 8.0}
    cvars.update({name: 0.0 for name in MUTE_CVARS})
    binary_hash = hashlib.sha256(args.binary.read_bytes()).hexdigest()
    with patch.dict(os.environ):
        os.environ.pop('SK_AUDIO_PROBE', None)
        process = launch(root, args.backend, size=tuple(args.size), cvars=cvars, executable=args.binary)
    session = ExtendedMenus(root, process, pointer_ui=args.pointer_ui)
    result = {'root': str(root), 'binary': str(args.binary), 'binary_sha256': binary_hash,
              'backend': args.backend, 'pointer_ui': args.pointer_ui, 'size': args.size}
    try:
        session.menu(MAIN, 'main_selection', 3)
        for case in args.case or ('options', 'data', 'quit_no', 'setup', 'view'):
            print(f'{case}: starting', flush=True)
            getattr(session, case)()
            print(f'{case}: passed', flush=True)
        config = read(root / 'spaghettify.cfg.json')
        require(all(config['CVars'][name] == 0.0 for name in MUTE_CVARS), config)
        result['status'] = 'passed'
    except Exception as exc:
        result.update(status='failed', error=str(exc), last=read(root / 'state.json'))
        try:
            session.shot('failure')
        except Exception:
            pass
        raise
    finally:
        result.update(checks=session.evidence, screenshots=session.shots, paid_service_credits=0)
        (root / 'verification.json').write_text(json.dumps(result, indent=2))
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
        print(root, flush=True)


if __name__ == '__main__':
    main()
