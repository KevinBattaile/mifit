#include <cmath>
#include <math/Point3.h>
#include <opengl/Camera.h>
#include <opengl/Frustum.h>
#include <opengl/ModernShader.h>
#include <opengl/OpenGL.h>
#include <opengl/QuatUtil.h>
#include <opengl/Viewport.h>

#include "core/corelib.h"
#include <chemlib/Residue.h>

#include "GLOverviewCanvas.h"
#include "Displaylist.h"
#include "MIMainWindow.h"
#include "MIGLWidget.h"

#include <QMouseEvent>
#include <QKeyEvent>

using namespace chemlib;
using namespace mi::math;
using namespace mi::opengl;
using namespace std;

class MIGLWidget;
static MIGLWidget *GetView()
{
    return MIMainWindow::instance()->currentMIGLWidget();
}


static void findCenter(std::vector<Bond> &bonds, Vector3<float> &min, Vector3<float> &max, Vector3<float> &center)
{
    min.set((float) INT_MAX, (float) INT_MAX, (float) INT_MAX);
    max.set((float) INT_MIN, (float) INT_MIN, (float) INT_MIN);
    std::vector<Bond>::iterator bondIter = bonds.begin();
    while (bondIter != bonds.end())
    {
        MIAtom *a1 = (*bondIter).getAtom1();
        MIAtom *a2 = (*bondIter).getAtom2();
        bondIter++;
        if (a1 != NULL)
        {
            min.x = std::min(min.x, a1->x());
            min.y = std::min(min.y, a1->y());
            min.z = std::min(min.z, a1->z());
            max.x = std::max(max.x, a1->x());
            max.y = std::max(max.y, a1->y());
            max.z = std::max(max.z, a1->z());
        }
        if (a2 != NULL)
        {
            min.x = std::min(min.x, a2->x());
            min.y = std::min(min.y, a2->y());
            min.z = std::min(min.z, a2->z());
            max.x = std::max(max.x, a2->x());
            max.y = std::max(max.y, a2->y());
            max.z = std::max(max.z, a2->z());
        }
    }
    center.set(max);
    center.add(min);
    center.scale(0.5f);
}

GLOverviewCanvas::GLOverviewCanvas(QWidget *parent)
    : QOpenGLWidget(parent)
{

    viewport = new Viewport();
    frustum = new Frustum();
    camera = new Camera();

    viewpointCenterMarkerRadius = 3.0f;
    selectedResidueMarkerRadius = 2.0f;
    selectedResidueMarkerResolution = 8.0f;
    //initializeForRendering();
}

GLOverviewCanvas::~GLOverviewCanvas()
{
}


void GLOverviewCanvas::initializeGL()
{
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0, 0.0, 0.0, 0.0);
}

void GLOverviewCanvas::resizeGL(int width, int height)
{
    viewport->set(0, 0, width, height);
    glViewport(0, 0, width, height);
    //render();
}

void GLOverviewCanvas::setIdentity(float rotation[])
{
    rotation[0] = 1.0f;
    rotation[1] = 0.0f;
    rotation[2] = 0.0f;
    rotation[3] = 0.0f;

    rotation[4] = 0.0f;
    rotation[5] = 1.0f;
    rotation[6] = 0.0f;
    rotation[7] = 0.0f;

    rotation[8] = 0.0f;
    rotation[9] = 0.0f;
    rotation[10] = 1.0f;
    rotation[11] = 0.0f;

    rotation[12] = 0.0f;
    rotation[13] = 0.0f;
    rotation[14] = 0.0f;
    rotation[15] = 1.0f;
}

