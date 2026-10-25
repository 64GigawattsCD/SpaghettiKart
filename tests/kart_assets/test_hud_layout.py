"""Muted renderer captures and per-player HUD geometry/pixel assertions."""
import json
import argparse
from pathlib import Path
import time

import numpy as np
from PIL import Image
from game_client import ROOT, launch, run
from test_game import wait_for


def verify(shot, path, players):
    pixels = np.asarray(Image.open(path)).astype(np.int16)
    height, width = pixels.shape[:2]
    hud = shot['state']['hud']
    assert len(hud) == players
    origin = 160 - 120 * width / height
    def crop(rect):
        x,y,w,h = rect
        return pixels[max(0,round(y*height/240)):min(height,round((y+h)*height/240)),
                      max(0,round((x-origin)*height/240)):min(width,round((x+w-origin)*height/240))]
    counts=[]
    for r in hud:
        view, col, lap, rpm, track = [r[k] for k in ('viewport','column','laps','rpm','map')]
        assert abs(lap[0]+lap[2]-col[0]-col[2]) < .01
        assert abs(rpm[0]+rpm[2]-col[0]-col[2]) < .01
        assert lap[1]+lap[3] < track[1] < track[1]+track[3] < rpm[1]
        assert abs(rpm[1]+rpm[3]-col[1]-col[3]) < .01
        for rect in (col,lap,rpm,track):
            assert rect[0] >= view[0]-.01 and rect[1] >= view[1]-.01
            assert rect[0]+rect[2] <= view[0]+view[2]+.01
            assert rect[1]+rect[3] <= view[1]+view[3]+.01
        digits, meter, minimap = crop(lap), crop(rpm), crop(track)
        yellow = (digits[:,:,0]>150)&(digits[:,:,1]>100)&(digits[:,:,2]<100)
        red = (meter[:,:,0]>150)&(meter[:,:,1]<80)&(meter[:,:,2]<80)
        purple = (meter[:,:,0]>120)&(meter[:,:,2]>170)&(meter[:,:,1]<160)&(meter[:,:,2]>meter[:,:,0])
        white = (minimap.min(axis=2)>160)
        sample={'lap_pixels':int(yellow.sum()),'needle_pixels':int(red.sum()),
                'gear_pixels':int(purple.sum()),'map_pixels':int(white.sum())}
        assert all(n>5 for n in sample.values()), sample
        counts.append(sample)
    return counts


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case',action='append')
    args=parser.parse_args()
    cases=[('classic','OpenGL',(960,720),0,0),
           ('wide','OpenGL',(1400,600),0,3),
           ('dx-wide','DirectX11',(1280,720),0,1),
           ('ultrawide','DirectX11',(1600,450),0,7),
           ('two-horizontal','OpenGL',(1280,720),1,0),
           ('two-vertical','OpenGL',(1280,720),2,0),
           ('three','OpenGL',(1280,720),3,0),
           ('four','DirectX11',(1280,720),4,0)]
    output=ROOT/'build/hud-game-test'/time.strftime('%Y%m%d-%H%M%S')
    evidence={}
    for name,backend,size,layout,track in cases:
        if args.case and name not in args.case:
            continue
        root=output/name
        process=launch(root,backend,size=size,cvars={'gKartTest.ScreenLayout':layout,'gKartTest.Track':track})
        try:
            wait_for(root,lambda r:r['state']['race_state']==3,timeout=90)
            report=run(root,{'id':'hud','steps':[{'frames':8,'capture':'idle'},
                      {'frames':60,'buttons':0x8000,'capture':'drive'},
                      {'frames':60,'buttons':0x4000,'capture':'brake'}]})
            players=[1,2,2,3,4][layout]
            checks=[verify(s,root/s['file'],players) for s in report['captures']]
            assert len(checks)==3
            evidence[name]={'root':str(root),'checks':checks}
            (output/'verification.json').write_text(json.dumps(evidence,indent=2))
            print(name,'passed',flush=True)
        finally:
            process.terminate()
            process.wait(timeout=15)
    print(output)


if __name__=='__main__':
    main()
