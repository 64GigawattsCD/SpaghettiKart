"""Muted native manual-transmission checks and app-owned gate gesture tests."""
import argparse
import json
from pathlib import Path
import time
from PIL import Image
from android_client import Phone, ROOT, FILES, TEST


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial')
    phone = Phone(parser.parse_args().serial)
    output = ROOT/'build/android-phone'/('shifter-'+time.strftime('%Y%m%d-%H%M%S'))
    output.mkdir(parents=True)
    phone.stop()
    original = json.loads(phone.shell('cat', FILES+'/spaghettify.cfg.json').stdout)
    phone.shell('rm','-f',TEST+'/state.json',TEST+'/command.json',TEST+'/touch-shifter.png')
    try:
        phone.launch(test=True)
        until=time.monotonic()+90
        while time.monotonic()<until:
            s=phone.state()
            if s and s['state']['race_state']==3: break
            time.sleep(.2)
        else: raise TimeoutError('Phone is not in a race; check it is unlocked')
        phone.run(output,[{'frames':35,'transmission':0,'phone_layout':0}])
        touch=json.loads(phone.shell('cat',TEST+'/touch-selftest.json').stdout)
        assert 'shifter_third' in touch and all(touch.values()), touch
        steps=[]
        for gear in (1,0,2,0,3,0,4,0,5,0,6,0,-1,0,1):
            button=0x800000 if gear==-1 else 0x1000000 if gear==0 else 1<<(16+gear)
            steps += [{'frames':3,'buttons':button|0x10000}, {'frames':3,'buttons':button}]
        result=phone.run(output,steps)
        expected=(1,0,2,0,3,0,4,0,5,0,6,0,-1,0,1)
        for i,gear in enumerate(expected):
            held=result['events'][2*i]['end']; released=result['events'][2*i+1]['end']
            assert held['gear']==gear and held['clutch']>.9, held
            assert released['gear']==gear and released['clutch']==0, released
        drive=phone.run(output,[{'frames':60,'buttons':0x8000,'capture':'manual-driving'},
                                {'frames':30,'buttons':0x4000}, {'frames':5,'capture':'manual-gate'}])
        phone.adb('pull',TEST+'/touch-shifter.png',output/'touch-shifter.png')
        for side in ('left','right'):
            phone.adb('pull',TEST+'/touch-steer-'+side+'.png',output/('touch-steer-'+side+'.png'))
        frame=Image.open((output/drive['captures'][-1]['file']).with_suffix('.png')).convert('RGBA')
        overlay=Image.open(output/'touch-shifter.png').convert('RGBA')
        assert frame.size==overlay.size
        Image.alpha_composite(frame,overlay).save(output/'manual-gate-composite.png')
        for side in ('left','right'):
            steering=Image.open(output/('touch-steer-'+side+'.png')).convert('RGBA')
            Image.alpha_composite(frame,steering).save(output/('steering-'+side+'-composite.png'))
        hud=drive['captures'][-1]['state']['hud'][0]
        assert 88 < hud['map'][3] < 89, hud
        assert hud['laps'][1]+hud['laps'][3] < hud['map'][1]
        assert hud['map'][1]+hud['map'][3] < hud['rpm'][1]
        automatic=phone.run(output,[{'frames':35,'transmission':1,'capture':'automatic'}])
        assert automatic['state']['automatic']
        phone.adb('pull',TEST+'/touch-layout.png',output/'touch-automatic.png')
        unchanged=json.loads(phone.shell('cat',FILES+'/spaghettify.cfg.json').stdout)==original
        assert unchanged
        (output/'verification.json').write_text(json.dumps({'touch_checks':touch,
            'detents_verified':list(expected),'normal_settings_unchanged':unchanged,
            'minimap_height':hud['map'][3],'minimap_clear_of_corners':True},indent=2))
        print(output)
    finally: phone.stop()


if __name__=='__main__': main()
