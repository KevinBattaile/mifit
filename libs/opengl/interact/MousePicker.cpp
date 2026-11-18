#include <opengl/interact/MousePicker.h>

#include <opengl/Frustum.h>
#include <opengl/Renderable.h>
#include <opengl/ModernShader.h>
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <climits>

namespace mi
{
namespace opengl
{
namespace interact
{

MousePicker::MousePicker()
{
    selectBufferLength = 1024;
    selectBuffer = new GLuint[selectBufferLength];
}

MousePicker::~MousePicker()
{
    delete[] selectBuffer;
}

std::vector<GLuint> MousePicker::pick(int x, int y, Frustum *frustum, Renderable *scene)
{
    std::vector<GLuint> selectedIds;
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return selectedIds;

    GLint vp[4] = { 0, 0, 0, 0 };
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2];
    int h = vp[3];
    if (w <= 0 || h <= 0)
        return selectedIds;

    QOpenGLFramebufferObjectFormat fboFormat;
    fboFormat.setAttachment(QOpenGLFramebufferObject::Depth);
    QOpenGLFramebufferObject fbo(w, h, fboFormat);
    if (!fbo.isValid() || !fbo.bind())
        return selectedIds;

    ModernShader::pushProjection();
    ModernShader::pushModelView();

    ModernShader::setPickingMode(true);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    scene->render();

    int glX = x;
    int glY = h - y;
    const int radius = 3;
    unsigned int bestPrimary = 0;
    unsigned int bestSecondary = 0;
    int bestDistSq = INT_MAX;

    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            int px = glX + dx;
            int py = glY + dy;
            if (px < 0 || px >= w || py < 0 || py >= h)
                continue;

            unsigned char pixel[4] = { 0, 0, 0, 0 };
            glReadPixels(px, py, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            unsigned int primaryId = 0;
            unsigned int secondaryId = 0;
            ModernShader::decodePickColor(pixel[0], pixel[1], pixel[2], primaryId, secondaryId);
            if (primaryId != 0)
            {
                int distSq = dx * dx + dy * dy;
                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestPrimary = primaryId;
                    bestSecondary = secondaryId;
                }
            }
        }
    }

    if (bestPrimary != 0)
    {
        selectedIds.push_back(bestPrimary);
        if (bestSecondary != 0)
            selectedIds.push_back(bestSecondary);
    }

    ModernShader::setPickingMode(false);
    ModernShader::popModelView();
    ModernShader::popProjection();

    fbo.release();

    return selectedIds;
}

}
}
}
