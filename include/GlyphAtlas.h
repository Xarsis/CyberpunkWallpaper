// GlyphAtlas.h — FreeType glyph texture atlas

#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>
#include <unordered_map>
#include <vector>

struct GlyphInfo {
    glm::vec2 uvMin;
    glm::vec2 uvMax;
    glm::vec2 size;
    glm::vec2 bearing;
    float     advance;
};

class GlyphAtlas {
public:
    GLuint   textureID = 0;
    int      atlasW    = 1024;
    int      atlasH    = 256;
    float    glyphW    = 0;
    float    glyphH    = 0;

    std::unordered_map<wchar_t, GlyphInfo> glyphs;

    // Katakana only
    static std::wstring defaultCharset() {
        std::wstring cs;
        for (wchar_t c = 0xFF66; c <= 0xFF9F; ++c) cs += c;
        return cs;
    }

    bool init(const std::string& fontPath, int pixelHeight) {
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) return false;

        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
            FT_Done_FreeType(ft);
            return false;
        }

        FT_Set_Pixel_Sizes(face, 0, pixelHeight);
        std::wstring charset = defaultCharset();

        int maxGlyphW = 0, maxGlyphH = 0;
        for (wchar_t c : charset) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;
            maxGlyphW = std::max(maxGlyphW, (int)face->glyph->bitmap.width);
            maxGlyphH = std::max(maxGlyphH, (int)face->glyph->bitmap.rows);
        }
        glyphW = (float)(maxGlyphW + 2);
        glyphH = (float)(maxGlyphH + 2);

        int cols   = atlasW / (int)glyphW;
        int needed = ((int)charset.size() + cols - 1) / cols;
        atlasH     = needed * (int)glyphH + 4;

        std::vector<unsigned char> pixels(atlasW * atlasH, 0);

        int cx = 0, cy = 0;
        for (wchar_t c : charset) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;
            FT_GlyphSlot g = face->glyph;

            if (cx + (int)glyphW > atlasW) { cx = 0; cy += (int)glyphH; }

            for (unsigned int row = 0; row < g->bitmap.rows; ++row) {
                for (unsigned int col = 0; col < g->bitmap.width; ++col) {
                    int px = cx + col + 1;
                    int py = cy + row + 1;
                    if (px < atlasW && py < atlasH)
                        pixels[py * atlasW + px] = g->bitmap.buffer[row * g->bitmap.pitch + col];
                }
            }

            GlyphInfo info;
            info.uvMin   = glm::vec2((float)cx / atlasW,            (float)cy / atlasH);
            info.uvMax   = glm::vec2((float)(cx + glyphW) / atlasW, (float)(cy + glyphH) / atlasH);
            info.size    = glm::vec2(g->bitmap.width, g->bitmap.rows);
            info.bearing = glm::vec2(g->bitmap_left, g->bitmap_top);
            info.advance = (float)(g->advance.x >> 6);
            glyphs[c]    = info;

            cx += (int)glyphW;
        }

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasW, atlasH, 0,
                     GL_RED, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        FT_Done_Face(face);
        FT_Done_FreeType(ft);
        return true;
    }

    const GlyphInfo* getGlyph(wchar_t c) const {
        auto it = glyphs.find(c);
        return (it != glyphs.end()) ? &it->second : nullptr;
    }

    void destroy() {
        if (textureID) { glDeleteTextures(1, &textureID); textureID = 0; }
    }
};
