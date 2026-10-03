#pragma once

#include "PixelCanvas.h"
#include <algorithm>
#include <vector>
#include <functional>

// Gioco nascosto (easter egg): una pallina corre da sinistra verso destra e si salta gli ostacoli con un click.
// Tutto in coordinate dello schermo pixel 224x168.

namespace grv
{
class RunnerGame
{
public:
    enum State { Ready, Running, Dead };

    std::function<void (int)> onEvent;      // 0 = salto, 1 = morte (per i suoni)

    State getState() const { return state; }
    int getScore() const { return (int) (dist / 8.f); }
    int getBest() const { return best; }
    float getBallHeight() const { return by; }

    // solo per le prove: porta il gioco a un certo punto della sfida e mostra una serie di ostacoli
    void debugShowPatterns (float distance, int firstIdx = 100)
    {
        state = Running;
        dist = distance;
        by = 90.f;                                  // la pallina resta in alto, così non si schianta durante la prova
        obstacles.clear();
        float x = 12.f;
        for (int i = 0; i < 6; ++i)
        {
            auto o = makePattern (firstIdx - i);
            o.x = x;
            x += (float) o.w + 14.f;
            if (x > 200.f) break;
            obstacles.push_back (o);
        }
    }

    void reset()
    {
        state = Ready;
        dist = 0.f;
        by = vy = 0.f;
        obstacles.clear();
        particles.clear();
        spawnIn = 190.f;
        lastMs = 0.0;
        deadAtMs = 0.0;
        held = false;
        lastMilestone = 0;
        flashAtMs = 0.0;
    }

    // click del mouse: avvia / salta / riprova
    void tap()
    {
        held = true;                       // finché il tasto resta premuto la pallina continua a saltare
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (state == Ready)
        {
            state = Running;
            jump();
        }
        else if (state == Running)
            jump();
        else if (now - deadAtMs > 350.0)
        {
            reset();
            state = Running;
            jump();
        }
    }

    // rilascio del tasto JUMP
    void release() { held = false; }

    // fa avanzare la fisica col tempo reale (a passi piccoli, così le collisioni sono precise)
    void update()
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        double dt = lastMs > 0.0 ? juce::jlimit (0.0, 50.0, now - lastMs) / 1000.0 : 0.0;
        lastMs = now;
        while (dt > 0.0)
        {
            const float step = (float) juce::jmin (dt, 0.008);
            tick (step);
            dt -= step;
        }
    }

    void draw (px::Canvas& c) const
    {
        using namespace px;
        c.clear (pal::bg);

        // stelle lontane (parallasse lenta)
        for (int i = 0; i < 22; ++i)
        {
            const juce::uint32 h = (juce::uint32) (i + 1) * 2654435761u;
            const int sx = (int) (((h >> 8) % 224u) - (juce::uint32) (dist * (0.04f + 0.03f * (float) (i % 3)))) ;
            const int x = ((sx % 224) + 224) % 224;
            const int y = 14 + (int) ((h >> 20) % 100u);
            c.plot (x, y, i % 4 == 0 ? pal::mid : pal::grid);
        }

        // terreno
        c.fill (0, kGround, kW, 2, pal::light);
        for (int row = 0; row < 3; ++row)
        {
            const float k = 1.f + 0.25f * (float) row;
            const int y = kGround + 7 + row * 8;
            const int off = (int) (dist * k) % 24;
            for (int x = -24; x < kW; x += 24)
                c.fill (x + 8 * row + 24 - off, y, 6 - row, 1, row == 0 ? pal::dim : pal::grid);
        }

        // ostacoli
        for (auto& o : obstacles)
            drawObstacle (c, o);

        // ombra e pallina
        if (state != Dead || particles.empty())
        {
            const int sw = juce::jmax (4, 12 - (int) (by / 5.f));
            c.fill (kBallX - sw / 2, kGround + 2, sw, 1, pal::grid);
            if (state != Dead)
                drawBall (c);
        }

        for (auto& p : particles)
            c.fill ((int) p.x, (int) p.y, 2, 2, p.col);

        // HUD
        c.text ("BEST SCORE " + juce::String (best).paddedLeft ('0', 5), 4, 4, pal::dim);
        c.textRight ("SCORE " + juce::String (getScore()).paddedLeft ('0', 5), 220, 4, pal::light);

        const bool blink = (juce::Time::getMillisecondCounter() / 450) % 2 == 0;
        if (state == Running && juce::Time::getMillisecondCounterHiRes() - flashAtMs < 1300.0)
        {
            const bool on = (juce::Time::getMillisecondCounter() / 110) % 2 == 0;
            c.textCentre ("SPEED UP!", 112, 22, on ? pal::yellow : pal::red);
        }
        if (state == Ready)
        {
            c.textCentre ("PRESS JUMP TO START", 112, 62, blink ? pal::yellow : pal::dim);
            c.textCentre ("HOLD JUMP TO KEEP JUMPING", 112, 78, pal::mid);
        }
        else if (state == Dead)
        {
            c.textCentre ("GAME OVER", 112, 52, pal::red, 2);
            c.textCentre ("SCORE " + juce::String (getScore()), 112, 74, pal::light);
            if (blink)
                c.textCentre ("PRESS JUMP TO RETRY", 112, 90, pal::yellow);
        }
    }

private:
    static constexpr int kGround = 136, kBallX = 44, kR = 6;
    static constexpr float kGravity = 760.f, kJump = 255.f;

