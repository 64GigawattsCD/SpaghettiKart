"""APK deployment and native, muted phone tests over ADB. Never injects device input."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
PACKAGE = 'dev.spaghettikart.phone'
FILES = f'/sdcard/Android/data/{PACKAGE}/files'
TEST = FILES + '/kart-test'


class Phone:
    def __init__(self, serial=None):
        devices = subprocess.check_output(['adb', 'devices'], text=True).splitlines()[1:]
        ready = [line.split()[0] for line in devices if len(line.split()) >= 2 and line.split()[1] == 'device']
        if serial is None:
            if len(ready) != 1:
                raise RuntimeError(f'Expected one connected, authorized phone; found {ready}. Use --serial.')
            serial = ready[0]
        if serial not in ready:
            raise RuntimeError(f'Device {serial} is not connected and authorized')
        self.serial = serial

    def adb(self, *args, check=True):
        return subprocess.run(['adb', '-s', self.serial, *map(str, args)],
                              capture_output=True, text=True, check=check)

    def shell(self, *args, check=True):
        return self.adb('shell', *args, check=check)

    def upload(self, source, target):
        self.adb('push', source, target + '.pending')
        self.shell('mv', target + '.pending', target)

    def state(self):
        data = self.shell('cat', TEST + '/state.json', check=False)
        try:
            return json.loads(data.stdout)
        except json.JSONDecodeError:
            return None

    def run(self, output, steps, timeout=180):
        command = {'id': 'phone-' + uuid.uuid4().hex[:10], 'steps': steps}
        local = output / 'command.json'
        local.write_text(json.dumps(command))
        self.shell('touch', TEST + '/heartbeat')
        self.upload(local, TEST + '/command.json')
        until = time.monotonic() + timeout
        while time.monotonic() < until:
            self.shell('touch', TEST + '/heartbeat')
            data = self.state()
            if data and data['id'] == command['id'] and data['status'] not in ('idle', 'running'):
                (output / (command['id'] + '.json')).write_text(json.dumps(data, indent=2))
                if data['status'] != 'complete' or data['error']:
                    raise RuntimeError(data)
                from PIL import Image
                for capture in data['captures']:
                    name = capture['file']
                    if Path(name).name != name:
                        raise RuntimeError('Unexpected capture path')
                    self.adb('pull', TEST + '/' + name, output / name)
                    with Image.open(output / name) as image:
                        image.save((output / name).with_suffix('.png'))
                return data
            time.sleep(.2)
        raise TimeoutError('Phone test timed out; check that the screen is unlocked')

    def deploy(self, apk, assets, output):
        abi = self.shell('getprop', 'ro.product.cpu.abilist').stdout.strip()
        if 'arm64-v8a' not in abi:
            raise RuntimeError(f'This APK requires arm64-v8a, phone reports {abi}')
        for file in (apk, assets / 'mk64.o2r', assets / 'spaghetti.o2r'):
            if not file.is_file():
                raise FileNotFoundError(file)
        self.adb('install', '-r', apk)
        self.shell('mkdir', '-p', FILES)
        for name in ('mk64.o2r', 'spaghetti.o2r'):
            self.upload(assets / name, FILES + '/' + name)
        receipt = {'serial': self.serial, 'model': self.shell('getprop', 'ro.product.model').stdout.strip(),
                   'apk': str(apk), 'package': PACKAGE, 'vr': False, 'timestamp': time.time()}
        (output / 'deployment.json').write_text(json.dumps(receipt, indent=2))
        return receipt

    def stop(self):
        self.shell('am', 'force-stop', PACKAGE)
        until = time.monotonic() + 10
        while self.shell('pidof', PACKAGE, check=False).stdout.strip():
            if time.monotonic() >= until:
                raise TimeoutError('Test app is still running; settings left untouched')
            time.sleep(.1)

    def launch(self, test=False, controller=False, audio=False, net_join=None):
        self.stop()
        # A fresh task and explicit false flags keep singleTask recents from retaining test extras.
        args = ['am', 'start', '-W', '-f', '0x10008000', '-n', PACKAGE + '/.MainActivity',
                '--ez', 'kart_test', str(test).lower(), '--ez', 'controller_test', str(controller).lower(),
                '--ez', 'audio_test', str(audio).lower(), '--ez', 'touch_dispatch_test', 'false']
        if net_join:
            import ipaddress
            args += ['--es', 'net_join', str(ipaddress.IPv4Address(net_join))]
        self.shell(*args)

    def audio_test(self, output):
        config = FILES + '/spaghettify.cfg.json'
        saved = self.shell('cat', config)
        backup = output / 'settings-before.json'
        backup.write_text(saved.stdout)
        self.stop()
        self.shell('rm', '-f', TEST + '/command.json', TEST + '/state.json',
                   TEST + '/audio-report.json', TEST + '/audio.wav')
        try:
            self.launch(test=True, audio=True)
            until = time.monotonic() + 90
            while time.monotonic() < until:
                data = self.state()
                if data and data['state']['race_state'] == 3:
                    break
                time.sleep(.5)
            else:
                raise TimeoutError('No active race; check phone is unlocked')
            self.run(output, [{'frames': 600, 'buttons': 0x8000},
                              {'frames': 2, 'capture': 'audio-drive'}])
            report = json.loads(self.shell('cat', TEST + '/audio-report.json').stdout)
            report['normal_settings_unchanged'] = json.loads(self.shell('cat', config).stdout) == json.loads(saved.stdout)
            self.adb('pull', TEST + '/audio.wav', output / 'audio.wav')
            (output / 'audio-report.json').write_text(json.dumps(report, indent=2))
            if not report['output_muted'] or not report['nonzero_samples'] or not report['normal_settings_unchanged']:
                raise AssertionError(report)
            if report['measured_ticks'] < 600 or report['empty_queue_ticks'] != 0:
                raise AssertionError({'audio_queue_regression': report})
            return report
        finally:
            self.stop()
            self.upload(backup, config)

    def controller_test(self, output):
        config = FILES + '/spaghettify.cfg.json'
        saved = self.shell('cat', config)
        backup = output / 'settings-before.json'
        backup.write_text(saved.stdout)
        self.shell('rm', '-f', TEST + '/command.json', TEST + '/state.json', TEST + '/controller-selftest.json')
        try:
            self.launch(test=True, controller=True)
            until = time.monotonic() + 120
            while time.monotonic() < until:
                raw = self.shell('cat', TEST + '/controller-selftest.json', check=False)
                if raw.returncode == 0:
                    report = json.loads(raw.stdout)
                    report['normal_settings_unchanged'] = json.loads(self.shell('cat', config).stdout) == json.loads(saved.stdout)
                    (output / 'controller-selftest.json').write_text(json.dumps(report, indent=2))
                    failures = [case['name'] for case in report['checks'] if not case['pass']]
                    if not report['normal_settings_unchanged']:
                        failures.append('normal-settings-isolation')
                    if report['error'] or failures or not report['checks']:
                        raise AssertionError({'error': report['error'], 'failed': failures})
                    return {'device': report['device'], 'passed': len(report['checks']), 'method': report['method'],
                            'normal_settings_unchanged': report['normal_settings_unchanged']}
                time.sleep(.5)
            raise TimeoutError('No controller report; check phone is unlocked and GameSir attached')
        finally:
            self.stop()
            self.upload(backup, config)

    def smoke(self, output):
        config = FILES + '/spaghettify.cfg.json'
        saved = self.shell('cat', config, check=False)
        backup = output / 'settings-before.json'
        if saved.returncode == 0:
            backup.write_text(saved.stdout)
        # Android must own the directory. The Activity creates it before native startup.
        self.shell('rm', '-f', TEST + '/command.json', TEST + '/state.json', TEST + '/touch-selftest.json')
        try:
            self.launch(test=True)
            until = time.monotonic() + 15
            while self.shell('test', '-d', TEST, check=False).returncode != 0:
                if time.monotonic() >= until:
                    raise TimeoutError('Activity did not initialize its test directory')
                time.sleep(.2)
            until = time.monotonic() + 90
            while time.monotonic() < until:
                self.shell('touch', TEST + '/heartbeat')
                data = self.state()
                if data and data['state']['race_state'] == 3:
                    break
                time.sleep(.5)
            else:
                raise TimeoutError('No active race; unlock phone and inspect app logs')
            touch = json.loads(self.shell('cat', TEST + '/touch-selftest.json').stdout)
            (output / 'touch-selftest.json').write_text(json.dumps(touch, indent=2))
            if not touch or not all(touch.values()):
                raise AssertionError(touch)
            palettes = self.run(output, [{'frames': 4, 'character': i, 'view': 1,
                                         'capture': f'paint-{i}'} for i in range(8)])
            result = self.run(output, [
                {'frames': 2, 'capture': 'before'},
                {'frames': 90, 'buttons': 0x8000, 'capture': 'driving'},
                {'frames': 2, 'view': 0, 'capture': 'third-person'},
                {'frames': 2, 'view': 1, 'capture': 'cockpit'}])
            start, end = result['events'][1]['start'], result['events'][1]['end']
            distance = math.dist(start['position'], end['position'])
            direction = [b-a for a,b in zip(end['camera'],end['look_at'])]
            velocity = end['velocity']
            alignment = sum(a*b for a,b in zip(direction,velocity)) / (
                math.sqrt(sum(x*x for x in direction))*math.sqrt(sum(x*x for x in velocity)))
            checks = {'touch_input': all(touch.values()), 'non_vr': not end['vr_active'], 'movement': distance > 100,
                      'camera_alignment': alignment > .99,
                      'eight_colors_captured': len(palettes['captures']) == 8,
                      'third_person': not result['events'][2]['end']['first_person'],
                      'first_person': result['events'][3]['end']['first_person'],
                      'input_released': result['state']['button'] == 0}
            from PIL import Image, ImageStat, ImageChops
            frames = [Image.open((output / c['file']).with_suffix('.png')).convert('RGB')
                      for c in palettes['captures']]
            checks['nonblank'] = all(max(ImageStat.Stat(im).stddev) > 15 for im in frames)
            checks['paint_changes'] = all(ImageChops.difference(frames[0], im).getbbox() is not None for im in frames[1:])
            import numpy as np
            colors = [(216,24,28),(28,184,66),(48,201,157),(42,107,235),
                      (190,113,40),(247,211,32),(245,113,176),(239,130,32)]
            paint_counts = []
            for im, color in zip(frames, colors):
                w,h = im.size
                # Widescreen projection places the nose lower than desktop 4:3.
                nose = np.array(im)[int(h*.70):int(h*.98),int(w*.40):int(w*.60)].astype(float)
                target = np.array(color,dtype=float)
                target /= np.linalg.norm(target)
                length = np.linalg.norm(nose,axis=2)
                similarity = np.sum(nose*target,axis=2)/np.maximum(length,1)
                paint_counts.append(int(np.sum((similarity>.997)&(length>50))))
            checks['character_paint_hues'] = all(count>150 for count in paint_counts)
            self.adb('pull', TEST + '/touch-layout.png', output / 'touch-layout.png')
            with Image.open(output / 'touch-layout.png') as overlay:
                scene = Image.open((output / result['captures'][-1]['file']).with_suffix('.png')).convert('RGBA')
                checks['touch_layout_dimensions'] = scene.size == overlay.size
                if checks['touch_layout_dimensions']:
                    Image.alpha_composite(scene, overlay.convert('RGBA')).save(output / 'phone-cockpit-with-controls.png')
            report = {'checks': checks, 'distance': distance, 'alignment': alignment,
                      'paint_pixels': paint_counts}
            if saved.returncode == 0:
                checks['normal_settings_unchanged'] = json.loads(self.shell('cat', config).stdout) == json.loads(saved.stdout)
            (output / 'verification.json').write_text(json.dumps(report, indent=2))
            if not all(checks.values()):
                raise AssertionError(report)
            return report
        finally:
            self.stop()
            if backup.exists():
                self.upload(backup, config)
            elif saved.returncode != 0:
                self.shell('rm', '-f', config)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('op', choices=['deploy', 'test', 'controller', 'audio', 'launch', 'state'])
    parser.add_argument('--serial')
    parser.add_argument('--apk', type=Path, default=ROOT / 'android/app/build/outputs/apk/release/app-release.apk')
    parser.add_argument('--assets', type=Path, default=ROOT / 'build/x64/Debug')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/android-phone' / time.strftime('%Y%m%d-%H%M%S'))
    args = parser.parse_args()
    phone = Phone(args.serial)
    args.output.mkdir(parents=True, exist_ok=True)
    if args.op == 'deploy':
        print(json.dumps(phone.deploy(args.apk, args.assets, args.output), indent=2))
    elif args.op == 'test':
        print(json.dumps(phone.smoke(args.output), indent=2))
    elif args.op == 'controller':
        print(json.dumps(phone.controller_test(args.output), indent=2))
    elif args.op == 'audio':
        print(json.dumps(phone.audio_test(args.output), indent=2))
    elif args.op == 'state':
        print(json.dumps(phone.state(), indent=2))
    else:
        phone.launch()


if __name__ == '__main__':
    main()
