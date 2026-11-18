#include <opengl/interact/TargetFeedback.h>

#include <opengl/OpenGL.h>
#include <opengl/ModernShader.h>

using namespace mi::math;
using namespace mi::opengl;

namespace mi
{
namespace opengl
{
namespace interact
{

TargetFeedback::TargetFeedback(float *color)
{
    this->color.set(color[0], color[1], color[2]);
}

void TargetFeedback::setLength(float length)
{
    this->length = length;
}

void TargetFeedback::setTarget(const Vector3<float> &target)
{
    this->target.set(target);
}

void TargetFeedback::render()
{
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);

    ModernShader::pushModelView();
    ModernShader::translateModelView(target.getX(), target.getY(), target.getZ());

    float margin = length / 3.0f;
    float r = color.getX(), g = color.getY(), b = color.getZ();
    ModernVertex verts[12] = {
        { -length, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        { -margin, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        {  margin, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        {  length, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },

        { 0.0f, -length, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        { 0.0f, -margin, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        { 0.0f,  margin, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        { 0.0f,  length, 0.0f,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },

        { 0.0f, 0.0f, -length,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f, -margin,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f,  margin,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f,  length,  0.0f, 0.0f, 1.0f,  r, g, b, 1.0f,  0.0f, 0.0f }
    };
    ModernShader::drawVertices(GL_LINES, verts, 12);

    ModernShader::popModelView();
    ModernShader::setLightingEnabled(prevLight);
}

}
}
}
