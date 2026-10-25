"""Muted phone/PC LAN driving and both phone HUD layouts, through native queues."""
import argparse
import json
import math
from pathlib import Path
import time

from android_client import Phone, FILES, TEST
from game_client import launch, read, run, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial')
    parser.add_argument('--host', default='192.168.1.23')
    args = parser.parse_args()
    phone = Phone(args.serial)
    output = ROOT / 'build/network-phone-test' / time.strftime('%Y%m%d-%H%M%S')
    output.mkdir(parents=True)
    host_root, phone_root = output / 'host', output / 'phone'
    phone_root.mkdir()
    phone.stop()
    config = FILES + '/spaghettify.cfg.json'
    original = phone.shell('cat', config).stdout
    (output / 'settings-before.json').write_text(original)
    process = None
    try:
        phone.shell('rm', '-f', TEST+'/state.json', TEST+'/command.json',
                    TEST+'/touch-layout-0.png', TEST+'/touch-layout-1.png')
        process = launch(host_root, 'OpenGL', cvars={
            'gNet.Mode': 1, 'gNet.BindAddress': args.host, 'gNet.Port': 54777,
            'gNet.Character': 0, 'gNet.AutoStart': 1, 'gSkipCourseTour': 1})
        phone.launch(test=True, net_join=args.host)
        until = time.monotonic() + 90
        while time.monotonic() < until:
            hs, ps = read(host_root/'state.json'), phone.state()
            if process.poll() is not None:
                raise RuntimeError('PC host exited')
            if all(s and s['state']['network']['phase'] == 'Racing'
                   and s['state']['race_state'] == 3 for s in (hs, ps)):
                break
            time.sleep(.3)
        else:
            raise TimeoutError({'host': hs, 'phone': ps})
        start = hs['state']['network']['racers']
        client = phone.run(phone_root, [
            {'frames': 90, 'buttons': 0x8000, 'capture': 'phone-driving'},
            {'frames': 45, 'buttons': 0x4000, 'capture': 'phone-braking'}])
        host = run(host_root, {'id': 'remote-check', 'steps': [{'frames': 3, 'capture': 'phone-kart'}]})
        racers = host['state']['network']['racers']
        checks = {
            'phone_moves_remote_kart': math.dist(start[1]['position'], racers[1]['position']) > 100,
            'host_unmoved_by_phone': math.dist(start[0]['position'], racers[0]['position']) < 30,
            'snapshot_convergence': math.dist(racers[1]['position'], client['state']['position']) < 60,
            'cockpit_follows_phone': math.dist(client['state']['position'], client['state']['kart_position']) < 15,
            'one_viewport_each': all(s['state']['network']['viewports'] == 1 for s in (host, client)),
            'input_acknowledged': host['state']['network']['input_ack'] > 100,
            'phone_received_updates': client['state']['network']['received_snapshots'] > 30,
        }
        layouts = []
        from PIL import Image, ImageStat
        for mode in (0, 1):
            phone.run(phone_root, [{'frames': 30, 'phone_layout': mode}])
            result = phone.run(phone_root, [{'frames': 3, 'view': 1, 'capture': f'layout-{mode}'}])
            layouts.append(result['state']['hud'][0])
            overlay = phone_root / f'touch-layout-{mode}.png'
            phone.adb('pull', TEST+f'/touch-layout-{mode}.png', overlay)
            scene = Image.open((phone_root/result['captures'][0]['file']).with_suffix('.png')).convert('RGBA')
            controls = Image.open(overlay).convert('RGBA')
            checks[f'layout_{mode}_dimensions'] = scene.size == controls.size
            checks[f'layout_{mode}_nonblank'] = max(ImageStat.Stat(scene.convert('RGB')).stddev) > 15
            Image.alpha_composite(scene, controls).save(phone_root/f'layout-{mode}-composite.png')
            touch = json.loads(phone.shell('cat', TEST+'/touch-selftest.json').stdout)
            checks[f'layout_{mode}_touch'] = bool(touch) and all(touch.values())
            (phone_root/f'touch-selftest-{mode}.json').write_text(json.dumps(touch, indent=2))
        checks['hud_moves_inward_in_b'] = layouts[1]['column'][0] < layouts[0]['column'][0] - 50
        host_drive = run(host_root, {'id': 'host-drive', 'steps': [
            {'frames': 90, 'buttons': 0x8000, 'capture': 'pc-driving'},
            {'frames': 45, 'buttons': 0x4000}]})
        phone_after = phone.run(phone_root, [{'frames': 5, 'capture': 'pc-kart'}])
        checks['host_moves_own_kart'] = math.dist(start[0]['position'], host_drive['state']['position']) > 100
        checks['phone_sees_host'] = math.dist(host_drive['state']['position'],
            phone_after['state']['network']['racers'][0]['position']) < 60
        checks['normal_settings_unchanged'] = json.loads(phone.shell('cat', config).stdout) == json.loads(original)
        phone.stop()
        until = time.monotonic()+10
        while time.monotonic() < until:
            hs = read(host_root/'state.json')
            if hs and hs['state']['network']['phase'] == 'Disconnected':
                checks['disconnect_detected'] = True
                break
            time.sleep(.2)
        checks.setdefault('disconnect_detected', False)
        report = {'checks': checks, 'host': host, 'phone': client,
                  'phone_after_host_drive': phone_after, 'layouts': layouts}
        (output/'report.json').write_text(json.dumps(report, indent=2))
        if not all(checks.values()):
            raise AssertionError(checks)
        print(output)
        print(json.dumps(checks, indent=2))
    finally:
        phone.stop()
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=15)
        # The app's test configuration lives in its isolated test directory.
        # Preserve any user edit rather than copying our original settings over it.
        (output/'settings-after.json').write_text(phone.shell('cat', config).stdout)


if __name__ == '__main__':
    main()
