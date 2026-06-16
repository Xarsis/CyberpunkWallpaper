// ShaderUtils.h — compile, link, and load GLSL shaders

#pragma once
#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace ShaderUtils {

    inline GLuint compile(GLenum type, const std::string& src) {
        GLuint id = glCreateShader(type);
        const char* c = src.c_str();
        glShaderSource(id, 1, &c, nullptr);
        glCompileShader(id);

        GLint ok = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetShaderInfoLog(id, 1024, nullptr, log);
            std::cerr << "Shader compile error:\n" << log << "\n";
            glDeleteShader(id);
            return 0;
        }
        return id;
    }

    inline GLuint link(GLuint vert, GLuint frag) {
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vert);
        glAttachShader(prog, frag);
        glLinkProgram(prog);

        GLint ok = 0;
        glGetProgramiv(prog, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[1024];
            glGetProgramInfoLog(prog, 1024, nullptr, log);
            std::cerr << "Shader link error:\n" << log << "\n";
            glDeleteProgram(prog);
            return 0;
        }
        glDeleteShader(vert);
        glDeleteShader(frag);
        return prog;
    }

    inline std::string readFile(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "Could not open shader: " << path << "\n";
            return "";
        }
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    inline GLuint loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
        std::string vs = readFile(vertPath);
        std::string fs = readFile(fragPath);
        if (vs.empty() || fs.empty()) return 0;

        GLuint vert = compile(GL_VERTEX_SHADER,   vs);
        GLuint frag = compile(GL_FRAGMENT_SHADER, fs);
        if (!vert || !frag) return 0;

        return link(vert, frag);
    }

    inline GLuint loadFromSource(const std::string& vs, const std::string& fs) {
        GLuint vert = compile(GL_VERTEX_SHADER,   vs);
        GLuint frag = compile(GL_FRAGMENT_SHADER, fs);
        if (!vert || !frag) return 0;
        return link(vert, frag);
    }
}
