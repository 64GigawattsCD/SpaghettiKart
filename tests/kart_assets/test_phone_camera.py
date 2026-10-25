"""Muted native look-axis/rear-view tests; View multitouch checks run separately."""
import argparse
import json
import math
import time
from android_client import Phone, ROOT, FILES, TEST
from PIL import Image


def direction(state):
    delta=[a-b for a,b in zip(state['look_at'],state['camera'])]
    length=math.sqrt(sum(x*x for x in delta))
    return [x/length for x in delta]


def forward_alignment(state):
    d=direction(state)
    f=state['kart_forward']
    return (d[0]*f[0]+d[2]*f[2])/math.hypot(d[0],d[2])/math.hypot(f[0],f[2])


def camera_steps():
    return [{'frames':40,'view':1,'transmission':1,'capture':'forward'},
            {'frames':35,'look_x':55,'capture':'look-right'},
            {'frames':35,'look_x':-55,'capture':'look-left'},
            {'frames':35,'look_y':80,'capture':'look-up'},
            {'frames':35,'look_y':-80,'capture':'look-down'},
            {'frames':35,'look_x':55,'buttons':2,'capture':'rear'},
            {'frames':35,'look_x':55,'capture':'restore-look'},
            {'frames':40,'capture':'recenter'},
            {'frames':50,'look_x':40,'buttons':0x8000,'capture':'drive-look'},
            {'frames':40,'capture':'released'}]


def verify_camera(result):
    states=[event['end'] for event in result['events']]
    assert forward_alignment(states[0])>.99
    assert forward_alignment(states[5])<-.99, states[5]
    assert forward_alignment(states[7])>.99 and forward_alignment(states[9])>.99
    r,l=direction(states[1]),direction(states[2])
    assert r[0]*l[0]+r[2]*l[2]<-.7, (r,l)
    assert direction(states[3])[1]>.3 and direction(states[4])[1]<-.3
    assert sum(a*b for a,b in zip(direction(states[1]),direction(states[6])))>.999
    assert all(s['stick_x']==0 for s in states)
    assert math.dist(states[8]['position'],states[7]['position'])>1
    return forward_alignment(states[5])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial')
    parser.add_argument('--desktop',action='store_true',help='Exercise the shared native camera without a phone')
    args=parser.parse_args()
    if args.desktop:
        from game_client import launch, run
        from test_game import wait_for
        output=ROOT/'build/kart-game-test'/('camera-'+time.strftime('%Y%m%d-%H%M%S'))
        process=launch(output,'OpenGL')
        try:
            wait_for(output,lambda r:r['state']['race_state']==3,timeout=90)
            result=run(output,{'id':'camera','steps':camera_steps()})
            alignment=verify_camera(result)
            (output/'verification.json').write_text(json.dumps({'native_camera_checks':True,'rear_alignment':alignment},indent=2))
            print(output)
        finally:
            process.terminate()
            process.wait(timeout=15)
        return
    phone=Phone(args.serial)
    output=ROOT/'build/android-phone'/('camera-'+time.strftime('%Y%m%d-%H%M%S'))
    output.mkdir(parents=True)
    phone.stop()
    original=json.loads(phone.shell('cat',FILES+'/spaghettify.cfg.json').stdout)
    phone.shell('rm','-f',TEST+'/state.json',TEST+'/command.json',TEST+'/touch-selftest.json')
    try:
        phone.launch(test=True)
        until=time.monotonic()+90
        while time.monotonic()<until:
            state=phone.state()
            if state and state['state']['race_state']==3: break
            time.sleep(.2)
        else: raise TimeoutError('Phone race not ready; check screen is unlocked')
        result=phone.run(output,camera_steps())
        alignment=verify_camera(result)
        touch=json.loads(phone.shell('cat',TEST+'/touch-selftest.json').stdout)
        assert all(touch.values()) and touch['look_steer_throttle'],touch
        phone.adb('pull',TEST+'/touch-layout.png',output/'touch-layout.png')
        phone.adb('pull',TEST+'/touch-camera.png',output/'touch-camera.png')
        for index,overlay,name in [(0,'touch-layout.png','controls'),(1,'touch-camera.png','look-right')]:
            frame=Image.open((output/result['captures'][index]['file']).with_suffix('.png')).convert('RGBA')
            Image.alpha_composite(frame,Image.open(output/overlay).convert('RGBA')).save(output/(name+'-composite.png'))
        unchanged=json.loads(phone.shell('cat',FILES+'/spaghettify.cfg.json').stdout)==original
        assert unchanged
        (output/'verification.json').write_text(json.dumps({'touch':touch,'native_camera_checks':True,
            'rear_alignment':alignment,'normal_settings_unchanged':unchanged},indent=2))
        print(output)
    finally: phone.stop()


if __name__=='__main__': main()
