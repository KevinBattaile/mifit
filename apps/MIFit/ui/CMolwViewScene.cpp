#include <QOpenGLContext>
#include <QFont>
#include <math/Vector4.h>
#include <opengl/Axes.h>
#include <opengl/Camera.h>
#include <opengl/Frustum.h>
#include <opengl/ModernShader.h>
#include <opengl/OpenGL.h>
#include <opengl/QuatUtil.h>
#include <opengl/Sphere.h>
#include <opengl/Text.h>
#include <opengl/Viewport.h>
#include <opengl/ViewportRelativeViewpoint.h>
#include <opengl/interact/TargetFeedback.h>

#include "Displaylist.h"
#include "CMolwViewScene.h"
#include "Application.h"
#include "GLRenderer.h"
#include "core/ViewPoint.h"
#include "surf.h"
#include "ViewPointSettings.h"

using namespace mi::math;
using namespace mi::opengl::interact;
using namespace mi::opengl;

static void logOpenGLErrors(const char *file, int line)
{
    int error = glGetError();
    if (error != 0)
    {
        Logger::log("OpenGL error %d at line %d in %s", error, line, file);
    }
}

CMolwViewScene::CMolwViewScene()
    : pentamerModel(NULL),
      showUnitCell(true),
      showSymmetryAsBackbone(false),
      targetFeedbackSize(16.0f)
{

    renderer = new GLRenderer();

    float white[] = { 1.0f, 1.0f, 1.0f };
    targetFeedback = new TargetFeedback(white);

    topView = false;
    corners = new Vector3<float>[8];
    frontCamera = new Camera();
}

CMolwViewScene::~CMolwViewScene()
{
    delete renderer;
    delete targetFeedback;
    for (std::map<void*, mi::opengl::Axes*>::iterator i = axes.begin();
         i!=axes.end(); ++i)
    {
        delete i->second;
    }
    delete[] corners;
    delete frontCamera;
}

void CMolwViewScene::setViewport(Viewport *viewport)
{
    this->viewport = viewport;
}

void CMolwViewScene::initializeForRender()
{
    if (axes[renderer->getContext()] == NULL)
    {
        axes[renderer->getContext()] = new Axes(std::unique_ptr<mi::opengl::Text>(new Text(QFont("Helvetica", 12, QFont::Bold))));
        axes[renderer->getContext()]->setFontSize(10);
        axes[renderer->getContext()]->setLength(30);
    }

    glEnable(GL_DEPTH_TEST);
    ModernShader::setLightingEnabled(true);
    renderer->initializeForRender();
}

void CMolwViewScene::preCameraRender()
{
    renderer->clearPickNames();
    if (viewpointSettings->isDepthCuedColors())
    {
        float clipDistance = frustum->getFarClipping() - frustum->getNearClipping();
        float fogEnd = frustum->getFarClipping() + clipDistance * 0.25f;
        renderer->setFogStart(frustum->getNearClipping());
        renderer->setFogEnd(fogEnd);
        renderer->setFogEnabled(true);
    }
    else
    {
        renderer->setFogEnabled(false);
    }
    renderer->renderFog();

    renderer->renderLight();

}

void CMolwViewScene::setCorners(Vector3<float> *corners)
{
    for (int i = 0; i < 8; ++i)
    {
        this->corners[i].set(corners[i]);
    }
}

static void planeFrom3Points(const Vector3<float> &v1, const Vector3<float> &v2, const Vector3<float> &v3, double *plane)
{

    Vector3<float> aux1(v1);
    aux1.subtract(v2);
    Vector3<float> aux2(v3);
    aux2.subtract(v2);
    Vector3<float> normal;
    normal.cross(aux2, aux1);
    normal.normalize();
    float d = -(normal.dot(v2));
    plane[0] = normal.x;
    plane[1] = normal.y;
    plane[2] = normal.z;
    plane[3] = d;
}

