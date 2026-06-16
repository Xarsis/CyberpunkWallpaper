// PostProcessor.h — full-screen framebuffer post-processing
// Applies: glitch strip offsets, RGB channel shift, scanlines, vignette

#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "ShaderUtils.h"
#include <iostream>

class PostProcessor {
public:
    GLuint FBO = 0;
    GLuint colorTexture = 0;
    GLuint RBO = 0;  // renderbuffer for depth (not used but good practice)
    GLuint quadVAO = 0, quadVBO = 0;
    GLuint shaderProgram = 0;
    int screenW = 0, screenH = 0;

    bool init(int sw, int sh) {
        screenW = sw;
        screenH = sh;

        // ── Framebuffer ───────────────────────────────────────────────────────
        glGenFramebuffers(1, &FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);

        glGenTextures(1, &colorTexture);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, sw, sh, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "PostProcessor FBO incomplete\n";
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            return false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ── Full-screen quad ──────────────────────────────────────────────────
        float quad[] = {
            -1.0f,  1.0f,   0.0f, 1.0f,
            -1.0f, -1.0f,   0.0f, 0.0f,
             1.0f, -1.0f,   1.0f, 0.0f,
            -1.0f,  1.0f,   0.0f, 1.0f,
             1.0f, -1.0f,   1.0f, 0.0f,
             1.0f,  1.0f,   1.0f, 1.0f,
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);

        // ── Shader ────────────────────────────────────────────────────────────
        shaderProgram = ShaderUtils::loadFromFiles(
            "shaders/post.vert",
            "shaders/post.frag"
        );
        return shaderProgram != 0;
    }

    void bindFramebuffer() {
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glViewport(0, 0, screenW, screenH);
    }

    void unbindFramebuffer() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void render(float time) {
        glDisable(GL_DEPTH_TEST);
        glUseProgram(shaderProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorTexture);
        glUniform1i(glGetUniformLocation(shaderProgram, "uScene"),    0);
        glUniform1f(glGetUniformLocation(shaderProgram, "uTime"),     time);
        glUniform1f(glGetUniformLocation(shaderProgram, "uResX"),     (float)screenW);
        glUniform1f(glGetUniformLocation(shaderProgram, "uResY"),     (float)screenH);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
    }

    void destroy() {
        if (FBO)           { glDeleteFramebuffers(1, &FBO); }
        if (colorTexture)  { glDeleteTextures(1, &colorTexture); }
        if (quadVAO)       { glDeleteVertexArrays(1, &quadVAO); }
        if (quadVBO)       { glDeleteBuffers(1, &quadVBO); }
        if (shaderProgram) { glDeleteProgram(shaderProgram); }
    }
};
