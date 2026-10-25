"""Validate Android-rendered touch art and composite it onto an existing game frame."""
import argparse
import json
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('captures', type=Path)
    parser.add_argument('--game-frame', type=Path)
    args = parser.parse_args()
    report = json.loads((args.captures/'art-report.json').read_text())
    assert report['passed'], report
    assert len(report['cases']) == 20
    for case in report['cases']:
        assert all(case['checks'].values()), case
        assert case['pressed_pixels'] >= 100, case
        if '-manual-' in case['case']:
            assert case['checks']['clutch_art'], case
            assert (args.captures/(case['case']+'-clutch.png')).is_file(), case
        path = args.captures/(case['case']+'.png')
        with Image.open(path) as image:
            width, height = image.size
            alpha = image.getchannel('A')
            assert alpha.getextrema() == (0,255), path
            # No art is cropped by any edge of the viewport.
            x0,y0,x1,y1 = alpha.getbbox()
            assert x0>0 and y0>0 and x1<width and y1<height, path
    resources = ROOT/'android/app/src/main/res/drawable-nodpi'
    for path in resources.glob('touch_*.png'):
        with Image.open(path) as image:
            assert image.size == (128,128) and image.mode == 'RGBA', path
            assert image.getchannel('A').getextrema() == (0,255), path
            if 'rim' in path.name:
                assert image.getpixel((64,64))[3] == 0, path
    if args.game_frame:
        frame = Image.open(args.game_frame).convert('RGBA')
        for mode in ('manual','auto'):
            overlay = Image.open(args.captures/f'{frame.width}x{frame.height}-{mode}-0.png').convert('RGBA')
            assert overlay.size == frame.size
            Image.alpha_composite(frame,overlay).save(args.captures/f'{mode}-art-preview.png')
    print('20 Android View cases, pressed/idle art, transparent centers and viewport edges passed.')


if __name__ == '__main__':
    main()
