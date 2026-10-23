"""Film a Waves editor that tools/MeterCapture cannot (H-Comp's web-view editor paints white in a JUCE window): the
editor is opened by pedalboard's show_editor() on the main thread, a thread plays a signal through the plug-in in
real time, another grabs the editor window about 30 times a second. Private measurement tool; the frames stay in the
git-ignored build/.

    python -I capture_vu.py <out.npz> <in.f32 mono 48 kHz> [param=value ...]      (values as pedalboard takes them)

out.npz: t (s since the signal started, per frame), frames (n, h, w, 3) uint8, audio (the plug-in's output).
"""
import sys, os, time, threading, ctypes, ctypes.wintypes as W
import numpy as np
from PIL import ImageGrab
import pedalboard as pb

FS, BLOCK = 48000, 480
out, src = sys.argv[1], sys.argv[2]
p = pb.load_plugin(r"C:\Program Files\Common Files\VST3\WaveShell1-VST3 17.1_x64.vst3", plugin_name="H-Comp Stereo")
for a in sys.argv[3:]:
    k, v = a.split('=', 1)
    try: setattr(p, k, float(v))
    except (ValueError, TypeError): setattr(p, k, v)
x = np.fromfile(src, np.float32)
x2 = np.stack([x, x])
user32 = ctypes.windll.user32
pid = os.getpid()
state = dict(t0=None, done=False, frames=[], times=[], audio=[])


def find_window():
    found = []
    def cb(h, _):
        n = user32.GetWindowTextLengthW(h)
        if n and user32.IsWindowVisible(h):
            q = W.DWORD(); user32.GetWindowThreadProcessId(h, ctypes.byref(q))
            if q.value == pid: found.append(h)
        return True
    user32.EnumWindows(ctypes.WINFUNCTYPE(ctypes.c_bool, W.HWND, W.LPARAM)(cb), 0)
    return found[0] if found else None


def player():
    time.sleep(4.0)                                   # the editor's web view loads first
    t0 = time.perf_counter(); state['t0'] = t0
    for i in range(0, x2.shape[1] - BLOCK, BLOCK):
        wait = t0 + i / FS - time.perf_counter()
        if wait > 0: time.sleep(wait)
        state['audio'].append(p.process(x2[:, i:i + BLOCK], FS, buffer_size=BLOCK, reset=False))
    state['done'] = True


def printwindow(h):
    """the window's own rendering (PW_RENDERFULLCONTENT), which includes GPU-composited content even when the screen
    shows nothing"""
    gdi = ctypes.windll.gdi32
    r = W.RECT(); user32.GetClientRect(h, ctypes.byref(r)); w, hh = r.right, r.bottom
    hdc = user32.GetDC(h); mem = gdi.CreateCompatibleDC(hdc); bmp = gdi.CreateCompatibleBitmap(hdc, w, hh)
    gdi.SelectObject(mem, bmp)
    user32.PrintWindow(h, mem, 3)                     # PW_CLIENTONLY | PW_RENDERFULLCONTENT
    class BIH(ctypes.Structure):
        _fields_ = [('biSize', ctypes.c_uint32), ('biWidth', ctypes.c_int32), ('biHeight', ctypes.c_int32), ('biPlanes', ctypes.c_uint16),
                    ('biBitCount', ctypes.c_uint16), ('biCompression', ctypes.c_uint32), ('biSizeImage', ctypes.c_uint32), ('a', ctypes.c_int32),
                    ('b', ctypes.c_int32), ('c', ctypes.c_uint32), ('d', ctypes.c_uint32)]
    bi = BIH(); bi.biSize = ctypes.sizeof(BIH); bi.biWidth = w; bi.biHeight = -hh; bi.biPlanes = 1; bi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * hh * 4)
    gdi.GetDIBits(mem, bmp, 0, hh, buf, ctypes.byref(bi), 0)
    gdi.DeleteObject(bmp); gdi.DeleteDC(mem); user32.ReleaseDC(h, hdc)
    a = np.frombuffer(buf, np.uint8).reshape(hh, w, 4)
    return a[:, :, 2::-1].copy()


GRAB = os.environ.get('VU_GRAB', 'printwindow')


def grabber():
    h = None
    while h is None and not state['done']:
        time.sleep(0.2); h = find_window()
    while state['t0'] is None: time.sleep(0.01)
    while not state['done']:
        if GRAB == 'printwindow': frame = printwindow(h)
        else:
            r = W.RECT(); user32.GetClientRect(h, ctypes.byref(r))
            pt = W.POINT(0, 0); user32.ClientToScreen(h, ctypes.byref(pt))
            frame = np.asarray(ImageGrab.grab(bbox=(pt.x, pt.y, pt.x + r.right, pt.y + r.bottom), all_screens=True))[:, :, :3].copy()
        state['times'].append(time.perf_counter() - state['t0']); state['frames'].append(frame)
        time.sleep(0.02)
    np.savez(out, t=np.array(state['times']), frames=np.array(state['frames']), audio=np.concatenate(state['audio'], 1))
    print('saved', len(state['frames']), 'frames', flush=True)
    user32.PostMessageW(h, 0x0010, 0, 0)               # WM_CLOSE: ends show_editor()


threading.Thread(target=player, daemon=True).start()
threading.Thread(target=grabber, daemon=True).start()
p.reset()
p.show_editor()
time.sleep(1.0)
