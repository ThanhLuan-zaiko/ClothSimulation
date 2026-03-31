#include "Shader.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace cloth {

Shader::Shader(const std::string& vertexSrc, const std::string& fragmentSrc) {
    m_RendererID = CreateShaderProgram(vertexSrc, fragmentSrc);
}

Shader::~Shader() {
    if (m_RendererID != 0) {
        glDeleteProgram(m_RendererID);
    }
}

Shader::Shader(Shader&& other) noexcept
    : m_RendererID(other.m_RendererID), m_UniformLocationCache(std::move(other.m_UniformLocationCache)) {
    other.m_RendererID = 0;
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (m_RendererID != 0) {
            glDeleteProgram(m_RendererID);
        }

        m_RendererID = other.m_RendererID;
        m_UniformLocationCache = std::move(other.m_UniformLocationCache);
        
        other.m_RendererID = 0;
    }
    return *this;
}

void Shader::Bind() const {
    glUseProgram(m_RendererID);
}

void Shader::Unbind() const {
    glUseProgram(0);
}

void Shader::SetBool(const std::string& name, bool value) const {
    glUniform1i(GetUniformLocation(name), static_cast<int>(value));
}

void Shader::SetInt(const std::string& name, int value) const {
    glUniform1i(GetUniformLocation(name), value);
}

void Shader::SetUint(const std::string& name, unsigned int value) const {
    glUniform1ui(GetUniformLocation(name), value);
}

void Shader::SetFloat(const std::string& name, float value) const {
    glUniform1f(GetUniformLocation(name), value);
}

void Shader::SetVec2(const std::string& name, const glm::vec2& value) const {
    glUniform2fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(GetUniformLocation(name), 1, &value[0]);
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, &value[0][0]);
}

int Shader::GetUniformLocation(const std::string& name) const {
    if (m_UniformLocationCache.find(name) != m_UniformLocationCache.end()) {
        return m_UniformLocationCache[name];
    }

    int location = glGetUniformLocation(m_RendererID, name.c_str());
    m_UniformLocationCache[name] = location;
    return location;
}

Shader Shader::CreateFromFile(const std::string& vertexPath, const std::string& fragmentPath) {
    std::ifstream vFile(vertexPath);
    std::ifstream fFile(fragmentPath);

    if (!vFile.is_open() || !fFile.is_open()) {
        std::cerr << "Failed to open shader files: " << vertexPath << ", " << fragmentPath << std::endl;
        return Shader();
    }

    std::stringstream vStream, fStream;
    vStream << vFile.rdbuf();
    fStream << fFile.rdbuf();

    return Shader(vStream.str(), fStream.str());
}

Shader Shader::CreateComputeShaderFromFile(const std::string& computePath) {
    std::ifstream cFile(computePath);

    if (!cFile.is_open()) {
        std::cerr << "Failed to open compute shader file: " << computePath << std::endl;
        return Shader();
    }

    std::stringstream cStream;
    cStream << cFile.rdbuf();

    Shader shader;
    shader.m_RendererID = shader.CreateComputeShaderProgram(cStream.str());
    return shader;
}

Shader Shader::CreateComputeShaderFromSource(const std::string& computeSource) {
    Shader shader;
    shader.m_RendererID = shader.CreateComputeShaderProgram(computeSource);
    return shader;
}

std::string Shader::LoadShaderSource(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return "";
    }

    std::stringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

unsigned int Shader::CompileShader(unsigned int type, const std::string& source) {
    unsigned int id = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    int result;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
        char* message = (char*)alloca(length * sizeof(char));
        glGetShaderInfoLog(id, length, &length, message);
        std::cerr << "Failed to compile ";
        if (type == GL_VERTEX_SHADER) std::cerr << "vertex";
        else if (type == GL_FRAGMENT_SHADER) std::cerr << "fragment";
        else if (type == GL_COMPUTE_SHADER) std::cerr << "compute";
        std::cerr << " shader!" << std::endl << message << std::endl;
        glDeleteShader(id);
        return 0;
    }

    std::cout << "Shader compiled OK: ";
    if (type == GL_VERTEX_SHADER) std::cout << "vertex";
    else if (type == GL_FRAGMENT_SHADER) std::cout << "fragment";
    else if (type == GL_COMPUTE_SHADER) std::cout << "compute";
    std::cout << std::endl;
    return id;
}

unsigned int Shader::CreateShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc) {
    unsigned int program = glCreateProgram();
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glValidateProgram(program);

    glDetachShader(program, vs);
    glDeleteShader(vs);
    glDetachShader(program, fs);
    glDeleteShader(fs);

    return program;
}

unsigned int Shader::CreateComputeShaderProgram(const std::string& computeSrc) {
    unsigned int program = glCreateProgram();
    unsigned int cs = CompileShader(GL_COMPUTE_SHADER, computeSrc);

    glAttachShader(program, cs);
    glLinkProgram(program);

    // Check link status
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR: Compute shader link failed:\n" << infoLog << std::endl;
    }

    glValidateProgram(program);

    // Check validate status
    glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "ERROR: Compute shader validate failed:\n" << infoLog << std::endl;
    }

    glDetachShader(program, cs);
    glDeleteShader(cs);

    return program;
}

} // namespace cloth
