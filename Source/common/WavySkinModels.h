#pragma once
// The fitted-model parts the newer Wavy editors draw from (written by tools/fit_*_parts.py into each plugin's
// *SkinData.h; WVox Chain's first; WVox Rider keeps its own copy of the same definitions): low-rank parts, round
// (polar) parts and panels, and their renderers at a physical scale.
#include <juce_graphics/juce_graphics.h>
#include <cmath>
#include <vector>

namespace wavy::skin
{
// a low-rank part: channel c (r, g, b, a premultiplied, 0 .. 1) of pixel (x, y) = sum over j < k of
// u[(y k + j) 4 + c] v[(x k + j) 4 + c]; an overlay uses the same form for signed RGB differences to a panel (a = 0)
struct LowRank { int w, h, k; const float* u; const float* v; };
// a round part: per radius knot (step apart, linear between) a value and nh harmonics in angle (a from straight up,
// clockwise: c0 + sum ck cos k a + sk sin k a) per channel, round (cx, cy) in its own 1x image; a pointer (if any) as
// the difference to it, sampled every 1 px along it (straight up) from along0 and across it (to its right) from across0
struct Polar { int radii, nh; float step, cx, cy; const float* c; int along, across; float along0, across0; const float* pointer; };
// a panel: a colour per row (r, g, b, 0 .. 255) times a factor per column, times a correction over cells (centres from
// fx0, fy0, a cell apart, linear between), and the grain (rms grey) left over
struct Panel { int y0, x0, h, w; const float* rows; const float* cols; int fixW, fixH; float fx0, fy0, step; const float* fix; float grain; };

// premultiplied channels 0 .. 1 into a pixel
inline void setPremultiplied (juce::Image::BitmapData& bd, int x, int y, float r, float g, float b, float a)
{
    a = juce::jlimit (0.0f, 1.0f, a);
    if (a <= 1.0e-4f) return;
    bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, r / a), juce::jlimit (0.0f, 1.0f, g / a),
                                                         juce::jlimit (0.0f, 1.0f, b / a), a));
}

// a low-rank part's value (4 channels) at design position (x, y) inside it (pixel centres at i + 0.5; linear between,
// clamped at the edges)
inline void lowRankAt (const LowRank& m, float x, float y, float out[4])
{
    auto pos = [] (float f, int n, int& i, float& t)
    {
        f = juce::jlimit (0.0f, (float) (n - 1), f - 0.5f);
        i = juce::jmin ((int) f, juce::jmax (0, n - 2)); t = n > 1 ? f - (float) i : 0.0f;
    };
    int iy, ix; float ty, tx;
    pos (y, m.h, iy, ty); pos (x, m.w, ix, tx);
    const int k = m.k, iy1 = juce::jmin (iy + 1, m.h - 1), ix1 = juce::jmin (ix + 1, m.w - 1);
    for (int c = 0; c < 4; ++c)
    {
        float v = 0.0f;
        for (int j = 0; j < k; ++j)
        {
            const float u = m.u[(iy * k + j) * 4 + c] * (1.0f - ty) + m.u[(iy1 * k + j) * 4 + c] * ty;
            const float w = m.v[(ix * k + j) * 4 + c] * (1.0f - tx) + m.v[(ix1 * k + j) * 4 + c] * tx;
            v += u * w;
        }
        out[c] = v;
    }
}

