#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader {
public:
    unsigned int ID;

    Shader();
    ~Shader();

    // Loads a standard Vertex + Fragment graphics pipeline
    bool loadGraphics(const std::string& vertexPath, const std::string& fragmentPath);

    // Loads a Compute Shader pipeline
    bool loadCompute(const std::string& computePath);

    // Activates the shader program
    void use() const;

    // Uniform setters
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setUInt(const std::string& name, unsigned int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setVec2(const std::string& name, float x, float y) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setVec4(const std::string& name, float x, float y, float z, float w) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

private:
    bool checkCompileErrors(unsigned int shader, const std::string& type);
    std::string readFile(const std::string& path);
};
