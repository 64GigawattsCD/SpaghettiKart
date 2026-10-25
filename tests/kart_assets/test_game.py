"""Muted end-to-end tests through native game input and renderer readback."""
import json
import math
from pathlib import Path
import time
import unittest
import numpy as np
from PIL import Image
from game_client import ROOT, launch, read, run, send


def wait_for(root, predicate, timeout=40):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        state = read(root / 'state.json')
        if state and predicate(state):
            return state
        time.sleep(.1)
    raise TimeoutError(root)


class GameTests(unittest.TestCase):
    def test_muted_game(self):
        evidence = {}
        for backend in ('OpenGL', 'DirectX11'):
            with self.subTest(backend=backend):
                root = ROOT / 'build/kart-game-test' / (backend + '-' + time.strftime('%Y%m%d-%H%M%S'))
                process = launch(root, backend)
                try:
                    wait_for(root, lambda r: r['state']['race_state'] == 3)
                    report = run(root, read(Path(__file__).with_name('palette-plan.json')))
                    self.assertEqual(len(report['captures']), 13)
                    shots = {c['file'].split('-')[1]: c for c in report['captures']}
                    expected = [(216,24,28), (28,184,66), (48,201,157), (42,107,235),
                                (190,113,40), (247,211,32), (245,113,176), (239,130,32)]
                    names = ['mario', 'luigi', 'yoshi', 'toad', 'dk', 'wario', 'peach', 'bowser']
                    floor = None
                    matched = []
                    for index, (name, color) in enumerate(zip(names, expected)):
                        shot = shots[name]
                        self.assertEqual(shot['state']['character'], index)
                        pixels = np.array(Image.open(root / shot['file'])).astype(float)
                        self.assertGreater(pixels.std(), 30)
                        # Match paint hue independent of baked lighting, in the nose area.
                        nose = pixels[425:475, 365:595, :3]
                        target = np.array(color, dtype=float)
                        target /= np.linalg.norm(target)
                        length = np.linalg.norm(nose, axis=2)
                        similarity = np.sum(nose * target, axis=2) / np.maximum(length, 1)
                        count = int(np.sum((similarity > .997) & (length > 50)))
                        self.assertGreater(count, 150, (backend, name, count))
                        matched.append(count)
                        metal = pixels[615:690, 420:540]
                        if floor is not None:
                            self.assertLess(np.abs(metal-floor).mean(), 2, 'Recolor changed the floor material')
                        floor = metal
                    self.assertFalse(shots['third']['state']['first_person'])
                    self.assertTrue(shots['first']['state']['first_person'])
                    drive = next(e for e in report['events'] if e['step'].get('capture') == 'drive')
                    distance = math.dist(drive['start']['position'], drive['end']['position'])
                    self.assertGreater(distance, 100)
                    view = np.array(drive['end']['look_at']) - drive['end']['camera']
                    velocity = np.array(drive['end']['velocity'])
                    alignment = float(np.dot(view, velocity)/(np.linalg.norm(view)*np.linalg.norm(velocity)))
                    self.assertGreater(alignment, .99)
                    straight = np.array(Image.open(root / shots['mario']['file'])).astype(float)
                    steering = np.array(Image.open(root / shots['steer']['file'])).astype(float)
                    changed = np.abs(straight[380:550,380:590]-steering[380:550,380:590]).mean()
                    self.assertGreater(changed, 3)
                    wait_for(root, lambda r: r['status'] == 'stopped')
                    stereo = None
                    if backend == 'OpenGL':
                        stereo = run(root, read(Path(__file__).with_name('stereo-plan.json')))
                        self.assertEqual([c['eye'] for c in stereo['captures']], ['left','right','left','right'])
                        for a, b in zip(stereo['captures'][::2], stereo['captures'][1::2]):
                            left = np.array(Image.open(root/a['file'])).astype(float)
                            right = np.array(Image.open(root/b['file'])).astype(float)
                            self.assertGreater(left.std(), 20)
                            self.assertGreater(right.std(), 20)
                            self.assertGreater(np.abs(left-right).mean(), 1)
                        wait_for(root, lambda r: r['status'] == 'stopped')
                    # A client disappearing must release all simulated holds.
                    (root / 'heartbeat').touch()
                    send(root, {'id':'watchdog', 'steps':[{'frames':600,'buttons':32768,'stick_x':50}]})
                    wait_for(root, lambda r: r['status'] == 'heartbeat_expired', 10)
                    end = wait_for(root, lambda r: r['state']['button'] == 0 and r['state']['stick_x'] == 0)
                    self.assertEqual(end['state']['queued'], 0)
                    # Explicit stop must also release inputs immediately.
                    (root/'heartbeat').touch()
                    send(root, {'id':'stop-test','steps':[{'frames':600,'buttons':32768}]})
                    wait_for(root, lambda r: r['id'] == 'stop-test' and r['state']['button'] == 32768)
                    send(root, {'op':'stop'})
                    wait_for(root, lambda r: r['status'] == 'stopped' and r['state']['button'] == 0)
                    send(root, {'id':'bad-plan','steps':[{'frames':20,'stick_x':81}]})
                    rejected = wait_for(root, lambda r: r['status'] == 'error')
                    self.assertIn('Axis outside', rejected['error'])
                    self.assertEqual(rejected['state']['button'], 0)
                    config = read(root / 'spaghettify.cfg.json')
                    # Fresh configuration mutes every game audio bus before process launch.
                    for key in ('gGameMasterVolume','gMainMusicVolume','gSFXMusicVolume','gEnvironmentVolume'):
                        self.assertEqual(config['CVars'][key], 0.0)
                    evidence[backend] = {'root':str(root), 'paint_pixels': matched, 'distance':distance,
                                         'forward_alignment':alignment, 'steering_image_delta':changed,
                                         'synthetic_stereo':stereo is not None, 'watchdog':True, 'stop':True}
                    (root/'verification.json').write_text(json.dumps(evidence[backend], indent=2))
                finally:
                    if process.poll() is None:
                        process.terminate()
                    process.wait(timeout=15)
        (ROOT/'build/kart-game-test/verification.json').write_text(json.dumps(evidence, indent=2))


if __name__ == '__main__':
    unittest.main(verbosity=2)
