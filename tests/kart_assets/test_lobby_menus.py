"""Native Local/Online menu checks. Gameplay is opt-in with --run and always muted."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time
import unittest
from unittest.mock import patch

from game_client import ROOT, launch, read, send
from test_pointer_menus import MenuSession, MUTE_CVARS, RACING, require


def matching_row(lobby, label):
    rows = [row for row in lobby['rows'] if row['label'] == label]
    require(len(rows) == 1, {'label': label, 'rows': rows})
    row = rows[0]
    require(row['enabled'], row)
    return row


class LobbySession(MenuSession):
    def page(self, name):
        return self.wait(lambda report: report['state']['menu']['lobby']['open']
                         and report['state']['menu']['lobby']['page'] == name
                         and not report['state']['menu']['fading'])['state']['menu']['lobby']

    def lobby_click(self, page, label, direction=0):
        for _ in range(20):
            lobby = self.page(page)
            row = matching_row(lobby, label)
            if row['visible']:
                return self.click(label.lower().replace(' ', '-'), lobby['context'],
                                  row['id'] + (1 if direction < 0 else 2 if direction > 0 else 0))
            first = min(r['index'] for r in lobby['rows'] if r['visible'])
            self.click('lobby-page', lobby['context'], 1002 if row['index'] < first else 1001)
        raise AssertionError('Menu row did not become visible: ' + label)

    def back(self, page):
        return self.click('back', self.page(page)['context'], 1000)

    def menus(self):
        main = self.page('MAIN')
        require(sum(p['joined'] for p in main['party']) == 1, main)
        for port in (1, 2, 3):
            self.click('join-' + str(port), self.page('MAIN')['context'], 410 + port)
            self.page('MAIN')
        main = self.page('MAIN')
        require(all(p['joined'] for p in main['party']), main)
        self.checkpoint('main-four-local')
        self.click('online', main['context'], 402)
        self.lobby_click('ONLINE', 'LAN SERVER BROWSER')
        self.page('LAN SERVER BROWSER')
        self.checkpoint('lan-browser')
        self.back('LAN SERVER BROWSER')
        self.lobby_click('ONLINE', 'HOST GAME')
        host = self.page('HOST SETUP')
        for label in ('RACE TYPE', 'CUP', 'PLAYER CAPACITY', 'SERVER NAME', 'SERVER PASSWORD'):
            matching_row(host, label)
        self.lobby_click('HOST SETUP', 'SERVER PASSWORD')
        self.page('SERVER PASSWORD')
        self.checkpoint('password-editor')
        self.back('SERVER PASSWORD')
        self.back('HOST SETUP')
        self.lobby_click('ONLINE', 'DIRECT CONNECT')
        self.lobby_click('DIRECT CONNECT', 'HOST ADDRESS')
        self.page('HOST ADDRESS')
        self.back('HOST ADDRESS')
        self.back('DIRECT CONNECT')
        self.back('ONLINE')
        require(all(p['joined'] for p in self.page('MAIN')['party']), 'Party survives submenu navigation')

    def local_race(self):
        self.click('local', self.page('MAIN')['context'], 401)
        setup = self.page('LOCAL RACE SETUP')
        require(matching_row(setup, 'RACE TYPE')['value'] == 'GRAND PRIX', setup)
        if matching_row(setup, 'CPU FILL')['value'] != 'ON':
            self.lobby_click('LOCAL RACE SETUP', 'CPU FILL')
        self.lobby_click('LOCAL RACE SETUP', 'CONTINUE TO LOBBY')
        lobby = self.page('LOBBY')
        require(lobby['phase'] == 2, lobby)
        self.click('drivers', self.page('LOBBY')['context'], 400)
        require(len(self.page('CHARACTER SELECT')['driver_selection']['players']) == 4, 'Shared pool has four local cursors')
        for port in range(4):
            self.click('pick-' + str(port), self.page('CHARACTER SELECT')['context'], 3100 + port)
        transmission = self.page('TRANSMISSION')
        for slot, player in enumerate(transmission['driver_selection']['players']):
            require(player['viewport'] == [(slot % 2) * 160, (slot // 2) * 120, 160, 120], player)
        for port in range(4):
            self.click('automatic-' + str(port), self.page('TRANSMISSION')['context'], 3300 + port * 10)
            self.click('confirm-transmission-' + str(port), self.page('TRANSMISSION')['context'], 3302 + port * 10)
        for port in range(4):
            self.click('ready-' + str(port), self.page('DRIVER READY')['context'], 3400 + port)
        self.click('start-local-race', self.page('LOBBY')['context'], 1003)
        report = self.wait(lambda r: r['state']['gamestate'] == RACING
                           and r['state']['network']['phase'] == 'Racing')
        require(report['state']['network']['viewports'] == 4, report)
        require(len(report['state']['network']['racers']) == 8, report)
        require(not report['state']['menu']['lobby']['open'], report)
        self.plan('four-independent-pads', [{'frames': 90, 'pads': [
            {'port': 0, 'buttons': 0x8000, 'throttle': 1.0},
            {'port': 1, 'buttons': 0x4000, 'brake': 1.0},
            {'port': 2, 'buttons': 0x8000, 'stick_x': -40, 'throttle': 0.5},
            {'port': 3, 'buttons': 0x8000, 'stick_x': 40, 'throttle': 0.5}],
            'capture': 'four-local-eight-racers'}])
        self.checkpoint('four-local-gp')


class LobbyClientTests(unittest.TestCase):
    def test_semantic_row(self):
        row = {'label': 'CUP', 'enabled': True, 'visible': True, 'id': 3, 'index': 1}
        self.assertIs(matching_row({'rows': [row]}, 'CUP'), row)

    def test_missing_duplicate_and_disabled_rows(self):
        row = {'label': 'CUP', 'enabled': True}
        for rows in ([], [row, row], [dict(row, enabled=False)]):
            with self.subTest(rows=rows), self.assertRaises(AssertionError):
                matching_row({'rows': rows}, 'CUP')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true')
    parser.add_argument('--race', action='store_true', help='Also load and drive a four-local GP')
    parser.add_argument('--backend', choices=('OpenGL', 'DirectX11'), default='OpenGL')
    parser.add_argument('--binary', type=Path, default=ROOT / 'build/x64/Debug/Spaghettify.exe')
    parser.add_argument('--root', type=Path)
    args = parser.parse_args()
    if not args.run:
        result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(LobbyClientTests))
        raise SystemExit(not result.wasSuccessful())
    root = (args.root or ROOT / 'build/lobby-game-test' / time.strftime('%Y%m%d-%H%M%S')).resolve()
    cvars = {'gSkipIntro': 3, 'gArcadeKart.DebugQuickBootHudRace': 0, 'gEnableDebugMode': 0}
    cvars.update({key: 0.0 for key in MUTE_CVARS})
    with patch.dict(os.environ):
        os.environ.pop('SK_AUDIO_PROBE', None)
        os.environ.pop('SK_NET_JOIN', None)
        process = launch(root, args.backend, cvars=cvars, executable=args.binary)
    session = LobbySession(root, process, pointer_ui=True)
    try:
        session.menus()
        if args.race:
            session.local_race()
        config = read(root / 'spaghettify.cfg.json')
        require(all(config['CVars'][key] == 0.0 for key in MUTE_CVARS), 'Muted test configuration')
        (root / 'verification.json').write_text(json.dumps({'status': 'passed', 'checkpoints': session.checkpoints}, indent=2))
        print(json.dumps({'status': 'passed', 'root': str(root)}))
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
