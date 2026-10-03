#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>

// Grafica pixel-art: ogni schermo è un'immagine 224x168 "virtuale" che viene ingrandita
// senza interpolazione (nearest neighbour), quindi i pixel restano nitidi e quadrati.

namespace grv::px
{
constexpr int kW = 224, kH = 168;

namespace pal
{
inline const juce::Colour bg     { 0xff12121f };
inline const juce::Colour panel  { 0xff22263d };
inline const juce::Colour grid   { 0xff30365a };
inline const juce::Colour dim    { 0xff5a6488 };
inline const juce::Colour mid    { 0xff96a2c4 };
inline const juce::Colour light  { 0xffe9edf9 };
inline const juce::Colour red    { 0xffff4d5e };
inline const juce::Colour orange { 0xffff9d3c };
inline const juce::Colour cyan   { 0xff45e8f5 };
inline const juce::Colour purple { 0xffa77bff };
inline const juce::Colour green  { 0xff8cf06a };
inline const juce::Colour yellow { 0xffffe066 };

inline juce::Colour pedal (int i)
{
    static const juce::Colour c[4] = { red, orange, cyan, purple };
    return c[i & 3];
}
} // namespace pal

//==============================================================================
// Font 5x7 disegnato a mano (solo maiuscole, cifre e simboli utili)
class Font5x7
{
public:
    static const Font5x7& get()
    {
        static const Font5x7 f;
        return f;
    }

    const std::array<uint8_t, 7>& glyph (char c) const
    {
        if (c >= 'a' && c <= 'z')
            c = (char) (c - 'a' + 'A');
        return rows[(size_t) (unsigned char) c & 127];
    }

private:
    Font5x7()
    {
        for (auto& r : rows)
            r.fill (0);

        const auto lines = juce::StringArray::fromLines (juce::String (juce::CharPointer_UTF8 (data())));
        char current = 0;
        int row = 0;
        for (auto line : lines)
        {
            line = line.trimEnd();
            if (line.startsWith ("@") && line.length() >= 2)
            {
                current = (char) line[1];
                row = 0;
            }
            else if (current != 0 && row < 7 && line.length() == 5)
            {
                uint8_t bits = 0;
                for (int i = 0; i < 5; ++i)
                    if (line[i] == '#')
                        bits |= (uint8_t) (1 << (4 - i));
                rows[(size_t) current & 127][(size_t) row++] = bits;
            }
        }
    }

    static const char* data()
    {
        return R"FONT(
@A
.###.
#...#
#...#
#####
#...#
#...#
#...#
@B
####.
#...#
#...#
####.
#...#
#...#
####.
@C
.###.
#...#
#....
#....
#....
#...#
.###.
@D
###..
#..#.
#...#
#...#
#...#
#..#.
###..
@E
#####
#....
#....
####.
#....
#....
#####
@F
#####
#....
#....
####.
#....
#....
#....
@G
.###.
#...#
#....
#.###
#...#
#...#
.###.
@H
#...#
#...#
#...#
#####
#...#
#...#
#...#
@I
.###.
..#..
..#..
..#..
..#..
..#..
.###.
@J
..###
...#.
...#.
...#.
...#.
#..#.
.##..
@K
#...#
#..#.
#.#..
##...
#.#..
#..#.
#...#
@L
#....
#....
#....
#....
#....
#....
#####
@M
#...#
##.##
#.#.#
#.#.#
#...#
#...#
#...#
@N
#...#
##..#
#.#.#
#..##
#...#
#...#
#...#
@O
.###.
#...#
#...#
#...#
#...#
#...#
.###.
@P
####.
#...#
#...#
####.
#....
#....
#....
@Q
.###.
#...#
#...#
#...#
#.#.#
#..#.
.##.#
@R
####.
#...#
#...#
####.
#.#..
#..#.
#...#
@S
.####
#....
#....
.###.
....#
....#
####.
@T
#####
..#..
..#..
..#..
..#..
..#..
..#..
@U
#...#
#...#
#...#
#...#
#...#
#...#
.###.
@V
#...#
#...#
#...#
#...#
#...#
.#.#.
..#..
@W
#...#
#...#
#...#
#.#.#
#.#.#
##.##
#...#
@X
#...#
#...#
.#.#.
..#..
.#.#.
#...#
#...#
@Y
#...#
#...#
.#.#.
..#..
..#..
..#..
..#..
@Z
#####
....#
...#.
..#..
.#...
#....
#####
@0
.###.
#...#
#..##
#.#.#
##..#
#...#
.###.
@1
..#..
.##..
..#..
..#..
..#..
..#..
.###.
@2
.###.
#...#
....#
...#.
..#..
.#...
#####
@3
#####
...#.
..#..
...#.
....#
#...#
.###.
@4
...#.
..##.
.#.#.
#..#.
#####
...#.
...#.
@5
#####
#....
####.
....#
....#
#...#
.###.
@6
..##.
.#...
#....
####.
#...#
#...#
.###.
@7
#####
....#
...#.
..#..
.#...
.#...
.#...
@8
.###.
#...#
#...#
.###.
#...#
#...#
.###.
@9
.###.
#...#
#...#
.####
....#
...#.
.##..
@.
.....
.....
.....
.....
.....
.##..
.##..
@,
.....
.....
.....
.....
.##..
..#..
.#...
@:
.....
.##..
.##..
.....
.##..
.##..
.....
@-
.....
.....
.....
#####
.....
.....
.....
@+
.....
..#..
..#..
#####
..#..
..#..
.....
@/
....#
....#
...#.
..#..
.#...
#....
#....
@%
##..#
##..#
...#.
..#..
.#...
#..##
#..##
@(
...#.
..#..
.#...
.#...
.#...
..#..
...#.
@)
.#...
..#..
...#.
...#.
...#.
..#..
.#...
@=
.....
.....
#####
.....
#####
.....
.....
@'
..#..
..#..
.....
.....
.....
.....
.....
@!
..#..
..#..
..#..
..#..
..#..
.....
..#..
@?
.###.
#...#
....#
...#.
..#..
.....
..#..
@*
.....
#.#.#
.###.
#####
.###.
#.#.#
.....
@#
.#.#.
.#.#.
#####
.#.#.
#####
.#.#.
.#.#.
@_
.....
.....
.....
.....
.....
.....
#####
)FONT";
    }

