#include <opengl/interact/TranslationFeedback.h>

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

TranslationFeedback::TranslationFeedback(float *color)
{
    this->color.set(color[0], color[1], color[2]);
}

void TranslationFeedback::setLength(float length)
{
    this->length = length;
}

void TranslationFeedback::render()
{
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);

    float r = color.getX(), g = color.getY(), b = color.getZ();
    ModernVertex seg[2] = {
        { beginPosition.getX(), beginPosition.getY(), beginPosition.getZ(), 0.0f, 0.0f, 1.0f, r, g, b, 1.0f, 0.0f, 0.0f },
        { endPosition.getX(),   endPosition.getY(),   endPosition.getZ(),   0.0f, 0.0f, 1.0f, r, g, b, 1.0f, 0.0f, 0.0f }
    };
    ModernShader::drawVertices(GL_LINES, seg, 2);

    ModernShader::pushModelView();
    ModernShader::translateModelView(beginPosition.getX(), beginPosition.getY(), beginPosition.getZ());

    float margin = length / 3.0f;
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

void TranslationFeedback::setFrom(const Vector3<float> &from)
{
    beginPosition.set(from);
}

void TranslationFeedback::setTo(const Vector3<float> &to)
{
    endPosition.set(to);
}

}
}
}
