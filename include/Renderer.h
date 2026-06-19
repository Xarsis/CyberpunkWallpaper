// Renderer.h — instanced glyph drawing with 3D perspective

#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "GlyphAtlas.h"
#include "ColumnManager.h"
#include "ShaderUtils.h"

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>

struct InstanceData {
    glm::vec2 pos;
    glm::vec2 size;
    glm::vec2 uvMin;
    glm::vec2 uvMax;
    glm::vec4 color;
    float     glitchOffset;
};

class Renderer {
public:
    GLuint shaderProgram = 0;
    GLuint VAO = 0, VBO_quad = 0, VBO_instance = 0;
    GlyphAtlas* atlas = nullptr;
    int screenW = 0, screenH = 0;

    glm::mat4 projection;
    glm::mat4 view;

    static const int MAX_INSTANCES = 65536;

    bool init(int sw, int sh, GlyphAtlas* a) {
        screenW = sw;
        screenH = sh;
        atlas   = a;

        shaderProgram = ShaderUtils::loadFromFiles(
            "shaders/glyph.vert",
            "shaders/glyph.frag"
        );
        if (!shaderProgram) {
            std::cerr << "Renderer: failed to load glyph shaders\n";
            return false;
        }

        float quad[] = {
            0.0f, 0.0f,  0.0f, 0.0f,
            1.0f, 0.0f,  1.0f, 0.0f,
            1.0f, 1.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  0.0f, 0.0f,
            1.0f, 1.0f,  1.0f, 1.0f,
            0.0f, 1.0f,  0.0f, 1.0f,
        };

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &VBO_quad);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_quad);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glGenBuffers(1, &VBO_instance);
        glBindBuffer(GL_ARRAY_BUFFER, VBO_instance);
        glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCES * sizeof(InstanceData), nullptr, GL_DYNAMIC_DRAW);

        size_t stride = sizeof(InstanceData);
        auto va = [&](GLuint idx, int count, size_t offset) {
            glEnableVertexAttribArray(idx);
            glVertexAttribPointer(idx, count, GL_FLOAT, GL_FALSE, (GLsizei)stride, (void*)offset);
            glVertexAttribDivisor(idx, 1);
        };
        va(2, 2, offsetof(InstanceData, pos));
        va(3, 2, offsetof(InstanceData, size));
        va(4, 2, offsetof(InstanceData, uvMin));
        va(5, 2, offsetof(InstanceData, uvMax));
        va(6, 4, offsetof(InstanceData, color));
        va(7, 1, offsetof(InstanceData, glitchOffset));

        glBindVertexArray(0);

        float fov    = glm::radians(60.0f);
        float aspect = (float)sw / (float)sh;
        projection   = glm::perspective(fov, aspect, 0.1f, 50.0f);
        view         = glm::lookAt(
            glm::vec3(0, 0, 12.0f),
            glm::vec3(0, 0, 0),
            glm::vec3(0, 1, 0)
        );

        return true;
    }

    void draw(const ColumnManager& cm, float time) {
        if (!shaderProgram) return;

        std::vector<InstanceData> instances;
        instances.reserve(4096);

        glm::mat4 vp = projection * view;

        // Darker green palette
        const glm::vec3 COLOR_HEAD = glm::vec3(0.55f, 0.95f, 0.55f); // pale green
        const glm::vec3 COLOR_MID  = glm::vec3(0.05f, 0.75f, 0.25f); // mid green
        const glm::vec3 COLOR_DARK = glm::vec3(0.02f, 0.40f, 0.12f); // dark green
        const glm::vec3 COLOR_CYAN = glm::vec3(0.00f, 0.60f, 0.65f); // cyan accent

        for (const auto& col : cm.getColumns()) {
            float glyphStep = 0.45f * col.depthScale();

            for (int i = 0; i < (int)col.chars.size(); ++i) {
                float gy = col.y + i * glyphStep;

                glm::vec4 clip = vp * glm::vec4(col.x, gy, col.z, 1.0f);
                if (clip.w <= 0.0f) continue;
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                if (ndc.x < -1.5f || ndc.x > 1.5f) continue;
                if (ndc.y < -1.5f || ndc.y > 1.5f) continue;

                float sx = (ndc.x * 0.5f + 0.5f) * screenW;
                float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * screenH;

                const GlyphInfo* g = atlas->getGlyph(col.chars[i]);
                if (!g) continue;

                // 28pt bitmaps * 2.2 scale gives readable glyph size
                float scale = col.depthScale() * 2.2f;
                float gw    = g->size.x * scale;
                float gh    = g->size.y * scale;
                if (gw < 1.0f || gh < 1.0f) continue;

                float t = (float)i / (float)col.chars.size();
                glm::vec3 rgb;
                if (i == 0) {
                    rgb = COLOR_HEAD;
                } else if (t < 0.2f) {
                    rgb = glm::mix(COLOR_HEAD, COLOR_MID, t / 0.2f);
                } else {
                    bool isCyan = ((int)(col.x * 3.7f) % 7 == 0);
                    glm::vec3 tail = isCyan ? COLOR_CYAN : COLOR_DARK;
                    rgb = glm::mix(COLOR_MID, tail, std::min((t - 0.2f) / 0.8f, 1.0f));
                }

                // Alpha: 0.75 ceiling, fades toward tail
                float alpha = col.depthAlpha() * 0.75f * (1.0f - std::pow(t, 1.8f));
                if (alpha < 0.01f) continue;

                InstanceData inst;
                inst.pos          = glm::vec2(sx - gw * 0.5f, sy - gh * 0.5f);
                inst.size         = glm::vec2(gw, gh);
                inst.uvMin        = g->uvMin;
                inst.uvMax        = g->uvMax;
                inst.color        = glm::vec4(rgb, alpha);
                inst.glitchOffset = 0.0f;

                instances.push_back(inst);
                if ((int)instances.size() >= MAX_INSTANCES) goto doneBuilding;
            }
        }
        doneBuilding:;

        if (instances.empty()) return;

        glBindBuffer(GL_ARRAY_BUFFER, VBO_instance);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
            instances.size() * sizeof(InstanceData), instances.data());

        glUseProgram(shaderProgram);

        glm::mat4 ortho = glm::ortho(0.0f, (float)screenW, (float)screenH, 0.0f, -1.0f, 1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uOrtho"),
                           1, GL_FALSE, glm::value_ptr(ortho));
        glUniform1f(glGetUniformLocation(shaderProgram, "uTime"), time);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlas->textureID);
        glUniform1i(glGetUniformLocation(shaderProgram, "uAtlas"), 0);

        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, (GLsizei)instances.size());
        glBindVertexArray(0);
    }

    void destroy() {
        if (shaderProgram) { glDeleteProgram(shaderProgram); shaderProgram = 0; }
        if (VAO)           { glDeleteVertexArrays(1, &VAO); }
        if (VBO_quad)      { glDeleteBuffers(1, &VBO_quad); }
        if (VBO_instance)  { glDeleteBuffers(1, &VBO_instance); }
    }
};
