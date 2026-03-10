#pragma once

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace cloth {

class Shader {
public:
    Shader() : m_RendererID(0) {}
    Shader(const std::string& vertexSrc, const std::string& fragmentSrc);
    ~Shader();

    void Bind() const;
    void Unbind() const;

    // Uniform setters
    void SetBool(const std::string& name, bool value) const;
    void SetInt(const std::string& name, int value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetVec2(const std::string& name, const glm::vec2& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetMat4(const std::string& name, const glm::mat4& value) const;

    unsigned int GetID() const { return m_RendererID; }

    static Shader CreateFromFile(const std::string& vertexPath, const std::string& fragmentPath);

private:
    unsigned int m_RendererID;
    mutable std::unordered_map<std::string, int> m_UniformLocationCache;

    int GetUniformLocation(const std::string& name) const;
    unsigned int CompileShader(unsigned int type, const std::string& source);
    unsigned int CreateShaderProgram(const std::string& vertexSrc, const std::string& fragmentSrc);
};

} // namespace cloth
