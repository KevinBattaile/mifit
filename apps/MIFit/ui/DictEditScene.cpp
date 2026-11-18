#include <math/Vector4.h>
#include <opengl/Camera.h>
#include <opengl/Frustum.h>
#include <opengl/Light.h>
#include <opengl/ModernShader.h>
#include <opengl/OpenGL.h>
#include <opengl/Sphere.h>
#include <opengl/Viewpoint.h>
#include <opengl/Viewport.h>
#include <opengl/interact/ArcBallFeedback.h>
#include <opengl/interact/MouseArcBallOrbitor.h>
#include <opengl/interact/MouseTranslator.h>
#include <opengl/interact/TargetFeedback.h>

#include "DictEditScene.h"
#include "Displaylist.h"
#include "GLRenderer.h"

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

DictEditScene::DictEditScene()
    : showAtomLabels(true),
      pickedBond(NULL),
      pickedAngle(NULL),
      pickedPlane(NULL)
{

    light = new Light();
    light->setAmbientColor(0.6f, 0.6f, 0.6f, 1.0f);
    light->setSpecularColor(1.0f, 1.0f, 1.0f, 1.0f);
    Vector4<float> lightPosition(0.0f, 20.0f, 10.0f, 0.0f);
    light->setPosition(lightPosition);

    renderer = new GLRenderer();
    renderer->setShowBondOrders(true);

    float red[] = { 1.0f, 0.0f, 0.0f };
    arcBallFeedback = new ArcBallFeedback(1.0f, red);
    targetFeedback = new TargetFeedback(red);

}

DictEditScene::~DictEditScene()
{
    delete renderer;
    delete light;
    delete arcBallFeedback;
    delete targetFeedback;
}

void DictEditScene::setViewport(Viewport *viewport)
{
    this->viewport = viewport;
}

void DictEditScene::initializeForRender()
{
    glEnable(GL_DEPTH_TEST);
    ModernShader::setLightingEnabled(true);
    renderer->initializeForRender();
}

void DictEditScene::preCameraRender()
{
    renderer->setFogEnabled(true);
    renderer->renderFog();

    light->glEnable();
    light->render();

}

void DictEditScene::render()
{

    logOpenGLErrors(__FILE__, __LINE__);

    ModernShader::pushModelView();

    renderer->Draw2(models, true, true);
    if (showAtomLabels)
    {
        renderer->DrawLabels(models->getMolecules());
    }
    renderer->DrawAnnotations(models->getMolecules());
    if (pickedBond != NULL)
    {
        bool prevLight = ModernShader::isLightingEnabled();
        ModernShader::setLightingEnabled(false);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
        float *col = renderer->getColor(Colors::GREEN);
        ModernVertex v[2] = {
            { pickedBond->getAtom1()->x(), pickedBond->getAtom1()->y(), pickedBond->getAtom1()->z(), 0.0f, 0.0f, 1.0f, col[0], col[1], col[2], 1.0f, 0.0f, 0.0f },
            { pickedBond->getAtom2()->x(), pickedBond->getAtom2()->y(), pickedBond->getAtom2()->z(), 0.0f, 0.0f, 1.0f, col[0], col[1], col[2], 1.0f, 0.0f, 0.0f }
        };

        ModernShader::drawVertices(GL_LINES, v, 2);
        glEnable(GL_DEPTH_TEST);
        ModernShader::setLightingEnabled(prevLight);
    }
    if (pickedAngle != NULL)
    {
        bool prevLight = ModernShader::isLightingEnabled();
        ModernShader::setLightingEnabled(false);
        glDisable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
        float *col = renderer->getColor(7);
        ModernVertex v[2] = {
            { pickedAngle->getAtom1()->x(), pickedAngle->getAtom1()->y(), pickedAngle->getAtom1()->z(), 0.0f, 0.0f, 1.0f, col[0], col[1], col[2], 1.0f, 0.0f, 0.0f },
            { pickedAngle->getAtom3()->x(), pickedAngle->getAtom3()->y(), pickedAngle->getAtom3()->z(), 0.0f, 0.0f, 1.0f, col[0], col[1], col[2], 1.0f, 0.0f, 0.0f }
        };

        ModernShader::drawVertices(GL_LINES, v, 2);
        glEnable(GL_DEPTH_TEST);
        ModernShader::setLightingEnabled(prevLight);
    }
    logOpenGLErrors(__FILE__, __LINE__);

    if (pickedPlane != NULL)
    {
        renderer->CircleAtoms(pickedPlane->atoms, pickedPlane->natoms);
    }
    logOpenGLErrors(__FILE__, __LINE__);

    renderer->circleStackAtoms(atomStack);
    logOpenGLErrors(__FILE__, __LINE__);

    ModernShader::popModelView();

    if (cameraMouseOrbitor->getStartVector() != NULL || mouseTranslator->getStartPosition() != NULL)
    {
        Vector3<float> target(camera->getTarget(frustum->getFocalLength()));
        targetFeedback->setLength(16.0f * glUnitsPerPixel);
        targetFeedback->setTarget(target);
        targetFeedback->render();
    }

}

void DictEditScene::renderOverlay()
{
    Vector3<float> *from = NULL;
    Vector3<float> *to = NULL;
    if (cameraMouseOrbitor->getStartVector() != NULL)
    {
        from = cameraMouseOrbitor->getStartVector();
        to = cameraMouseOrbitor->getCurrentVector();
    }

    if (from != NULL)
    {
        float radius = std::min(viewport->getWidth(), viewport->getHeight()) / 2.0f;
        arcBallFeedback->setRadius(radius);
        arcBallFeedback->setFrom(*from);
        arcBallFeedback->setTo(*to);
        arcBallFeedback->render();
    }
}