    // un ostacolo è fatto di "pezzi": quadrati e spine, a terra o sospesi
    struct Part { int dx, w, h, lift; bool spike, down; };
    struct Obstacle { float x; int w; std::vector<Part> parts; juce::Colour col; };
    struct Particle { float x, y, vx, vy; juce::Colour col; };

    State state = Ready;
    float dist = 0.f, by = 0.f, vy = 0.f, spawnIn = 190.f;
    int best = 0, lastMilestone = 0;
    bool held = false;
    double lastMs = 0.0, deadAtMs = 0.0, flashAtMs = 0.0;
    std::vector<Obstacle> obstacles;
    std::vector<Particle> particles;
    juce::Random rnd;

    // la velocità sale in fretta: da 100 a 300 px/s intorno al punteggio 500
    float speed() const { return juce::jmin (300.f, 100.f + dist * 0.05f); }

    void jump()
    {
        if (by <= 0.5f)
        {
            vy = kJump;
            if (onEvent)
                onEvent (0);
        }
    }

    static Part block (int dx, int w, int h, int lift = 0) { return { dx, w, h, lift, false, false }; }
    static Part spikeUp (int dx, int w, int h, int lift = 0) { return { dx, w, h, lift, true, false }; }
    static Part spikeDown (int dx, int w, int h, int lift) { return { dx, w, h, lift, true, true }; }

    // sceglie la forma dell'ostacolo: più si va avanti, più forme difficili entrano in gioco
    Obstacle makePattern (int forceIdx = -1)
    {
        const int score = getScore();
        const float sp = speed();
        const float maxW = 0.5f * sp - 14.f;                 // un gruppo troppo largo non si salterebbe a questa velocità
        std::vector<std::vector<Part>> pool;
        pool.push_back ({ block (0, 10, 12) });                                            // quadrato basso
        pool.push_back ({ block (0, 20, 12) });                                            // quadrato largo
        if (score >= 15) pool.push_back ({ spikeUp (0, 10, 12) });                         // spina
        if (score >= 25) pool.push_back ({ block (0, 10, 22) });                           // quadrato alto
        if (score >= 45) pool.push_back ({ block (0, 12, 10), spikeUp (1, 10, 9, 10) });   // quadrato con la spina sopra
        if (score >= 70) pool.push_back ({ block (0, 8, 12), block (14, 8, 12) });         // due quadrati
        if (score >= 90) pool.push_back ({ spikeUp (0, 10, 12), spikeUp (10, 10, 12) });   // due spine
        if (score >= 130) pool.push_back ({ block (0, 10, 10), block (10, 10, 18), spikeUp (20, 10, 12) });   // scala che finisce in una spina
        if (score >= 150) pool.push_back ({ block (0, 36, 6, 26), spikeDown (4, 8, 6, 20), spikeDown (14, 8, 6, 20), spikeDown (24, 8, 6, 20) });   // sbarra con spine: NON si salta, si passa sotto!
        if (score >= 180) pool.push_back ({ spikeUp (0, 10, 12), spikeUp (10, 10, 12), spikeUp (20, 10, 12) });   // tre spine
        if (score >= 230) pool.push_back ({ block (0, 10, 12), spikeUp (14, 10, 12), block (28, 10, 12) });       // quadrato, spina, quadrato

        std::vector<std::vector<Part>> ok;
        for (auto& p : pool)
        {
            int w = 0;
            for (auto& q : p) w = juce::jmax (w, q.dx + q.w);
            if ((float) w <= maxW)
                ok.push_back (p);
        }
        auto parts = forceIdx >= 0 ? ok[(size_t) forceIdx % ok.size()] : ok[(size_t) rnd.nextInt ((int) ok.size())];
        Obstacle o;
        o.x = (float) (px::kW + 8);
        o.parts = parts;
        o.w = 0;
        for (auto& q : o.parts) o.w = juce::jmax (o.w, q.dx + q.w);
        static const int blockPedals[2] = { 2, 3 };               // il rosso è riservato alle spine
        o.col = rnd.nextBool() ? px::pal::cyan : px::pal::pedal (blockPedals[rnd.nextInt (2)]);
        return o;
    }

    void spawn()
    {
        auto o = makePattern();
        obstacles.push_back (o);
        const float sp = speed();
        const float d = juce::jlimit (0.f, 1.f, (float) getScore() / 400.f);
        // distanza fino al prossimo ostacolo: il minimo giocabile è ~0.72 s di corsa; col tempo gli ostacoli si stringono
        spawnIn = (float) o.w + sp * (0.72f + rnd.nextFloat() * (0.75f - 0.35f * d)) + 8.f;
    }

