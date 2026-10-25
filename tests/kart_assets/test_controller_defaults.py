"""Muted runtime checks for defaults, legacy upgrades, and later custom mappings."""
import copy
import time
import unittest
import uuid
from game_client import ROOT, launch, read


def fixture(custom=False):
    controllers = {'ButtonMappings': {}}
    for port in range(4):
        buttons = {}
        for action, mask, button in [
                ('First Person View', 1, 3 if custom else 2),
                ('Use Item', 8192, 2 if custom else 10),
                ('Capture Screenshot', -2147483648, 4),
                ('Menu Cancel', 128, 1)]:
            mapping_id = f'P{port}-B{mask}-SDLB{button}'
            buttons[action + 'ButtonMappingIds'] = mapping_id + ','
            controllers['ButtonMappings'][mapping_id] = {
                'ButtonMappingClass': 'SDLButtonToButtonMapping', 'Bitmask': mask, 'SDLControllerButton': button}
        if not custom:
            mapping_id = f'P{port}-B4-SDLB1'
            buttons['Ceremony Debug Select DKButtonMappingIds'] = mapping_id + ','
            controllers['ButtonMappings'][mapping_id] = {
                'ButtonMappingClass': 'SDLButtonToButtonMapping', 'Bitmask': 4, 'SDLControllerButton': 1}
        for action, mask, scancode in [('First Person View', 1, 23), ('Capture Screenshot', -2147483648, 88)]:
            mapping_id = f'P{port}-B{mask}-KB{scancode}'
            buttons[action + 'ButtonMappingIds'] += mapping_id + ','
            controllers['ButtonMappings'][mapping_id] = {
                'ButtonMappingClass': 'KeyboardKeyToButtonMapping', 'Bitmask': mask, 'KeyboardScancode': scancode}
        controllers[f'Port{port+1}'] = {'HasConfig': 1, 'Buttons': buttons}
    return controllers


def ids(config, port, action):
    return set(filter(None, config[f'Port{port+1}']['Buttons'].get(action + 'ButtonMappingIds', '').split(',')))


class ControllerDefaultsTests(unittest.TestCase):
    def boot(self, controllers=None):
        root = ROOT / 'build/kart-game-test' / ('bindings-' + uuid.uuid4().hex[:10])
        process = launch(root, 'OpenGL', controllers=controllers)
        try:
            until = time.monotonic() + 45
            while time.monotonic() < until:
                state = read(root / 'state.json')
                config = read(root / 'spaghettify.cfg.json')
                if state and state['state']['race_state'] == 3 and config:
                    return config['CVars']['gControllers']
                if process.poll() is not None:
                    self.fail(f'Game exited; logs: {root}')
                time.sleep(.15)
            self.fail(f'No race; logs: {root}')
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)

    def test_fresh_defaults(self):
        config = self.boot()
        self.assertIn('P0-B1-SDLB4', ids(config, 0, 'First Person View'))
        self.assertNotIn('P0-B1-SDLB2', ids(config, 0, 'First Person View'))
        self.assertTrue({'P0-B8192-SDLB1', 'P0-B8192-SDLB10'} <= ids(config, 0, 'Use Item'))
        self.assertIn('P0-B128-SDLB1', ids(config, 0, 'Menu Cancel'))
        self.assertNotIn('P0-B4-SDLB1', ids(config, 0, 'Ceremony Debug Select DK'))
        self.assertNotIn('P0-B-2147483648-SDLB4', ids(config, 0, 'Capture Screenshot'))
        self.assertIn('P0-B-2147483648-KB88', ids(config, 0, 'Capture Screenshot'))

    def test_legacy_upgrade_and_idempotence(self):
        config = self.boot(fixture())
        for port in range(4):
            self.assertEqual(ids(config, port, 'First Person View'), {f'P{port}-B1-SDLB4', f'P{port}-B1-KB23'})
            self.assertEqual(ids(config, port, 'Use Item'), {f'P{port}-B8192-SDLB1', f'P{port}-B8192-SDLB10'})
            self.assertEqual(ids(config, port, 'Capture Screenshot'), {f'P{port}-B-2147483648-KB88'})
            self.assertNotIn(f'P{port}-B4-SDLB1', config['ButtonMappings'])
            self.assertEqual(config[f'Port{port+1}']['SelectCameraItemDefaultsVersion'], 1)
        again = self.boot(copy.deepcopy(config))
        for port in range(4):
            for action in ('First Person View', 'Use Item', 'Capture Screenshot', 'Menu Cancel'):
                self.assertEqual(ids(config, port, action), ids(again, port, action))

    def test_custom_bindings_survive(self):
        original = fixture(custom=True)
        config = self.boot(copy.deepcopy(original))
        for port in range(4):
            for action in ('First Person View', 'Use Item', 'Capture Screenshot', 'Menu Cancel'):
                self.assertEqual(ids(original, port, action), ids(config, port, action))
        # A later explicit remap to X must not be migrated back on every launch.
        later = fixture()
        for port in range(4):
            later[f'Port{port+1}']['SelectCameraItemDefaultsVersion'] = 1
        config = self.boot(copy.deepcopy(later))
        self.assertEqual(ids(later, 0, 'First Person View'), ids(config, 0, 'First Person View'))


if __name__ == '__main__':
    unittest.main()
