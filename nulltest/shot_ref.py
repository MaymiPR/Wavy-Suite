"""Private: open a reference plugin's editor and screenshot its window. python -I shot_ref.py "<plugin name>" out.png"""
import sys, subprocess, time, ctypes, ctypes.wintypes as W
from PIL import ImageGrab
name, out = sys.argv[1], sys.argv[2]
if len(sys.argv) > 3 and sys.argv[3] == '--child':
    import pedalboard as pb
    p = pb.load_plugin(r"C:\Program Files\Common Files\VST3\WaveShell1-VST3 17.1_x64.vst3", plugin_name=name)
    p.show_editor()
    sys.exit(0)
child = subprocess.Popen([sys.executable, '-I', __file__, name, out, '--child'])
user32 = ctypes.windll.user32
found = []
def cb(h, _):
    n = user32.GetWindowTextLengthW(h)
    if n and user32.IsWindowVisible(h):
        buf = ctypes.create_unicode_buffer(n + 1); user32.GetWindowTextW(h, buf, n + 1)
        pid = W.DWORD(); user32.GetWindowThreadProcessId(h, ctypes.byref(pid))
        if pid.value == child.pid: found.append((h, buf.value))
    return True
for _ in range(40):
    time.sleep(0.5); found.clear()
    user32.EnumWindows(ctypes.WINFUNCTYPE(ctypes.c_bool, W.HWND, W.LPARAM)(cb), 0)
    if found: break
time.sleep(2.5)
if found:
    h = found[0][0]
    user32.SetForegroundWindow(h); time.sleep(0.7)
    r = W.RECT(); user32.GetClientRect(h, ctypes.byref(r))
    pt = W.POINT(0, 0); user32.ClientToScreen(h, ctypes.byref(pt))
    img = ImageGrab.grab(bbox=(pt.x, pt.y, pt.x + r.right, pt.y + r.bottom), all_screens=True)
    img.save(out); print('saved', out, img.size, found[0][1])
else:
    print('no window found')
child.kill()
