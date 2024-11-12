#pragma once

#include <glm/glm.hpp>
#include <string>
#include <array>
#include "Colour.h"
#include "TexturePoint.h"

struct ModelTriangle {
    std::array<glm::vec3, 3> vertices{};
    std::array<int, 3> vertexIndex{};
    std::array<TexturePoint, 3> texturePoints{};
    Colour colour{};
    glm::vec3 normal{};
    std::array<glm::vec3, 3> normals{};

    ModelTriangle();
    ModelTriangle(const glm::vec3 &v0, const glm::vec3 &v1, const glm::vec3 &v2, Colour trigColour);

    friend std::ostream &operator<<(std::ostream &os, const ModelTriangle &triangle);
};
