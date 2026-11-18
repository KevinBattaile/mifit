#include "Arc.h"

#include <opengl/OpenGL.h>
#include <opengl/ModernShader.h>
#include <cmath>
#include <vector>

using namespace mi::math;
using namespace std;

namespace mi
{
namespace opengl
{

Arc::Arc()
    : vertexBuffer(NULL),
      vertexIndex(0),
      divisions(6)
{
}

Arc::Arc(Vector3<float> &from, Vector3<float> &to, int divisions)
    : vertexBuffer(NULL),
      vertexIndex(0)
{

    this->from = from;
    this->to = to;
    this->divisions = divisions;

    createVertices();
}

Vector3<float> Arc::getFrom()
{
    return from;
}

void Arc::setFrom(Vector3<float> &from)
{
    if (from == this->from)
    {
        return;
    }
    this->from.set(from);
    clearVertices();
}

Vector3<float> Arc::getTo()
{
    return to;
}

void Arc::setTo(Vector3<float> &to)
{
    if (to == this->to)
    {
        return;
    }
    this->to.set(to);
    clearVertices();
}

int Arc::getDivisions()
{
    return divisions;
}

void Arc::setDivisions(int divisions)
{
    if (this->divisions == divisions)
    {
        return;
    }
    this->divisions = divisions;
    clearVertices();
}

void Arc::render()
{
    if (vertexBuffer == NULL)
    {
        createVertices();
    }
    if (vertexBuffer == NULL)
    {
        return;
    }
    int count = bufferSize / 3;
    std::vector<ModernVertex> verts(count);
    for (int i = 0; i < count; ++i)
    {
        verts[i].px = vertexBuffer[i * 3 + 0];
        verts[i].py = vertexBuffer[i * 3 + 1];
        verts[i].pz = vertexBuffer[i * 3 + 2];
        verts[i].nx = 0.0f; verts[i].ny = 0.0f; verts[i].nz = 1.0f;
        verts[i].r = 1.0f; verts[i].g = 0.0f; verts[i].b = 0.0f; verts[i].a = 1.0f;
        verts[i].u = 0.0f; verts[i].v = 0.0f;
    }
    ModernShader::drawVertices(GL_LINE_STRIP, verts.data(), count);
}

void Arc::clearVertices()
{
    if (vertexBuffer != NULL)
    {
        delete vertexBuffer;
    }
    vertexBuffer = NULL;
    vertexIndex = 0;
}

void Arc::createVertices()
{
    if (from == to)
    {
        return;
    }

    bufferSize = 2;
    bufferSize += ((int) pow(2.0f, divisions)) - 1;
    bufferSize *= 3;
    vertexBuffer = new float[bufferSize];
    vertexBuffer[vertexIndex] = from.x;
    vertexIndex++;
    vertexBuffer[vertexIndex] = from.y;
    vertexIndex++;
    vertexBuffer[vertexIndex] = from.z;
    vertexIndex++;
    bisect(from, to, divisions);
    vertexBuffer[vertexIndex] = to.x;
    vertexIndex++;
    vertexBuffer[vertexIndex] = to.y;
    vertexIndex++;
    vertexBuffer[vertexIndex] = to.z;
    vertexIndex++;

}

void Arc::bisect(Vector3<float> &v0, Vector3<float> &v1, int level)
{
    if (level <= 0)
    {
        return;
    }
    Vector3<float> result;
    result.add(v0, v1);
    float mag = result.length();
    if (mag < 1.0e-3)
    {
        result.set(0.0f, 0.0f, v0.length());
    }
    else
    {
        result.scale(v0.length() / mag);
    }
    bisect(v0, result, level - 1);
    vertexBuffer[vertexIndex] = result.x;
    vertexIndex++;
    vertexBuffer[vertexIndex] = result.y;
    vertexIndex++;
    vertexBuffer[vertexIndex] = result.z;
    vertexIndex++;
    bisect(result, v1, level - 1);
}

}
}