void GLOverviewCanvas::createAlphaCarbonTrace()
{
    if (!GetView())
        return;

    std::vector<chemlib::Bond>().swap(bonds); // was bonds.clear();
    Displaylist *displaylist = GetView()->GetDisplaylist();
    if (displaylist == NULL)
    {
        return;
    }
    list<Molecule*>::iterator molIter = displaylist->begin();
    list<Molecule*>::iterator molIterEnd = displaylist->end();
    while (molIter != molIterEnd)
    {
        BuildCALinks(bonds, (*molIter)->residuesBegin());
        molIter++;
    }

    findCenter(bonds, min, max, center);

    Point3<float> minPos(min);
    Point3<float> maxPos(max);
    float maxDistance = minPos.distance(maxPos) / 2.0f;

    float fieldOfView = 40.0f;
    double cameraDistance = maxDistance * 2.0 / (2.0 *  tan(toRadians(fieldOfView) * 0.5));
    cameraOffset.set(0.0f, 0.0f, -cameraDistance);
    frustum->setFieldOfView(fieldOfView);
    frustum->setPerspective(false);
    frustum->setFocalLength(cameraDistance);
    frustum->setNearClipping(0.01f);
    frustum->setFarClipping(3.0f * cameraDistance);
}

void GLOverviewCanvas::paintGL()
{
    if (!isVisible())
        return;

    if (GetView() == NULL)
        return;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ModernShader::setLightingEnabled(false);

    createAlphaCarbonTrace();

    ViewPoint *viewpoint = GetView()->viewpoint;

    Quaternion<float> viewmat(viewpoint->orientation());

    Vector3<float> target(viewpoint->center()[0], viewpoint->center()[1], viewpoint->center()[2]);
    Vector3<float> eye(target);
    eye.add(cameraOffset);
    camera->setEye(eye);
    camera->lookAt(target);

    QVector4D fogColor(0.1f, 0.1f, 0.1f, 1.0f);
    ModernShader::setFog(true, 0.5f * frustum->getFocalLength(), 1.2f * frustum->getFocalLength(), fogColor);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    ModernShader::loadIdentityModelView();

    viewport->render();
    frustum->render(*viewport);

    camera->render();

    ModernShader::pushModelView();

    ModernShader::translateModelView(viewpoint->center()[0], viewpoint->center()[1], viewpoint->center()[2]);

    float rotation[16];
    QuatUtil::store(viewmat, rotation);
    ModernShader::multModelView(rotation);

    ModernShader::translateModelView(-viewpoint->center()[0], -viewpoint->center()[1], -viewpoint->center()[2]);

    const Residue *res = GetView()->getFocusResidue();
    MIAtom *focusedResidueAlphaCarbon = NULL;
    if (res != NULL)
        focusedResidueAlphaCarbon = atom_from_name("CA", *res);

    std::vector<ModernVertex> lineVerts;
    lineVerts.reserve(bonds.size() * 2);

    vector<Bond>::iterator bondIter = bonds.begin();
    while (bondIter != bonds.end())
    {
        MIAtom *a1 = (*bondIter).getAtom1();
        MIAtom *a2 = (*bondIter).getAtom2();
        int c1 = ::getColor(a1);
        float *col = getColor(c1);
        ModernVertex v1 = { a1->x(), a1->y(), a1->z(), 0.0f, 0.0f, 1.0f, col[0], col[1], col[2], 1.0f, 0.0f, 0.0f };
        ModernVertex v2 = { a2->x(), a2->y(), a2->z(), 0.0f, 0.0f, 1.0f, col[0], col[1], col[2], 1.0f, 0.0f, 0.0f };

        lineVerts.push_back(v1);
        lineVerts.push_back(v2);

        // mark the focusres CA
        if (a1 == focusedResidueAlphaCarbon)
        {
            drawSelectedResidueMarker(a1->x(), a1->y(), a1->z());
        }
        else if (a2 == focusedResidueAlphaCarbon)
        {
            drawSelectedResidueMarker(a2->x(), a2->y(), a2->z());
        }
        bondIter++;
    }
    if (!lineVerts.empty())
    {
        ModernShader::drawVertices(GL_LINES, lineVerts.data(), lineVerts.size());
    }

    ModernShader::popModelView();

    ModernShader::pushModelView();

    // Draw bounding box of view
    ModernShader::translateModelView(viewpoint->center()[0], viewpoint->center()[1], viewpoint->center()[2]);

    // Disable depth test to render on top of other elements.
    glDisable(GL_DEPTH_TEST);
    ModernShader::setFog(false, 0.5f * frustum->getFocalLength(), 1.2f * frustum->getFocalLength(), fogColor);

    float viewWidth = viewpoint->getwidth()/2.0f;
    float viewHeight = viewpoint->getheight()/2.0f;
    auto makeWhiteV = [](float x, float y, float z = 0.0f) {
        ModernVertex v = { x, y, z, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f };
        return v;
    };
    ModernVertex boxVerts[12] = {
        makeWhiteV(0.0f, -viewpointCenterMarkerRadius),
        makeWhiteV(0.0f, viewpointCenterMarkerRadius),
        makeWhiteV(-viewpointCenterMarkerRadius, 0.0f),
        makeWhiteV(viewpointCenterMarkerRadius, 0.0f),
        makeWhiteV(-viewWidth, -viewHeight),
        makeWhiteV(viewWidth, -viewHeight),
        makeWhiteV(-viewWidth, viewHeight),
        makeWhiteV(viewWidth, viewHeight),
        makeWhiteV(-viewWidth, -viewHeight),
        makeWhiteV(-viewWidth, viewHeight),
        makeWhiteV(viewWidth, -viewHeight),
        makeWhiteV(viewWidth, viewHeight)
    };
    ModernShader::drawVertices(GL_LINES, boxVerts, 12);

    glEnable(GL_DEPTH_TEST);

    ModernShader::popModelView();

    glFlush();
}

