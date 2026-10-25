"""Muted native two-process race test. Uses renderer captures and in-game pad queues."""
import json
import math
from pathlib import Path
import time
import uuid
from game_client import launch, read, run, ROOT


def wait_race(roots, processes, timeout=90):
    until = time.monotonic() + timeout
    while time.monotonic() < until:
        for p in processes:
            if p.poll() is not None:
                raise RuntimeError(f'Game exited: {p.returncode}')
        states = [read(r / 'state.json') for r in roots]
        if all(s and s['state']['network']['phase'] == 'Racing' and s['state']['race_state'] == 3 for s in states):
            return states
        time.sleep(.2)
    raise TimeoutError([s and s['state'].get('network') for s in states])


def distance(a, b):
    return math.dist(a, b)


def main():
    root = ROOT / 'build/network-game-test' / time.strftime('%Y%m%d-%H%M%S')
    roots = [root / 'host', root / 'client']
    processes = []
    port = 56000 + int(uuid.uuid4().hex[:4], 16) % 3000
    try:
        for i, folder in enumerate(roots):
            processes.append(launch(folder, 'OpenGL', cvars={
                'gNet.Mode': i+1, 'gNet.Address': '127.0.0.1', 'gNet.Port': port,
                'gNet.BindAddress': '127.0.0.1',
                'gNet.Character': i, 'gNet.AutoStart': 1, 'gSkipCourseTour': 1}))
        initial = wait_race(roots, processes)
        start = initial[0]['state']['network']['racers']
        for s in initial:
            assert s['state']['network']['viewports'] == 1, s
            assert s['state']['network']['racers'][s['state']['network']['participant']-1]['slot'] == 0, s
        client = run(roots[1], {'id':'remote-drive', 'steps':[
            {'frames':1,'capture':'grid'}, {'frames':120,'buttons':0x8000,'capture':'drive'},
            {'frames':45,'buttons':0x4000,'capture':'brake'}]})
        host = run(roots[0], {'id':'host-check', 'steps':[{'frames':3,'capture':'remote-kart'}]})
        hs, cs = host['state'], client['state']
        assert distance(cs['position'], cs['kart_position']) < 15, cs
        import numpy as np
        from PIL import Image
        im = np.array(Image.open(roots[1]/'remote-drive-drive-desktop.png')).astype(float)
        h, w, _ = im.shape
        nose = im[int(h*.70):int(h*.98), int(w*.40):int(w*.60)]
        target = np.array([28, 184, 66], dtype=float)
        target /= np.linalg.norm(target)
        length = np.linalg.norm(nose, axis=2)
        similarity = np.sum(nose*target, axis=2)/np.maximum(length, 1)
        assert np.sum((similarity>.997) & (length>50)) > 150, 'Moving client cockpit is not visible'
        assert all(r['display_rank'] in (0, 1) for r in hs['network']['racers']), hs
        assert all(r['display_rank'] in (0, 1) for r in cs['network']['racers']), cs
        racers = hs['network']['racers']
        assert distance(start[1]['position'], racers[1]['position']) > 100, (start, racers)
        assert distance(start[0]['position'], racers[0]['position']) < 30, (start, racers)
        assert hs['network']['input_ack'] > 100 and cs['network']['received_snapshots'] > 30, (hs, cs)
        assert distance(racers[1]['position'], cs['network']['racers'][1]['position']) < 50, (hs, cs)
        host_drive = run(roots[0], {'id':'host-drive', 'steps':[{'frames':90,'buttons':0x8000,'capture':'drive'}]})
        assert distance(start[0]['position'],host_drive['state']['network']['racers'][0]['position']) > 100
        processes[1].terminate(); processes[1].wait(timeout=15)
        until = time.monotonic()+10
        while time.monotonic()<until:
            s=read(roots[0]/'state.json')
            if s and s['state']['network']['phase']=='Disconnected': break
            time.sleep(.2)
        else: raise AssertionError('Disconnect did not stop the race')
        (root/'report.json').write_text(json.dumps({'passed':True,'host':host,'client':client},indent=2))
        print(root)
    finally:
        for p in processes:
            if p.poll() is None:
                p.terminate(); p.wait(timeout=15)


if __name__ == '__main__': main()
