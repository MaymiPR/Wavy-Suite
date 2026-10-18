"""Read / modify the Waves parameter array inside a WaveShell VST3 state (as pedalboard's raw_state returns it).
The IComponent part is a JUCE MemoryBlock::toBase64Encoding string ("<size>.<chars>")."""
import re
_ALPHA = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
_IDX = {c: i for i, c in enumerate(_ALPHA)}

def juce_b64_decode(s):
    size, data = s.split('.', 1)
    size = int(size)
    out = bytearray(size)
    pos = 0
    for c in data:
        v = _IDX[c]
        for b in range(6):
            if (v >> b) & 1:
                byte, bit = divmod(pos + b, 8)
                if byte < size: out[byte] |= 1 << bit
        pos += 6
    return bytes(out)

def juce_b64_encode(b):
    n = len(b); bits = n * 8
    chars = []
    for pos in range(0, bits, 6):
        v = 0
        for k in range(6):
            p = pos + k
            if p < bits and (b[p // 8] >> (p % 8)) & 1: v |= 1 << k
        chars.append(_ALPHA[v])
    return f"{n}." + ''.join(chars)

def _split(raw):
    txt = raw.decode('latin-1')
    m = re.search(r'<IComponent>([^<]*)</IComponent>', txt)
    return txt, m

def get_inner(raw):
    txt, m = _split(raw)
    return juce_b64_decode(m.group(1))

def set_inner(raw, inner):
    txt, m = _split(raw)
    new = (txt[:m.start(1)] + juce_b64_encode(inner) + txt[m.end(1):]).encode('latin-1')
    # outer header: 'VC2!' + uint32 length; keep the original's relation to the body length
    if raw[:4] == b'VC2!':
        import struct
        old_len = struct.unpack('<I', raw[4:8])[0]
        new = b'VC2!' + struct.pack('<I', old_len + len(new) - len(raw)) + new[8:]
    return new

def get_values(raw):
    inner = get_inner(raw).decode('latin-1')
    m = re.search(r'<Parameters Type="RealWorld">([^<]*)</Parameters>', inner)
    return m.group(1).split() if m else None

def set_values(raw, values):
    """Write the RealWorld parameter array into every setup (A and B) of the state."""
    inner = get_inner(raw).decode('latin-1')
    text = ' '.join(str(v) for v in values) + ' '
    inner = re.sub(r'(<Parameters Type="RealWorld">)([^<]*)(</Parameters>)', lambda m: m.group(1) + text + m.group(3), inner)
    return set_inner(raw, inner.encode('latin-1'))


def apply_values(plugin, values, base=None):
    """Write the parameter array into a loaded WaveShell plugin and verify by reading it back. The shell ignores some
    otherwise valid states (it depends on the payload size), so a few paddings of the text are tried."""
    base = base if base is not None else plugin.raw_state
    want = [str(v) for v in values]
    inner0 = get_inner(base).decode('latin-1')
    def variants(w):
        # same values written with different text lengths (trailing zeros), plus trailing spaces
        for zeros in range(4):
            yield [x if (x == '*' or zeros == 0) else ('%.*f' % (zeros, float(x))) for x in w]
    got = None
    for vals in variants(want):
        for pad in range(12):
            text = ' '.join(vals) + ' ' * (1 + pad)
            inner = re.sub(r'(<Parameters Type="RealWorld">)([^<]*)(</Parameters>)', lambda m: m.group(1) + text + m.group(3), inner0)
            plugin.raw_state = set_inner(base, inner.encode('latin-1'))
            got = get_values(plugin.raw_state)
            if all(w == '*' or abs(float(w) - float(g)) < 1e-6 for w, g in zip(want, got)):
                return pad
    raise RuntimeError('the plugin did not accept the state %s (read back %s)' % (want, got))