// a low-rank part rendered at a physical scale
inline juce::Image renderLowRank (const LowRank& m, float s)
{
    const int w = juce::jmax (1, juce::roundToInt ((float) m.w * s)), h = juce::jmax (1, juce::roundToInt ((float) m.h * s));
    juce::Image img (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    float v[4];
    for (int py = 0; py < h; ++py)
        for (int px = 0; px < w; ++px)
        {
            lowRankAt (m, ((float) px + 0.5f) / s, ((float) py + 0.5f) / s, v);
            setPremultiplied (bd, px, py, v[0], v[1], v[2], v[3]);
        }
    return img;
}

// a polar part's value at (dx, dy) from its centre (design px), its pointer turned to angle (degrees, clockwise from up)
inline bool polarAt (const Polar& m, float dx, float dy, float angle, float rmax, float v[4])
{
    const float r = std::sqrt (dx * dx + dy * dy);
    if (r > rmax) return false;
    const int nb = 1 + 2 * m.nh;
    const float th = std::atan2 (dx, -dy);
    float basis[64];
    basis[0] = 1.0f;
    for (int k = 1; k <= m.nh; ++k) { basis[2 * k - 1] = std::cos ((float) k * th); basis[2 * k] = std::sin ((float) k * th); }
    const float fi = juce::jlimit (0.0f, (float) (m.radii - 1) - 1.0e-3f, r / m.step);
    const int i = (int) fi;
    const float t = fi - (float) i;
    // the harmonics fade out toward the very centre, where no pixel pins them (a pixel centred on the part's centre
    // would otherwise take the first knot's angle terms at an arbitrary angle)
    const float hw = juce::jmin (1.0f, r / m.step);
    for (int j = 1; j < nb; ++j) basis[j] *= hw;
    for (int ch = 0; ch < 4; ++ch)
    {
        float a = 0.0f, b = 0.0f;
        for (int j = 0; j < nb; ++j) { a += basis[j] * m.c[(i * nb + j) * 4 + ch]; b += basis[j] * m.c[((i + 1) * nb + j) * 4 + ch]; }
        v[ch] = a * (1.0f - t) + b * t;
    }
    if (m.pointer != nullptr)
    {
        const float pa = juce::degreesToRadians (angle), ux = std::sin (pa), uy = -std::cos (pa);
        const float al = dx * ux + dy * uy, ac = -dx * uy + dy * ux;
        const float fa = al - m.along0, fc = ac - m.across0;
        if (fa >= 0.0f && fa <= (float) (m.along - 1) && fc >= 0.0f && fc <= (float) (m.across - 1))
        {
            const int ia = juce::jmin ((int) fa, m.along - 2), ic = juce::jmin ((int) fc, m.across - 2);
            const float ta = fa - (float) ia, tc = fc - (float) ic;
            for (int ch = 0; ch < 4; ++ch)
            {
                auto d = [&] (int a, int c) { return m.pointer[(a * m.across + c) * 4 + ch]; };
                v[ch] += (d (ia, ic) * (1.0f - tc) + d (ia, ic + 1) * tc) * (1.0f - ta) + (d (ia + 1, ic) * (1.0f - tc) + d (ia + 1, ic + 1) * tc) * ta;
            }
        }
    }
    return true;
}

// a polar part rendered into a w x h (design px) image, its centre at (cx, cy) in that image
inline juce::Image renderPolar (const Polar& m, int w, int h, float cx, float cy, float s, float angle, float rmax)
{
    const int iw = juce::jmax (1, juce::roundToInt ((float) w * s)), ih = juce::jmax (1, juce::roundToInt ((float) h * s));
    juce::Image img (juce::Image::ARGB, iw, ih, true, juce::SoftwareImageType());
    juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
    float v[4];
    for (int py = 0; py < ih; ++py)
        for (int px = 0; px < iw; ++px)
            if (polarAt (m, ((float) px + 0.5f) / s - cx, ((float) py + 0.5f) / s - cy, angle, rmax, v))
                setPremultiplied (bd, px, py, v[0], v[1], v[2], v[3]);
    return img;
}

// a panel's colour at design (x, y) (r, g, b, 0 .. 255)
inline void panelAt (const Panel& p, float x, float y, float rgb[3])
{
    const int r = juce::jlimit (0, p.h - 1, (int) (y - (float) p.y0)), c = juce::jlimit (0, p.w - 1, (int) (x - (float) p.x0));
    const float fx = juce::jlimit (0.0f, (float) p.fixW - 1.001f, (x - p.fx0) / p.step);
    const float fy = juce::jlimit (0.0f, (float) p.fixH - 1.001f, (y - p.fy0) / p.step);
    const int i = (int) fx, j = (int) fy;
    const float tx = fx - (float) i, ty = fy - (float) j;
    auto F = [&] (int jj, int ii) { return p.fix[jj * p.fixW + ii]; };
    const float f = (F (j, i) * (1.0f - tx) + F (j, i + 1) * tx) * (1.0f - ty) + (F (j + 1, i) * (1.0f - tx) + F (j + 1, i + 1) * tx) * ty;
    for (int ch = 0; ch < 3; ++ch) rgb[ch] = p.rows[r * 3 + ch] * p.cols[c] * f;
}
}
