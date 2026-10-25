"""Muted phone menu route through app-owned Android touches, SDL and native targets."""
import argparse
import json
import time
from android_client import Phone, ROOT, FILES, TEST, PACKAGE
from test_pointer_menus import MenuSession, image_relative, require


class PhoneMenus(MenuSession):
    def __init__(self, phone, root):
        super().__init__(root, None)
        self.phone = phone
        self.touches = []

    def wait(self, predicate, timeout=60):
        deadline = time.monotonic()+timeout
        last = None
        while time.monotonic()<deadline:
            last = self.phone.state() or last
            if last and predicate(last): return last
            time.sleep(.1)
        raise TimeoutError(f'Phone menu wait: {json.dumps(last)}')

    def plan(self, name, steps):
        report = self.phone.run(self.root, steps)
        (self.root/(name+'-report.json')).write_text(json.dumps(report,indent=2))
        return report

    def checkpoint(self, name):
        report = super().checkpoint(name)
        request_id = 'overlay-' + name
        local = self.root/'android-touch-command.json'
        local.write_text(json.dumps({'id': request_id, 'op': 'status'}))
        self.phone.upload(local, TEST+'/android-touch-command.json')
        deadline = time.monotonic()+10
        while time.monotonic() < deadline:
            raw = self.phone.shell('cat', TEST+'/android-touch-result.json', check=False)
            try: result = json.loads(raw.stdout)
            except json.JSONDecodeError: result = {}
            if result.get('id') == request_id: break
            time.sleep(.1)
        else: raise TimeoutError('No overlay status: '+name)
        (self.root/(request_id+'.json')).write_text(json.dumps(result, indent=2))
        status = result['overlay']
        require(result.get('window_copy_result') == 0, result)
        self.phone.adb('pull', TEST+'/'+result['window_capture'], self.root/('window-'+name+'.png'))
        self.phone.adb('pull', TEST+'/'+status['capture'], self.root/(request_id+'.png'))
        from PIL import Image
        with Image.open(self.root/(request_id+'.png')) as capture:
            drawn = capture.getchannel('A').getbbox() is not None
        gameplay = name in ('race', 'resume')
        compositor = self.phone.shell('dumpsys', 'SurfaceFlinger').stdout
        app_layers = [line.strip() for line in compositor.splitlines()
                      if 'Output Layer' in line and '('+PACKAGE+'/' in line]
        status['compositor_output_layers'] = app_layers
        (self.root/(request_id+'.json')).write_text(json.dumps(result, indent=2))
        require(status['gameplay_active'] == gameplay and status['shown'] == gameplay, status)
        require(drawn == gameplay, status)
        if gameplay:
            require(status['draw_count'] > 0 and status['width'] > 0 and status['height'] > 0, status)
            require(bool(app_layers), {'overlay_omitted_by_compositor': status})
        self.checkpoints[name]['android_overlay'] = status
        return report

    def cancellation_checks(self):
        # These run through the shared native bridge in test_pointer_menus.py.
        pass

    def stale_context_check(self):
        # Keep Android touches separate from the synthetic pointer source.
        self.click('transmission-back',812,0x4000)
        self.wait(lambda r:not r['state']['menu']['players'][0]['locked'])
        self.click('character-relock',201,2)

    def click(self, name, context, target_id, hold=2):
        point = self.point(context,target_id)
        before = self.wait(lambda r: True)['state']
        require(before['touch_mode'] == 0, before)
        x,y = image_relative(point,before['aspect_ratio'])
        local = self.root/'android-touch-command.json'
        local.write_text(json.dumps({'id':name,'x':x,'y':y,'hold_ms':max(120,hold*34),'gameplay':False}))
        self.phone.upload(local,TEST+'/android-touch-command.json')
        deadline = time.monotonic()+10
        while time.monotonic()<deadline:
            raw = self.phone.shell('cat',TEST+'/android-touch-result.json',check=False)
            try: result = json.loads(raw.stdout)
            except json.JSONDecodeError: result = {}
            if result.get('id') == name: break
            time.sleep(.1)
        else: raise TimeoutError('No app-owned touch report: '+name)
        (self.root/(name+'-touch.json')).write_text(json.dumps(result,indent=2))
        require(result.get('dispatched') and not result.get('error'),result)
        require(result['sdl_surface_events'] == [0,1],result)
        require(result['overlay_before'] == 4 and result['touch_pad_after'] == 0,result)
        self.touches.append(result)
        after = self.wait(lambda r:r['state']['tick'] >= before['tick']+hold+8)['state']
        return {'events':[{'end':before},{'end':after}],'state':after}

    def route(self):
        evidence = super().route()
        evidence['route_input'] = 'Android Activity touchscreen -> SDL mouse -> native menu'
        evidence['checks'] = ['android-touch-menu-route','touch-back-cancel','resume-movement','overlay-mode-transitions']
        evidence['android_touch_dispatch'] = self.touches
        evidence['gameplay_overlay_visible'] = self.phone.state()['state']['touch_mode'] == 1
        require(evidence['gameplay_overlay_visible'],evidence)
        return evidence


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--serial',required=True)
    args=parser.parse_args()
    phone=Phone(args.serial)
    output=ROOT/'build/android-phone'/('menus-'+time.strftime('%Y%m%d-%H%M%S'))
    output.mkdir(parents=True)
    phone.stop()
    original=json.loads(phone.shell('cat',FILES+'/spaghettify.cfg.json').stdout)
    marker=output/'menu-boot'
    marker.write_text('test menus\n')
    phone.shell('mkdir','-p',TEST)
    phone.upload(marker,TEST+'/menu-boot')
    phone.shell('rm','-f',TEST+'/state.json',TEST+'/command.json',TEST+'/android-touch-command.json',TEST+'/android-touch-result.json')
    try:
        phone.shell('am','start','-W','-f','0x10008000','-n',PACKAGE+'/.MainActivity',
                    '--ez','kart_test','true','--ez','touch_dispatch_test','true',
                    '--ez','controller_test','false','--ez','audio_test','false')
        evidence=PhoneMenus(phone,output).route()
        evidence['normal_settings_unchanged']=json.loads(phone.shell('cat',FILES+'/spaghettify.cfg.json').stdout)==original
        require(evidence['normal_settings_unchanged'],evidence)
        (output/'verification.json').write_text(json.dumps(evidence,indent=2))
        print(output)
    except Exception as error:
        (output/'failure.json').write_text(json.dumps({'error':str(error),'last':phone.state()},indent=2))
        raise
    finally:
        phone.stop()
        phone.shell('rm','-f',TEST+'/menu-boot',TEST+'/android-touch-command.json')


if __name__=='__main__': main()
