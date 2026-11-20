#include <opengl/Axes.h>

#include <math/Matrix4.h>
#include <math/Quaternion.h>
#include <opengl/OpenGL.h>
#include <opengl/Text.h>
#include <opengl/ModernShader.h>
#include <QtCore/QString>

using namespace mi::math;

namespace mi
{
namespace opengl
{

int Axes::labelsFontSize = 16;

Axes::Axes(std::unique_ptr<Text> font)
    : labelsFont(std::move(font))
{

}

Axes::~Axes() = default;

void Axes::setFontSize(int fontSize)
{
    this->fontSize = fontSize;
}

void Axes::setLength(int length)
{
    this->length = length;
}

void Axes::setGlUnitsPerPixel(float glUnitsPerPixel)
{
    this->glUnitsPerPixel = glUnitsPerPixel;
}

static void getInverseRotation(float inverseRotation[16])
{
    static GLfloat modelview[16];
    memcpy(modelview, ModernShader::modelViewMatrix().constData(), 16 * sizeof(float));
    Matrix4<float> matrix(modelview);
    Quaternion<float> quat;
    quat.set(matrix);
    quat.negate();
    matrix.set(quat);
    matrix.getInColumnMajorOrder(inverseRotation);
}

void Axes::drawText(const char *text, float x, float y, float z, float offset, float scale, float inverseRotation[16])
{
    ModernShader::pushModelView();
    ModernShader::translateModelView(x, y, z);
    ModernShader::multModelView(inverseRotation);
    ModernShader::translateModelView(offset, offset, offset);
    ModernShader::scaleModelView(scale, scale, 1.0f);

    float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    labelsFont->renderText(0, 0, QString(text), white);

    ModernShader::popModelView();
}

void Axes::render()
{
    double textHeight = (double) fontSize * glUnitsPerPixel;
    float labelsTextScale = (float) (14.0f / (float) labelsFontSize * 0.1f * textHeight);
    float renderedLength = length * glUnitsPerPixel;

    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);

    glLineWidth(1.0f);
    ModernVertex lines[6] = {
        { 0.0f, 0.0f, 0.0f,           0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f },
        { renderedLength, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f,           0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, renderedLength, 0.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f,           0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  0.0f, 0.0f },
        { 0.0f, 0.0f, renderedLength, 0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f,  0.0f, 0.0f }
    };
    ModernShader::drawVertices(GL_LINES, lines, 6);

    float inverseRotation[16];
    getInverseRotation(inverseRotation);
    drawText(" X ", renderedLength, 0.0f, 0.0f, 0.0f, labelsTextScale, inverseRotation);
    drawText(" Y ", 0.0f, renderedLength, 0.0f, 0.0f, labelsTextScale, inverseRotation);
    drawText(" Z ", 0.0f, 0.0f, renderedLength, 0.0f, labelsTextScale, inverseRotation);

    ModernShader::setLightingEnabled(prevLight);
}

}
}
