#include <opengl/interact/ArcBallFeedback.h>

#include <opengl/Arc.h>
#include <opengl/Circle.h>
#include <opengl/ModernShader.h>
#include <opengl/OpenGL.h>

using namespace mi::math;
using namespace mi::opengl;

namespace mi
{
namespace opengl
{
namespace interact
{

ArcBallFeedback::ArcBallFeedback(float radius, float *color)
{
    arc = new Arc();
    arcColor.set(color[0], color[1], color[2]);
    ballColor.set(arcColor);
    // Dim the ball circle
    ballColor.scale(0.8f);
    ball = new Circle(radius);
}

void ArcBallFeedback::render()
{
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glDisable(GL_DEPTH_TEST);

    ball->render();
    arc->render();

    glEnable(GL_DEPTH_TEST);
    ModernShader::setLightingEnabled(prevLight);
}

void ArcBallFeedback::setRadius(float radius)
{
    ball->setRadius(radius);
}

void ArcBallFeedback::setFrom(Vector3<float> &from)
{
    Vector3<float> v(from);
    v.scale(ball->getRadius());
    arc->setFrom(v);
}

void ArcBallFeedback::setTo(Vector3<float> &to)
{
    Vector3<float> v(to);
    v.scale(ball->getRadius());
    arc->setTo(v);
}

}
}
}

