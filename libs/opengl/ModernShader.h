#ifndef mi_opengl_ModernShader_h
#define mi_opengl_ModernShader_h

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <vector>
#include <stack>

namespace mi
{
namespace opengl
{

struct ModernVertex
{
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b, a;
    float u, v;
};

class ModernShader
{
public:
    static ModernShader *instance();

    // Matrix stack replacement for GL_PROJECTION and GL_MODELVIEW
    static void setProjectionMatrix(const QMatrix4x4 &proj);
    static QMatrix4x4 projectionMatrix();
    static void pushProjection();
    static void popProjection();

    static void setModelViewMatrix(const QMatrix4x4 &mv);
    static QMatrix4x4 modelViewMatrix();
    static void loadIdentityModelView();
    static void multModelView(const QMatrix4x4 &m);
    static void multModelView(const float *colMajor16);
    static void translateModelView(float x, float y, float z);
    static void scaleModelView(float x, float y, float z);
    static void pushModelView();
    static void popModelView();

    // Lighting state
    static void setLightingEnabled(bool enabled);
    static bool isLightingEnabled();
    static void setLight(const QVector4D &posEye, const QVector4D &ambient,
                         const QVector4D &diffuse, const QVector4D &specular);

    // Fog state
    static void setFog(bool enabled, float start, float end, const QVector4D &color);

    // Clip planes (world space or eye space)
    static void setClipPlane(int index, bool enabled, const QVector4D &eyePlane);
    static void disableClipPlanes();

    // Picking state
    static void setPickingMode(bool enabled);
    static bool isPickingMode();
    static void pushPickName(unsigned int name);
    static void popPickName();
    static void clearPickNames();
    static QVector4D currentPickColor();
    static void decodePickColor(unsigned char r, unsigned char g, unsigned char b,
                                unsigned int &primaryId, unsigned int &secondaryId);

    // Drawing batches with VAO / VBO / EBO
    static void drawVertices(GLenum mode, const ModernVertex *vertices, int count,
                             bool useTexture = false, GLuint textureId = 0);
    static void drawIndexed(GLenum mode, const ModernVertex *vertices, int vertexCount,
                            const unsigned int *indices, int indexCount,
                            bool useTexture = false, GLuint textureId = 0);

    // Cached unit sphere and unit cylinder drawn with VAO/VBO/EBO
    static void drawUnitSphere(const QVector3D &center, float radius, const QVector4D &color, int slices = 16, int stacks = 16);
    static void drawCylinder(const QVector3D &start, const QVector3D &end, float radius, const QVector4D &color);

private:
    ModernShader() = default;
};

} // namespace opengl
} // namespace mi

#endif // mi_opengl_ModernShader_h