void CMolwViewScene::render()
{

    logOpenGLErrors(__FILE__, __LINE__);

    ModernShader::disableClipPlanes();

    if (topView)
    {
        Quaternion<float> cameraOrientation = frontCamera->getRotation();
        Vector3<float> cameraEye = frontCamera->getEye();

        for (int i = 0; i < 8; ++i)
        {
            Vector3<float> &corner = corners[i];
            corner.z *= -1.0f;
            QuatUtil::rotateVector(cameraOrientation, corner);
            corner.add(cameraEye);
        }
        glLineWidth(1.0f);
        std::vector<ModernVertex> clipVerts(4);
        auto setV = [](ModernVertex &v, const Vector3<float> &p) {
            v = { p.x, p.y, p.z, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
        };
        setV(clipVerts[0], corners[2]);
        setV(clipVerts[1], corners[3]);
        setV(clipVerts[2], corners[6]);
        setV(clipVerts[3], corners[7]);
        bool prevLight = ModernShader::isLightingEnabled();
        ModernShader::setLightingEnabled(false);
        ModernShader::drawVertices(GL_LINES, clipVerts.data(), clipVerts.size());
        ModernShader::setLightingEnabled(prevLight);

        double plane[4];
        // Back plane
        planeFrom3Points(corners[7], corners[4], corners[5], plane);
        ModernShader::setClipPlane(0, true, QVector4D(plane[0], plane[1], plane[2], plane[3]));
        // Front plane
        planeFrom3Points(corners[1], corners[0], corners[2], plane);
        ModernShader::setClipPlane(1, true, QVector4D(plane[0], plane[1], plane[2], plane[3]));
    }
    logOpenGLErrors(__FILE__, __LINE__);


    RenderStyle renderStyle;
    if (viewpointSettings->GetBallandStick() == ViewPointSettings::STICKS)
    {
        renderStyle.set(RenderStyle::getLine());
        renderStyle.setBondLineWidth(viewpointSettings->GetLineThickness());
    }
    else if (viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDSTICK)
    {
        renderStyle.set(RenderStyle::getBallAndLine());
        renderStyle.setBallPercent(viewpointSettings->GetBallSize() / 100.0f);
        renderStyle.setBondLineWidth(viewpointSettings->GetLineThickness());
    }
    else if (viewpointSettings->GetBallandStick() == ViewPointSettings::BALLANDCYLINDER)
    {
        renderStyle.set(RenderStyle::getBallAndStick());
        renderStyle.setBallPercent(viewpointSettings->GetBallSize() / 100.0f);
        renderStyle.setStickPercent((viewpointSettings->GetBallSize() / 100.0f) * (viewpointSettings->GetCylinderSize() / 100.0f));
    }
    else if (viewpointSettings->GetBallandStick() == ViewPointSettings::CPK)
    {
        renderStyle.set(RenderStyle::getBall());
    }
    renderer->setRenderStyle(renderStyle);

    logOpenGLErrors(__FILE__, __LINE__);

    ModernShader::pushModelView();

    renderer->Draw2(models, true, true, showSymmetryAsBackbone);
    if (pentamerModel != NULL)
    {
        renderer->drawMolecule(pentamerModel);
    }

    if (ShowContacts)
    {
        renderer->DrawContacts(models->getContacts());
    }
    std::list<Molecule*>::iterator moleculeIter = models->getMolecules().begin();
    while (moleculeIter != models->getMolecules().end())
    {
        Molecule *molecule = *moleculeIter;
        ++moleculeIter;
        if (molecule->DotsVisible())
        {
            renderer->drawSurface(molecule->getDots());
        }
    }


    MIDrawSurfaces();
    renderer->DrawAnnotations(models->getMolecules());

    ModernShader::popModelView();
    logOpenGLErrors(__FILE__, __LINE__);

    Vector3<float> target(camera->getTarget(frustum->getFocalLength()));
    targetFeedback->setLength(targetFeedbackSize * glUnitsPerPixel);
    targetFeedback->setTarget(target);
    targetFeedback->render();

    if (showUnitCell)
    {
        bool prevLight = ModernShader::isLightingEnabled();
        ModernShader::setLightingEnabled(false);
        ModernShader::disableClipPlanes();
        ModernShader::pushModelView();

        Molecule *molecule = models->GetCurrentModel();
        if (molecule != NULL)
        {
            const CMapHeaderBase &mapHeader = molecule->GetMapHeader();
            float points[8][3] =
            {
                { 0.0f, 0.0f, 0.0f },
                { 1.0f, 0.0f, 0.0f },
                { 0.0f, 1.0f, 0.0f },
                { 0.0f, 0.0f, 1.0f },
                { 1.0f, 1.0f, 0.0f },
                { 1.0f, 0.0f, 1.0f },
                { 0.0f, 1.0f, 1.0f },
                { 1.0f, 1.0f, 1.0f }
            };
            for (int i = 0; i < 8; ++i)
            {
                float *point = points[i];
                mapHeader.FtoC(&point[0], &point[1], &point[2]);
            }
            const int edgePairs[12][2] = {
                {0, 1}, {0, 2}, {0, 3},
                {4, 1}, {5, 1},
                {4, 2}, {6, 2},
                {5, 3}, {6, 3},
                {7, 4}, {7, 5}, {7, 6}
            };
            std::vector<ModernVertex> cellVerts(24);
            for (int e = 0; e < 12; ++e)
            {
                for (int k = 0; k < 2; ++k)
                {
                    int idx = edgePairs[e][k];
                    cellVerts[e * 2 + k] = {
                        points[idx][0], points[idx][1], points[idx][2],
                        0.0f, 0.0f, 1.0f,
                        1.0f, 1.0f, 1.0f, 1.0f,
                        0.0f, 0.0f
                    };
                }
            }
            ModernShader::drawVertices(GL_LINES, cellVerts.data(), cellVerts.size());
            renderer->drawText("O", points[0][0], points[0][1], points[0][2]);
        }

        ModernShader::popModelView();
        ModernShader::setLightingEnabled(prevLight);
    }

    if (ShowGnomon && axes[renderer->getContext()] != NULL)
    {
        bool prevLight = ModernShader::isLightingEnabled();
        ModernShader::setLightingEnabled(false);
        ModernShader::disableClipPlanes();
        ModernShader::pushModelView();
        float margin = 40.0f;
        float axesX = margin - viewport->getWidth() / 2.0f;
        float axesY = viewport->getHeight() / 2.0f - margin;
        ViewportRelativeViewpoint axesViewpoint(camera);
        axesViewpoint.setPosition(viewport, frustum, axesX, axesY);
        axesViewpoint.render();
        axes[renderer->getContext()]->setGlUnitsPerPixel(glUnitsPerPixel);
        axes[renderer->getContext()]->render();
        ModernShader::popModelView();
        ModernShader::setLightingEnabled(prevLight);
    }

    glClear(GL_DEPTH_BUFFER_BIT);
    if (ShowLabels)
    {
        renderer->DrawLabels(models->getMolecules());
    }
    renderer->circleStackAtoms(atomStack);
    renderer->drawLines(torsionArrow, 4, false);

    ModernShader::disableClipPlanes();
}

void CMolwViewScene::renderOverlay()
{
}

float CMolwViewScene::getTargetSize()
{
    return targetFeedbackSize;
}

void CMolwViewScene::setTargetSize(float size)
{
    targetFeedbackSize = size;
}