    std::array<std::array<uint8_t, 7>, 128> rows;
};

//==============================================================================
class Canvas
{
public:
    explicit Canvas (juce::Image& img) : g (img) {}

    void clear (juce::Colour c) { g.fillAll (c); }

    void fill (int x, int y, int w, int h, juce::Colour c)
    {
        if (w > 0 && h > 0)
        {
            g.setColour (c);
            g.fillRect (x, y, w, h);
        }
    }
    void fill (juce::Rectangle<int> r, juce::Colour c) { fill (r.getX(), r.getY(), r.getWidth(), r.getHeight(), c); }
    void plot (int x, int y, juce::Colour c) { fill (x, y, 1, 1, c); }

    void frame (juce::Rectangle<int> r, juce::Colour c)
    {
        fill (r.getX(), r.getY(), r.getWidth(), 1, c);
        fill (r.getX(), r.getBottom() - 1, r.getWidth(), 1, c);
        fill (r.getX(), r.getY(), 1, r.getHeight(), c);
        fill (r.getRight() - 1, r.getY(), 1, r.getHeight(), c);
    }

    // rettangolo con angoli "mangiati" (look 8-bit)
    void panel (juce::Rectangle<int> r, juce::Colour body, juce::Colour border)
    {
        fill (r.getX() + 1, r.getY(), r.getWidth() - 2, r.getHeight(), border);
        fill (r.getX(), r.getY() + 1, r.getWidth(), r.getHeight() - 2, border);
        fill (r.getX() + 1, r.getY() + 1, r.getWidth() - 2, r.getHeight() - 2, body);
    }

    void line (int x0, int y0, int x1, int y1, juce::Colour c)
    {
        const int dx = std::abs (x1 - x0), dy = -std::abs (y1 - y0);
        const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;)
        {
            plot (x0, y0, c);
            if (x0 == x1 && y0 == y1)
                break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // riempimento a scacchiera
    void dither (int x, int y, int w, int h, juce::Colour c)
    {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x + ((yy + x) & 1 ? 0 : 0); xx < x + w; ++xx)
                if (((xx + yy) & 1) == 0)
                    plot (xx, yy, c);
    }

    void dottedH (int x, int y, int w, juce::Colour c)
    {
        for (int i = 0; i < w; i += 2)
            plot (x + i, y, c);
    }
    void dottedV (int x, int y, int h, juce::Colour c)
    {
        for (int i = 0; i < h; i += 2)
            plot (x, y + i, c);
    }

    static int textWidth (const juce::String& s, int scale = 1)
    {
        return s.isEmpty() ? 0 : (s.length() * 6 - 1) * scale;
    }

    void text (const juce::String& s, int x, int y, juce::Colour c, int scale = 1)
    {
        for (int i = 0; i < s.length(); ++i)
        {
            const auto& gl = Font5x7::get().glyph ((char) s[i]);
            for (int r = 0; r < 7; ++r)
                for (int b = 0; b < 5; ++b)
                    if (gl[(size_t) r] & (1 << (4 - b)))
                        fill (x + (i * 6 + b) * scale, y + r * scale, scale, scale, c);
        }
    }

    void textRight (const juce::String& s, int xRight, int y, juce::Colour c, int scale = 1)
    {
        text (s, xRight - textWidth (s, scale), y, c, scale);
    }

    void textCentre (const juce::String& s, int cx, int y, juce::Colour c, int scale = 1)
    {
        text (s, cx - textWidth (s, scale) / 2, y, c, scale);
    }

