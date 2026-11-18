#include <opengl/Circle.h>

#include <opengl/OpenGL.h>
#include <opengl/ModernShader.h>
#include <math/mathlib.h>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace mi
{
namespace opengl
{

Circle::Circle(float radius)
{
    setRadius(radius);
    setAngle((float) (2.0 * M_PI));
    setDetail(80);
}

Circle::Circle(float radius, float angle)
{
    setRadius(radius);
    setAngle(angle);
    setDetail(80);
}

float Circle::getRadius()
{
    return radius;
}

void Circle::setRadius(float radius)
{
    radius = std::abs(radius);
    if (this->radius != radius)
    {
        this->radius = radius;
    }
}

int Circle::getDetail()
{
    return detail;
}

void Circle::setDetail(int detail)
{
    detail = std::abs(detail);
    if (detail < 6)
    {
        detail = 6;
    }
    if (detail != this->detail)
    {
        this->detail = detail;
    }
}

float Circle::getAngle()
{
    return angle;
}

void Circle::setAngle(float angle)
{
    if (this->angle != angle)
    {
        this->angle = angle;
    }
}

void Circle::render()
{
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);

    std::vector<ModernVertex> verts(detail + 1);
    for (int i = 0; i <= detail; i++)
    {
        float x = radius * (float) std::cos(i * angle / detail);
        float y = radius * (float) std::sin(i * angle / detail);
        verts[i] = { x, y, 0.0f, 0.0f, 0.0f, 1.0f, 0.8f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
    }
    ModernShader::drawVertices(GL_LINE_STRIP, verts.data(), static_cast<int>(verts.size()));

    ModernShader::setLightingEnabled(prevLight);
}

}
}