float*GLOverviewCanvas::getColor(int colorIndex)
{
    static float color[3];
    int c = PaletteIndex(colorIndex);
    color[0] = Colors::RPallette[c] / 255.0f;
    color[1] = Colors::GPallette[c] / 255.0f;
    color[2] = Colors::BPallette[c] / 255.0f;
    return color;
}

void GLOverviewCanvas::drawSelectedResidueMarker(float x, float y, float z)
{
    ModernShader::drawUnitSphere(QVector3D(x, y, z), selectedResidueMarkerRadius, QVector4D(1.0f, 1.0f, 1.0f, 1.0f),
                                 (int)selectedResidueMarkerResolution, (int)selectedResidueMarkerResolution);
}

void GLOverviewCanvas::mousePressEvent(QMouseEvent *e)
{
    e->accept();
    if (!GetView())
        return;
    GetView()->OnMousePress(e->pos(), e->buttons(), e->modifiers());
}
void GLOverviewCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    e->accept();
    if (!GetView())
        return;
    GetView()->OnMouseRelease(e->pos(), e->buttons(), e->modifiers());
}
void GLOverviewCanvas::mouseMoveEvent(QMouseEvent *e)
{
    e->accept();
    if (!GetView())
        return;
    GetView()->OnMouseMove(e->pos(), e->buttons(), e->modifiers());
}
void GLOverviewCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    e->accept();
    if (!GetView())
        return;
    QCoreApplication::sendEvent(GetView(), e);
}

void GLOverviewCanvas::keyPressEvent(QKeyEvent *e)
{
    e->accept();

    unsigned short nChar;
    QString s = e->text();
    if (s.size()==0)
    {
        return;
    }
    nChar = s[0].toLatin1();

    bool shift = (e->modifiers() & Qt::ShiftModifier);
    if (nChar <= 256 && isupper(nChar) && !shift)
    {
        nChar = tolower(nChar);
    }

    if (nChar == 186 && !shift)
    {
        nChar = ';';
    }
    if (nChar == 186 && shift)
    {
        nChar = ':';
    }
    if (nChar == 188 && !shift)
    {
        nChar = ',';
    }
    if (nChar == 188 && shift)
    {
        nChar = '<';
    }
    if (nChar == 190 && !shift)
    {
        nChar = '.';
    }
    if (nChar == 190 && shift)
    {
        nChar = '>';
    }
    if (nChar == 219 && !shift)
    {
        nChar = '[';
    }
    if (nChar == 219 && shift)
    {
        nChar = '{';
    }
    if (nChar == 221 && !shift)
    {
        nChar = ']';
    }
    if (nChar == 221 && shift)
    {
        nChar = '}';
    }

    GetView()->OnKeyDown(nChar, e->modifiers());
}

QSize GLOverviewCanvas::sizeHint() const
{
    return QSize(200, 200);
}
