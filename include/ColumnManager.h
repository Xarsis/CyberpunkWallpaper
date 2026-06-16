// ColumnManager.h — manages 3D falling character streams

#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <random>
#include <algorithm>

// ─── One glyph instance (fed to the renderer as instance data) ────────────────
struct GlyphInstance {
    glm::vec3 worldPos;     // 3D position
    glm::vec2 uvMin;        // atlas coords
    glm::vec2 uvMax;
    glm::vec4 color;        // RGBA — head glyphs are bright white/cyan, trail fades
    float     size;         // world-space glyph size
    float     glitchOffset; // per-glyph horizontal glitch displacement
};

// ─── One falling column / stream ─────────────────────────────────────────────
struct Column {
    float x;           // world X
    float z;           // world Z (depth for perspective)
    float y;           // current head Y (starts at top, falls downward)
    float speed;       // units/sec
    int   length;      // number of glyphs in trail
    float glitchTimer; // seconds until next random char swap
    float glitchRate;  // how often chars mutate
    std::vector<wchar_t> chars; // trail characters

    // Derived from z: depth scale and tint
    float depthScale() const {
        // z in [-6, +6], scale 0.6 at back to 1.2 at front
        return 0.6f + (z + 6.0f) / 12.0f * 0.6f;
    }
    float depthAlpha() const {
        // Minimum 0.6 so even back-most columns are clearly visible
        return 0.6f + (z + 6.0f) / 12.0f * 0.4f;
    }
};

class ColumnManager {
public:
    std::vector<Column>        columns;
    std::vector<GlyphInstance> instances; // rebuilt each frame, passed to renderer

    // Cyberpunk color palette: neon green, cyan, hot pink accents
    // Head char = near-white; trail fades through palette to near-black
    static constexpr glm::vec3 COLOR_HEAD    = { 0.85f, 1.0f,  0.85f }; // bright white-green
    static constexpr glm::vec3 COLOR_MID     = { 0.05f, 0.9f,  0.35f }; // neon green
    static constexpr glm::vec3 COLOR_CYAN    = { 0.0f,  0.85f, 0.9f  }; // cyan accent
    static constexpr glm::vec3 COLOR_PINK    = { 0.9f,  0.05f, 0.55f }; // hot pink glitch

    std::mt19937 rng;

    // World dimensions (OpenGL units, not pixels)
    float worldW = 40.0f;   // visible world width
    float worldH = 25.0f;   // visible world height
    int   screenW, screenH;

    // Glitch state
    float glitchIntensity = 0.0f;  // 0..1, spikes randomly
    float glitchTimer     = 0.0f;

    void init(int sw, int sh) {
        screenW = sw;
        screenH = sh;
        rng.seed(std::random_device{}());

        // Spawn initial columns across x and z range
        int numColumns = 650;
        for (int i = 0; i < numColumns; ++i)
            spawnColumn(true);
    }

    void update(float dt, float time) {
        updateGlitch(dt);
        instances.clear();

        for (auto& col : columns) {
            // ── Move column head downward ──────────────────────────────────
            col.y -= col.speed * dt;

            // ── Mutate characters over time ────────────────────────────────
            col.glitchTimer -= dt;
            if (col.glitchTimer <= 0.0f) {
                col.glitchTimer = col.glitchRate;
                // randomize a random position in the trail
                std::uniform_int_distribution<int> idx(0, (int)col.chars.size() - 1);
                col.chars[idx(rng)] = randomChar();
            }

            // ── Build glyph instances for this column ──────────────────────
            float glyphStep = 0.45f * col.depthScale(); // spacing between glyphs
            for (int i = 0; i < (int)col.chars.size(); ++i) {
                float gy = col.y + i * glyphStep;

                // Cull glyphs outside vertical view
                if (gy < -worldH * 0.5f - 2.0f || gy > worldH * 0.5f + 2.0f)
                    continue;

                GlyphInstance inst;
                inst.worldPos = glm::vec3(col.x, gy, col.z);
                inst.size     = 0.38f * col.depthScale();

                // Color: head = bright, trail fades
                float t = (float)i / (float)col.chars.size(); // 0=head, 1=tail
                glm::vec3 col_rgb;
                if (i == 0) {
                    col_rgb = COLOR_HEAD;
                } else if (t < 0.15f) {
                    col_rgb = glm::mix(COLOR_HEAD, COLOR_MID, t / 0.15f);
                } else {
                    // Occasional cyan/pink columns
                    bool isCyan = ((int)(col.x * 3.7f) % 7 == 0);
                    bool isPink = ((int)(col.x * 5.1f) % 11 == 0) && glitchIntensity > 0.3f;
                    glm::vec3 tailColor = isCyan ? COLOR_CYAN : (isPink ? COLOR_PINK : COLOR_MID);
                    col_rgb = glm::mix(tailColor, glm::vec3(0.0f), std::min(t * 1.2f, 1.0f));
                }

                float alpha = col.depthAlpha() * (1.0f - t * 0.85f);
                inst.color  = glm::vec4(col_rgb, alpha);

                // Per-glyph glitch: horizontal jitter during glitch spike
                inst.glitchOffset = 0.0f;
                if (glitchIntensity > 0.4f && i > 2) {
                    std::uniform_real_distribution<float> jitter(-0.08f, 0.08f);
                    inst.glitchOffset = jitter(rng) * glitchIntensity;
                }

                // Set UV from atlas (placeholder — Renderer will look these up)
                inst.uvMin = glm::vec2(0);
                inst.uvMax = glm::vec2(1);

                // Store character index as a float in glitchOffset.y trick:
                // We'll encode the char as a separate field in a real impl,
                // but for this scaffold we store the char in a parallel array.
                // See Renderer.h for how chars[] is cross-referenced.
                instances.push_back(inst);
            }

            // ── Reset column when fully off bottom ────────────────────────
            if (col.y + col.chars.size() * 0.45f < -worldH * 0.5f - 2.0f)
                resetColumn(col);
        }
    }

