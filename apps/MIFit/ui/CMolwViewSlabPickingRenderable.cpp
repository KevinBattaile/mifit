#include <chemlib/chemlib.h>
#include "CMolwViewSlabPickingRenderable.h"

#include <opengl/Camera.h>
#include <opengl/Frustum.h>
#include <opengl/ModernShader.h>
#include <opengl/OpenGL.h>
#include <opengl/QuatUtil.h>
#include <opengl/StereoView.h>
#include <opengl/Sphere.h>
#include <vector>

using namespace chemlib;
using namespace mi::math;
using namespace mi::opengl;

CMolwViewSlabPickingRenderable::CMolwViewSlabPickingRenderable(StereoView *stereoView)
    : stereoView(stereoView)
{

    slabRenderable = new CMolwViewSlabRenderable();

}

CMolwViewSlabPickingRenderable::~CMolwViewSlabPickingRenderable()
{
    delete slabRenderable;
}

void CMolwViewSlabPickingRenderable::render()
{
    stereoView->render(*slabRenderable);
}

void CMolwViewSlabPickingRenderable::setCorners(Vector3<float> *corners)
{
    slabRenderable->setCorners(corners);
}

void CMolwViewSlabPickingRenderable::setViewpoint(const Viewpoint &viewpoint)
{
    slabRenderable->setViewpoint(viewpoint);
}

CMolwViewSlabRenderable::CMolwViewSlabRenderable()
{
    viewpoint = new Viewpoint();
    corners = new Vector3<float>[8];
}

CMolwViewSlabRenderable::~CMolwViewSlabRenderable()
{
    delete viewpoint;
    delete corners;
}

void CMolwViewSlabRenderable::setCorners(Vector3<float> *corners)
{
    for (int i = 0; i < 8; ++i)
    {
        this->corners[i].set(corners[i]);
    }
}

void CMolwViewSlabRenderable::setViewpoint(const Viewpoint &viewpoint)
{
    this->viewpoint->set(viewpoint);
}

void CMolwViewSlabRenderable::render()
{

    Quaternion<float> orientation = viewpoint->getRotation();
    Vector3<float> position = viewpoint->getPosition();

    for (int i = 0; i < 8; ++i)
    {
        Vector3<float> &corner = corners[i];
        corner.z *= -1.0f;
        QuatUtil::rotateVector(orientation, corner);
        corner.add(position);
    }

    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glDisable(GL_DEPTH_TEST);

    glLineWidth(1.0f);
    auto setV = [](ModernVertex &v, const Vector3<float> &p) {
        v = { p.x, p.y, p.z, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
    };

    std::vector<ModernVertex> v1(4);
    setV(v1[0], corners[0]);
    setV(v1[1], corners[1]);
    setV(v1[2], corners[2]);
    setV(v1[3], corners[3]);

    ModernShader::pushPickName(1);
    ModernShader::drawVertices(GL_LINES, v1.data(), v1.size());
    ModernShader::popPickName();

    std::vector<ModernVertex> v2(4);
    setV(v2[0], corners[4]);
    setV(v2[1], corners[5]);
    setV(v2[2], corners[6]);
    setV(v2[3], corners[7]);

    ModernShader::pushPickName(2);
    ModernShader::drawVertices(GL_LINES, v2.data(), v2.size());
    ModernShader::popPickName();

    glEnable(GL_DEPTH_TEST);
    ModernShader::setLightingEnabled(prevLight);
}

