// ColumnManager.h — manages 3D falling character streams

#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <random>
#include <algorithm>

struct Column {
    float x, z, y;
    float speed;
    int   length;
    float glitchTimer;
    float glitchRate;
    std::vector<wchar_t> chars;

    float depthScale() const {
        // z in [-6,+6]: 0.6 back, 1.2 front
        return 0.6f + (z + 6.0f) / 12.0f * 0.6f;
    }
    float depthAlpha() const {
        // 0.6 minimum so back columns stay visible
        return 0.6f + (z + 6.0f) / 12.0f * 0.4f;
    }
};

class ColumnManager {
public:
    std::vector<Column> columns;
    std::mt19937        rng;
    float worldW            = 40.0f;
    float worldH            = 25.0f;
    float glitchIntensity   = 0.0f;
    float glitchSpawnTimer  = 0.0f;

    void init(int /*sw*/, int /*sh*/) {
        rng.seed(std::random_device{}());
        for (int i = 0; i < 650; ++i)
            spawnColumn(true);
    }

    void update(float dt, float /*time*/) {
        // Glitch intensity decay
        glitchIntensity = std::max(0.0f, glitchIntensity - dt * 1.5f);
        glitchSpawnTimer -= dt;
        if (glitchSpawnTimer <= 0.0f) {
            std::uniform_real_distribution<float> nextSpike(5.0f, 18.0f);
            std::uniform_real_distribution<float> intensityDist(0.15f, 0.5f);
            glitchSpawnTimer = nextSpike(rng);
            glitchIntensity  = intensityDist(rng);
        }

        for (auto& col : columns) {
            col.y -= col.speed * dt;

            col.glitchTimer -= dt;
            if (col.glitchTimer <= 0.0f) {
                col.glitchTimer = col.glitchRate;
                std::uniform_int_distribution<int> idx(0, (int)col.chars.size() - 1);
                col.chars[idx(rng)] = randomChar();
            }

            float step = 0.45f * col.depthScale();
            if (col.y + col.chars.size() * step < -worldH * 0.5f - 2.0f)
                resetColumn(col);
        }
    }

    const std::vector<Column>& getColumns() const { return columns; }
    float getGlitchIntensity() const { return glitchIntensity; }

private:
    void spawnColumn(bool randomYStart = false) {
        std::uniform_real_distribution<float> xDist(-worldW * 0.5f, worldW * 0.5f);
        std::uniform_real_distribution<float> zDist(-6.0f, 6.0f);
        std::uniform_real_distribution<float> speedDist(1.5f, 5.5f);
        std::uniform_int_distribution<int>    lenDist(8, 32);
        std::uniform_real_distribution<float> rateDist(0.05f, 0.2f);

        Column col;
        col.x           = xDist(rng);
        col.z           = zDist(rng);
        col.speed       = speedDist(rng) * (0.5f + (col.z + 6.0f) / 12.0f);
        col.length      = lenDist(rng);
        col.glitchRate  = rateDist(rng);
        col.glitchTimer = col.glitchRate;

        if (randomYStart) {
            std::uniform_real_distribution<float> yDist(-worldH * 0.5f, worldH * 0.5f);
            col.y = yDist(rng);
        } else {
            col.y = worldH * 0.5f + 2.0f;
        }

        col.chars.resize(col.length);
        for (auto& c : col.chars) c = randomChar();
        columns.push_back(col);
    }

    void resetColumn(Column& col) {
        std::uniform_real_distribution<float> xDist(-worldW * 0.5f, worldW * 0.5f);
        std::uniform_real_distribution<float> zDist(-6.0f, 6.0f);
        std::uniform_real_distribution<float> speedDist(1.5f, 5.5f);
        std::uniform_int_distribution<int>    lenDist(8, 32);

        col.x      = xDist(rng);
        col.z      = zDist(rng);
        col.speed  = speedDist(rng) * (0.5f + (col.z + 6.0f) / 12.0f);
        col.length = lenDist(rng);
        col.y      = worldH * 0.5f + 2.0f;
        col.chars.resize(col.length);
        for (auto& c : col.chars) c = randomChar();
    }

    wchar_t randomChar() {
        // Halfwidth katakana only: FF66-FF9F
        static const std::wstring pool = []() {
            std::wstring s;
            for (wchar_t c = 0xFF66; c <= 0xFF9F; ++c) s += c;
            return s;
        }();
        std::uniform_int_distribution<int> d(0, (int)pool.size() - 1);
        return pool[d(rng)];
    }
};
