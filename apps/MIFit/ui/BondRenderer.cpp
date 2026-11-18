#include "BondRenderer.h"
#include <opengl/ModernShader.h>
#include <vector>

using namespace mi::opengl;

BondRenderer::BondRenderer()
{
}

void BondRenderer::addBond(const QVector3D &pos1, const QVector4D &color1, const QVector3D &pos2, const QVector4D &color2)
{
    if (color1 == color2)
    {
        vertices.append(pos1);
        colors.append(color1);
        vertices.append(pos2);
        colors.append(color2);
    }
    else
    {
        QVector3D midPoint = 0.5 * (pos1 + pos2);

        vertices.append(pos1);
        colors.append(color1);
        vertices.append(midPoint);
        colors.append(color1);
        vertices.append(midPoint);
        colors.append(color2);
        vertices.append(pos2);
        colors.append(color2);
    }
}

void BondRenderer::draw() const
{
    if (vertices.isEmpty())
        return;

    std::vector<ModernVertex> mv(vertices.size());
    for (int i = 0; i < vertices.size(); ++i)
    {
        const QVector3D &p = vertices[i];
        const QVector4D &c = colors[i];
        mv[i] = { p.x(), p.y(), p.z(), 0.0f, 0.0f, 1.0f, c.x(), c.y(), c.z(), c.w(), 0.0f, 0.0f };
    }
    ModernShader::drawVertices(GL_LINES, mv.data(), static_cast<int>(mv.size()));
}