    // Returns all instances for the renderer
    const std::vector<GlyphInstance>& getInstances() const { return instances; }

    // All columns (needed by renderer to get char data)
    const std::vector<Column>& getColumns() const { return columns; }

private:
    float glitchSpawnTimer = 0.0f;

    void updateGlitch(float dt) {
        // Glitch intensity decays naturally
        glitchIntensity = std::max(0.0f, glitchIntensity - dt * 1.5f);

        // Random glitch spikes
        glitchSpawnTimer -= dt;
        if (glitchSpawnTimer <= 0.0f) {
            std::uniform_real_distribution<float> nextSpike(3.0f, 12.0f);
            std::uniform_real_distribution<float> intensity(0.3f, 1.0f);
            glitchSpawnTimer  = nextSpike(rng);
            glitchIntensity   = intensity(rng);
        }
    }

    void spawnColumn(bool randomYStart = false) {
        std::uniform_real_distribution<float> xDist(-worldW * 0.5f, worldW * 0.5f);
        std::uniform_real_distribution<float> zDist(-6.0f, 6.0f);
        std::uniform_real_distribution<float> speedDist(3.0f, 10.0f);
        std::uniform_int_distribution<int>    lenDist(8, 32);
        std::uniform_real_distribution<float> rateDist(0.05f, 0.2f);

        Column col;
        col.x         = xDist(rng);
        col.z         = zDist(rng);
        col.speed     = speedDist(rng) * (0.5f + (col.z + 6.0f) / 12.0f); // faster = closer
        col.length    = lenDist(rng);
        col.glitchRate = rateDist(rng);
        col.glitchTimer = col.glitchRate;

        if (randomYStart) {
            std::uniform_real_distribution<float> yDist(-worldH * 0.5f, worldH * 0.5f);
            col.y = yDist(rng);
        } else {
            col.y = worldH * 0.5f + 2.0f; // start above top
        }

        col.chars.resize(col.length);
        for (auto& c : col.chars) c = randomChar();

        columns.push_back(col);
    }

    void resetColumn(Column& col) {
        std::uniform_real_distribution<float> xDist(-worldW * 0.5f, worldW * 0.5f);
        std::uniform_real_distribution<float> zDist(-6.0f, 6.0f);
        std::uniform_real_distribution<float> speedDist(3.0f, 10.0f);
        std::uniform_int_distribution<int>    lenDist(8, 32);

        col.x     = xDist(rng);
        col.z     = zDist(rng);
        col.speed = speedDist(rng) * (0.5f + (col.z + 6.0f) / 12.0f);
        col.length = lenDist(rng);
        col.y     = worldH * 0.5f + 2.0f;
        col.chars.resize(col.length);
        for (auto& c : col.chars) c = randomChar();
    }

    wchar_t randomChar() {
        // Katakana halfwidth range: FF66–FF9F
        static const std::wstring pool = []() {
            std::wstring s;
            for (wchar_t c = 0xFF66; c <= 0xFF9F; ++c) s += c;
            for (wchar_t c = L'0'; c <= L'9'; ++c) s += c;
            s += L"ABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*";
            return s;
        }();
        std::uniform_int_distribution<int> d(0, (int)pool.size() - 1);
        return pool[d(rng)];
    }
};
