#include <opengl/Camera.h>

#include <opengl/QuatUtil.h>
#include <opengl/OpenGL.h>
#include <opengl/ModernShader.h>

using namespace mi::math;

namespace mi
{
namespace opengl
{

Camera::Camera()
    : Viewpoint()
{
}

Vector3<float> Camera::getEye()
{
    return getPosition();
}

void Camera::setEye(const Vector3<float> &eye)
{
    setPosition(eye);
}

void Camera::render()
{
    ModernShader::loadIdentityModelView();

    Quaternion<float> tmpQuat(getRotation());
    tmpQuat.inverse();
    float floatBuffer[16];
    QuatUtil::store(tmpQuat, floatBuffer);
    ModernShader::multModelView(floatBuffer);
    Vector3<float> eye = getPosition();
    ModernShader::translateModelView(-eye.x, -eye.y, -eye.z);
}

void Camera::lookAt(const Vector3<float> &target)
{
    Vector3<float> t(target);
    t.subtract(getPosition());
    Vector3<float> z(0.0f, 0.0f, -1.0f);
    Quaternion<float> q = QuatUtil::alignVectors(z, t);
    setRotation(q);
}

}
}