    // triangolo pieno: dir 0=sinistra 1=destra 2=su 3=giù; size = mezza altezza
    void triangle (int cx, int cy, int dir, int size, juce::Colour c)
    {
        for (int i = 0; i <= size; ++i)
        {
            const int len = i; // mezza altezza della colonna (la punta è a i = 0)
            switch (dir)
            {
                case 0: fill (cx - size + i, cy - len, 1, len * 2 + 1, c); break;
                case 1: fill (cx + size - i, cy - len, 1, len * 2 + 1, c); break;
                case 2: fill (cx - len, cy - size + i, len * 2 + 1, 1, c); break;
                default: fill (cx - len, cy + size - i, len * 2 + 1, 1, c); break;
            }
        }
    }

private:
    juce::Graphics g;
};

//==============================================================================
// Schermo pixel-art: le classi derivate disegnano in coordinate 224x168
class PixelScreen : public juce::Component, private juce::Timer
{
public:
    PixelScreen() : image (juce::Image::ARGB, kW, kH, true)
    {
        startTimerHz (30);
    }

    // 1 = acceso, 0 = spento; in mezzo i pixel si accendono (o spengono) uno a uno in ordine casuale
    void setPower (float p) { power = juce::jlimit (0.f, 1.f, p); repaint(); }
    float getPower() const { return power; }

    // disturbo "glitch" (0 = nessuno, 1 = massimo): fasce spostate, canali RGB separati, blocchi di rumore
    void setGlitch (float g) { glitch = juce::jlimit (0.f, 1.f, g); }
    void setRefreshRate (int hz) { startTimerHz (hz); }

    void paint (juce::Graphics& g) override
    {
        {
            Canvas c (image);
            draw (c);
        }
        if (glitch > 0.01f)
            applyGlitch();
        if (power < 1.f)
        {
            juce::Image::BitmapData data (image, juce::Image::BitmapData::readWrite);
            for (int y = 0; y < kH; ++y)
                for (int x = 0; x < kW; ++x)
                {
                    juce::uint32 h = (juce::uint32) x * 374761393u + (juce::uint32) y * 668265263u;
                    h = (h ^ (h >> 13)) * 1274126177u;
                    h ^= h >> 16;
                    if ((float) (h & 0xffff) / 65536.f >= power)
                        data.setPixelColour (x, y, pal::bg);
                }
        }
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (image, getLocalBounds().toFloat());
    }

protected:
    virtual void draw (Canvas&) = 0;

    juce::Point<float> toPx (const juce::MouseEvent& e) const
    {
        return e.position * ((float) kW / (float) juce::jmax (1, getWidth()));
    }

private:
    void applyGlitch()
    {
        juce::Image::BitmapData d (image, juce::Image::BitmapData::readWrite);
        juce::Random rnd ((juce::int64) (juce::Time::getMillisecondCounter() / 45u) * 7919 + 13);

        // fasce orizzontali spostate, a volte con il rosso separato dagli altri canali
        const int bands = 2 + (int) (glitch * 9.f);
        const int maxShift = 4 + (int) (glitch * 44.f);
        std::vector<juce::Colour> row ((size_t) kW);
        for (int b = 0; b < bands; ++b)
        {
            const int y0 = rnd.nextInt (kH), h = 1 + rnd.nextInt (2 + (int) (glitch * 14.f));
            const int shift = rnd.nextInt (2 * maxShift + 1) - maxShift;
            const bool split = rnd.nextBool();
            for (int y = y0; y < juce::jmin (y0 + h, kH); ++y)
            {
                for (int x = 0; x < kW; ++x)
                    row[(size_t) x] = d.getPixelColour (x, y);
                for (int x = 0; x < kW; ++x)
                {
                    auto src = row[(size_t) (((x - shift) % kW + kW) % kW)];
                    if (split)
                    {
                        const auto r = row[(size_t) (((x - shift - 3) % kW + kW) % kW)];
                        src = juce::Colour (r.getRed(), src.getGreen(), src.getBlue());
                    }
                    d.setPixelColour (x, y, src);
                }
            }
        }

        // blocchi di rumore colorato
        static const juce::uint32 cols[5] = { 0xffffffff, 0xff45e8f5, 0xffff3cc8, 0xff000000, 0xffffe066 };
        const int blocks = (int) (glitch * 30.f);
        for (int i = 0; i < blocks; ++i)
        {
            const int x = rnd.nextInt (kW), y = rnd.nextInt (kH), w = 4 + rnd.nextInt (24), h = 1 + rnd.nextInt (3);
            const juce::Colour col (cols[rnd.nextInt (5)]);
            for (int yy = y; yy < juce::jmin (y + h, kH); ++yy)
                for (int xx = x; xx < juce::jmin (x + w, kW); ++xx)
                    d.setPixelColour (xx, yy, col);
        }

        // righe scure alternate (scanline)
        for (int y = 1; y < kH; y += 2)
            for (int x = 0; x < kW; ++x)
                d.setPixelColour (x, y, d.getPixelColour (x, y).darker (glitch * 0.45f));
    }

    void timerCallback() override
    {
        if (isShowing())
            repaint();
    }

    juce::Image image;
    float power = 1.f;
    float glitch = 0.f;
};
} // namespace grv::px