    bool hits (const Obstacle& o) const
    {
        const float cx = (float) kBallX, cy = (float) kGround - (float) kR - by;
        const float r = (float) kR - 1.5f;
        for (auto& p : o.parts)
        {
            float rx = o.x + (float) p.dx, rw = (float) p.w, rh = (float) p.h;
            float ry = (float) (kGround - p.lift - p.h);
            if (p.spike)
            {
                rx += 3.f; rw -= 6.f; rh -= 4.f;               // la punta è più indulgente
                if (! p.down) ry += 4.f;
            }
            const float nx = juce::jlimit (rx, rx + rw, cx), ny = juce::jlimit (ry, ry + rh, cy);
            const float dx = cx - nx, dy = cy - ny;
            if (dx * dx + dy * dy < r * r)
                return true;
        }
        return false;
    }

    void tick (float dt)
    {
        for (auto& p : particles)
        {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vy += 500.f * dt;
        }
        particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& p) { return p.y > 180.f; }), particles.end());

        if (state != Running)
            return;

        const float sp = speed();
        dist += sp * dt;

        if (by > 0.f || vy > 0.f)
        {
            by += vy * dt;
            vy -= kGravity * dt;
            if (by <= 0.f)
                by = vy = 0.f;
        }
        if (held && by <= 0.5f && vy <= 0.f)
            jump();                                  // tasto tenuto: appena atterra rimbalza di nuovo

        const int milestone = getScore() / 100;
        if (milestone > lastMilestone)
        {
            lastMilestone = milestone;
            flashAtMs = juce::Time::getMillisecondCounterHiRes();
        }

        for (auto& o : obstacles)
            o.x -= sp * dt;
        obstacles.erase (std::remove_if (obstacles.begin(), obstacles.end(), [] (const Obstacle& o) { return o.x + (float) o.w < -4.f; }), obstacles.end());

        spawnIn -= sp * dt;
        if (spawnIn <= 0.f)
            spawn();

        for (auto& o : obstacles)
            if (hits (o))
            {
                state = Dead;
                deadAtMs = juce::Time::getMillisecondCounterHiRes();
                best = juce::jmax (best, getScore());
                if (onEvent)
                    onEvent (1);
                for (int i = 0; i < 16; ++i)
                    particles.push_back ({ (float) kBallX, (float) kGround - (float) kR - by,
                                           (rnd.nextFloat() - 0.5f) * 170.f, -40.f - rnd.nextFloat() * 160.f,
                                           i % 3 == 0 ? px::pal::light : px::pal::yellow });
                return;
            }
    }

    void drawBall (px::Canvas& c) const
    {
        using namespace px;
        const int cy = kGround - kR - (int) by;
        for (int dy = -kR; dy < kR; ++dy)
        {
            const float yy = (float) dy + 0.5f;
            const int hw = (int) std::floor (std::sqrt ((float) (kR * kR) - yy * yy) + 0.5f);
            c.fill (kBallX - hw, cy + dy, hw * 2, 1, pal::yellow);
        }
        c.fill (kBallX - 4, cy - 4, 2, 2, juce::Colours::white);            // riflesso
        const float ang = dist / (float) kR;                                  // la macchia ruota: sembra che rotoli
        c.fill (kBallX + (int) std::round (std::cos (ang) * 3.f) - 1, cy + (int) std::round (std::sin (ang) * 3.f) - 1, 2, 2, juce::Colour (0xffc89a2a));
    }

    static void drawObstacle (px::Canvas& c, const Obstacle& o)
    {
        for (auto& p : o.parts)
        {
            const int x = (int) o.x + p.dx, y = kGround - p.lift - p.h;
            if (p.spike)
            {
                const auto col = px::pal::red;
                for (int r = 0; r < p.h; ++r)
                {
                    // punta in su: più larga verso il basso; spina appesa: più larga verso l'alto
                    const int row = p.down ? p.h - 1 - r : r;
                    const int hw = juce::jmax (1, (int) ((float) p.w * 0.5f * (float) (row + 1) / (float) p.h));
                    c.fill (x + p.w / 2 - hw, y + r, hw * 2, 1, col);
                    c.fill (x + p.w / 2 - hw, y + r, 1, 1, juce::Colour (0xffff9aa4));   // bordo luminoso a sinistra
                }
            }
            else
            {
                const auto dark = o.col.darker (0.5f);
                c.fill (x, y, p.w, p.h, o.col);
                c.fill (x, y, 2, p.h, dark);
                c.fill (x + p.w - 2, y, 2, p.h, dark);
                c.fill (x + 2, y + 2, juce::jmax (1, p.w - 4), 1, juce::Colours::white.withAlpha (0.5f));
            }
        }
    }
};
} // namespace grv
