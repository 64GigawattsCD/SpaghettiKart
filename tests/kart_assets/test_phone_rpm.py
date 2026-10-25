"""Muted RPM transition capture through the native phone bridge."""
import argparse
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image
from android_client import Phone, FILES, TEST


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    phone = Phone(args.serial)
    config = FILES + '/spaghettify.cfg.json'
    saved = phone.shell('cat', config).stdout
    backup = args.output / 'settings-before.json'
    backup.write_text(saved)
    phone.stop()
    phone.shell('rm', '-f', TEST + '/command.json', TEST + '/state.json')
    try:
        phone.launch(test=True)
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            data = phone.state()
            if data and data['state']['race_state'] == 3:
                break
            time.sleep(.2)
        else:
            raise TimeoutError('No active race')
        steps = [{'frames': 6, 'buttons': button, 'capture': f'{phase}-{i:02}'}
                 for phase, button in [('accelerate', 0x8000), ('brake', 0x4000)]
                 for i in range(16)]
        result = phone.run(args.output, steps)
        checks = []
        for shot in result['captures']:
            path = args.output / shot['file']
            pixels = np.asarray(Image.open(path)).astype(np.int16)
            red = (pixels[:, :, 0] > 150) & (pixels[:, :, 0] > pixels[:, :, 1] * 2) & (pixels[:, :, 0] > pixels[:, :, 2] * 2)
            h, w = red.shape
            # The needle lives in the lower-right; detect a large red spill into the scene.
            scene = red[:int(h * .65), int(w * .25):int(w * .85)]
            checks.append({'file': path.with_suffix('.png').name, 'scene_red_fraction': float(scene.mean()),
                           'total_red_fraction': float(red.mean())})
        report = {'captures': checks, 'settings_unchanged': json.loads(phone.shell('cat', config).stdout) == json.loads(saved)}
        (args.output / 'rpm-report.json').write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2))
        assert report['settings_unchanged']
        assert len(checks) == len(steps), 'Missing RPM captures'
        assert all(c['scene_red_fraction'] < .05 for c in checks), 'RPM red spill'
        assert all(c['total_red_fraction'] < .05 for c in checks), 'RPM full-frame red spill'
    finally:
        phone.stop()
        phone.upload(backup, config)


if __name__ == '__main__':
    main()
