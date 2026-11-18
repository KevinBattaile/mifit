#include <math/Matrix4.h>
#include <math/Vector3.h>
#include <math/Vector4.h>
#include <opengl/Camera.h>
#include <opengl/Frustum.h>
#include <opengl/Light.h>
#include <opengl/OpenGL.h>
#include <opengl/Text.h>
#include <opengl/ModernShader.h>

#include <map/maplib.h>
#include "core/corelib.h"
#include <chemlib/Monomer.h>
#include "GLRenderer.h"
#include "BondRenderer.h"
#include "EMap.h"
#include <climits>
#include <vector>
#undef X
#undef Y
#undef Z

#include "Application.h"
#include "Displaylist.h"

#include <QOpenGLWidget>
#include <QPainter>


using namespace chemlib;
using namespace mi::math;
using namespace mi::opengl;
using namespace std;

static const bool useClockwiseFrontFaces = false;
static float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

static void logOpenGLErrors(const char *file, int line)
{
    int error = glGetError();
    if (error != 0)
    {
        Logger::log("OpenGL error %s (%d) at line %d in %s", gluErrorString(error), error, line, file);
    }
}

class PushedName
{
    bool enabled;
public:
    PushedName(GLuint name, bool enabled) : enabled(enabled)
    {
        if (enabled)
            ModernShader::pushPickName(name);
    }

    ~PushedName()
    {
        if (enabled)
            ModernShader::popPickName();
    }
};


GLRenderer::GLRenderer()
    :  pickingEnabled(false),
      qcanvas_win(NULL),
      hideHydrogens(false),
      light(NULL),
      light2(NULL),
      frustum(NULL),
      camera(NULL),
      currentModel(true),
      fogEnabled(false),
      antialiasLines(true),
      joinBondsOfSameColor(true),
      showBondOrders(false),
      fontSize(ATOMLABEL::defaultSize()),
      viewVectorSet(false)
{

    style.set(RenderStyle::getBallAndStick());

    light = new Light(GL_LIGHT0);
    light->setAmbientColor(0.6f, 0.6f, 0.6f, 1.0f);
    light->setSpecularColor(1.0f, 1.0f, 1.0f, 1.0f);
    Vector4<float> lightPosition(0.0f, 20.0f, 10.0f, 0.0f);
    light->setPosition(lightPosition);

    light2 = new Light(GL_LIGHT1);
    light2->setAmbientColor(0.1f, 0.1f, 0.1f, 1.0f);
    light2->setDiffuseColor(0.0f, 0.0f, 0.0f, 1.0f);
    light2->setSpecularColor(0.0f, 0.0f, 0.0f, 1.0f);
    Vector4<float> lightPosition2(0.0f, 150.0f, 10.0f, 0.0f);
    light2->setPosition(lightPosition2);

    currentColor[0] = 1.0f;
    currentColor[1] = 1.0f;
    currentColor[2] = 1.0f;
    currentColor[3] = 1.0f;
}

GLRenderer::~GLRenderer()
{
    delete light;
    delete light2;
}

void GLRenderer::setViewVector(const Vector3<float> &viewVector)
{
    this->viewVector.set(viewVector);
    viewVectorSet = true;
}

void GLRenderer::updateTextScale(float glUnitsPerPixel)
{
    fontSize = ATOMLABEL::defaultSize();
    double textHeight = (double) fontSize * glUnitsPerPixel;
    labelsTextScale = (float) fontSize / 16 * 0.1 * textHeight;
}

void GLRenderer::renderFog()
{
    if (fogEnabled)
    {
        ModernShader::setFog(true, fogStart, fogEnd, QVector4D(0.2f, 0.2f, 0.2f, 1.0f));
    }
    else
    {
        ModernShader::setFog(false, fogStart, fogEnd, QVector4D(0.2f, 0.2f, 0.2f, 1.0f));
    }
}

void GLRenderer::initializeForRender()
{
    glEnable(GL_DEPTH_TEST);
    if (useClockwiseFrontFaces)
    {
        glFrontFace(GL_CW);
    }
    glEnable(GL_CULL_FACE);
    ModernShader::setLightingEnabled(true);
}

void GLRenderer::renderLight()
{
    light->glEnable();
    light->render();
}


void GLRenderer::setQOpenGLWidget(QOpenGLWidget *qcanvas_win)
{
    this->qcanvas_win = qcanvas_win;
    labelsFont_.reset(new mi::opengl::Text(QFont("Helvetica", 12, QFont::Bold)));
}

void*GLRenderer::getContext()
{
    if (!qcanvas_win)
        return 0;
    return (void*)qcanvas_win->context();
}

void GLRenderer::swapBuffers()
{
    // QOpenGLWidget handles buffer swapping automatically
}

void GLRenderer::computeBounds(std::list<Molecule*> &molecules)
{
    float minX = (float) INT_MAX;
    float minY = (float) INT_MAX;
    float minZ = (float) INT_MAX;
    float maxX = (float) INT_MIN;
    float maxY = (float) INT_MIN;
    float maxZ = (float) INT_MIN;

    std::list<Molecule*>::iterator molIter = molecules.begin();
    while (molIter != molecules.end())
    {
        Molecule *molecule = *molIter;
        molIter++;
        for (ResidueListIterator res = molecule->residuesBegin();
             res != molecule->residuesEnd(); ++res)
        {
            for (int i = 0; i < res->atomCount(); i++)
            {
                MIAtom *atom = res->atom(i);
                minX = std::min(minX, atom->x());
                minY = std::min(minY, atom->y());
                minZ = std::min(minZ, atom->z());
                maxX = std::max(maxX, atom->x());
                maxY = std::max(maxY, atom->y());
                maxZ = std::max(maxZ, atom->z());
            }
        }
    }

    xyDisplayRange = std::max(std::max(maxX-minX, maxY-minY), maxZ-minZ);
    if (xyDisplayRange == 0.0f)
    {
        xyDisplayRange = 1.0f;
    }
}

void GLRenderer::applyProjection(float scale)
{
    float zDisplayRange = xyDisplayRange;

    // Maintain square perspective regardless of window aspect ratio
    float widthRange = xyDisplayRange / scale;
    float aspectRatio = (float)width / (float)height;
    QMatrix4x4 proj;
    if (width <= height)
    {
        proj.ortho(-widthRange, widthRange, -widthRange / aspectRatio, widthRange / aspectRatio, 0.0f, 2.0f * zDisplayRange);
    }
    else
    {
        proj.ortho(-widthRange * aspectRatio, widthRange * aspectRatio, -widthRange, widthRange, 0.0f, 2.0f * zDisplayRange);
    }
    ModernShader::setProjectionMatrix(proj);
}

void GLRenderer::Draw2(Displaylist *displaylist, bool ShowVus, bool /* drawMapsFirst */, bool showSymmetryAsBackbone)
{

    logOpenGLErrors(__FILE__, __LINE__);
    computeBounds(displaylist->getMolecules());

    ModernShader::pushModelView();
    logOpenGLErrors(__FILE__, __LINE__);

    DrawMaps(displaylist->getMaps());
    int moleculeCounter = 0;
    std::list<Molecule*>::iterator node = displaylist->getMolecules().begin();
    while (node != displaylist->getMolecules().end())
    {
        if (dimNonactiveModels)
        {
            if ((*node) == displaylist->CurrentItem())
            {
                currentModel = true;
                light->glEnable();
                light2->glDisable();
            }
            else
            {
                light->glDisable();
                light2->glEnable();
                currentModel = false;
            }
        }
        else
        {
            currentModel = true;
            light->glEnable();
            light2->glDisable();
        }
        drawMolecule(*node);
        drawSymmetryMolecule(*node, showSymmetryAsBackbone);

        moleculeCounter++;
        if (ShowVus)
        {
            drawLines(displaylist->getLines(), 1, true);
        }
        drawSurface(displaylist->getCurrentDots());
        if ((*node)->getSecondaryStructure() != NULL)
        {
            glDisable(GL_CULL_FACE);
            DrawSecondaryStructure((*node)->getSecondaryStructure());
            glEnable(GL_CULL_FACE);
        }

        node++;
    }
    logOpenGLErrors(__FILE__, __LINE__);

    ModernShader::popModelView();

    logOpenGLErrors(__FILE__, __LINE__);
}

void GLRenderer::DrawSecondaryStructure(SecondaryStructure *secondaryStructure)
{
    RibbonSegment *pNextRSeg = secondaryStructure->m_pRibbonSegmentList;
    while (pNextRSeg)
    {
        DrawRibbonSegment(pNextRSeg);
        pNextRSeg = pNextRSeg->m_pNext;
    }

    std::vector<Helix*>::iterator pHelix = secondaryStructure->m_pHelixList.begin();
    for (; pHelix != secondaryStructure->m_pHelixList.end(); ++pHelix)
    {
        DrawHelix(*pHelix);
    }

    pNextRSeg = secondaryStructure->m_pSheetList;
    while (pNextRSeg)
    {
        DrawRibbonSegment(pNextRSeg);
        pNextRSeg = pNextRSeg->m_pNext;
    }


    pNextRSeg = secondaryStructure->m_pTurnList;
    while (pNextRSeg)
    {
        DrawRibbonSegment(pNextRSeg);
        pNextRSeg = pNextRSeg->m_pNext;
    }

    pNextRSeg = secondaryStructure->m_pRandomList;
    while (pNextRSeg)
    {
        DrawRibbonSegment(pNextRSeg);
        pNextRSeg = pNextRSeg->m_pNext;
    }

}

void GLRenderer::DrawRibbonSegment(RibbonSegment *ribbonSegment)
{
    SetColor(ribbonSegment->m_bRGB);
    RibbonSpan *pSpan = ribbonSegment->m_pRibbonSpanList->m_pNext;
    while (pSpan)
    {
        DrawRibbonSpan(pSpan);
        pSpan = pSpan->m_pNext;
    }
}

void GLRenderer::SetColor(unsigned char *bRGBA)
{
    for (int i = 0; i < 3; i++)
    {
        currentColor[i] = (float)bRGBA[i] / 255.0f;
    }
    currentColor[3] = 1.0f;
}

void GLRenderer::DrawRibbonSpan(RibbonSpan *ribbonSpan)
{
    DrawPolySurf(ribbonSpan->npr, ribbonSpan->nseg, ribbonSpan->ribbon_pts, ribbonSpan->ribbon_norms);
}

void GLRenderer::DrawHelix(Helix *helix)
{
    SetColor(helix->rgb);
    DrawCappedCylinder(Helix_NSEG, helix->m_dAxisA, helix->m_dAxisB, helix->m_dEndA, helix->m_dEndB, helix->m_dNormA, helix->m_dNormB, helix->m_dCapNormA, helix->m_dCapNormB);
}

void GLRenderer::DrawPolySurf(int npr, int nseg, double dPoints[][3], double dNorms[][3])
{
    if (npr <= 0 || nseg <= 1)
        return;

    std::vector<ModernVertex> verts;
    verts.reserve(npr * (nseg - 1) * 6);
    auto addV = [&](int idx) {
        ModernVertex v;
        v.px = static_cast<float>(dPoints[idx][0]);
        v.py = static_cast<float>(dPoints[idx][1]);
        v.pz = static_cast<float>(dPoints[idx][2]);
        v.nx = static_cast<float>(dNorms[idx][0]);
        v.ny = static_cast<float>(dNorms[idx][1]);
        v.nz = static_cast<float>(dNorms[idx][2]);
        v.r = currentColor[0]; v.g = currentColor[1]; v.b = currentColor[2]; v.a = currentColor[3];
        v.u = 0.0f; v.v = 0.0f;
        verts.push_back(v);
    };

    for (int i = 0; i < npr; i++)
    {
        for (int k = 0; k < nseg - 1; k++)
        {
            int ip0 = i * nseg + k;
            int ip1 = (i == npr - 1) ? k : (i + 1) * nseg + k;
            int ip2 = ip1 + 1;
            int ip3 = ip0 + 1;

            addV(ip0);
            addV(ip1);
            addV(ip3);
            addV(ip3);
            addV(ip1);
            addV(ip2);
        }
    }
    ModernShader::drawVertices(GL_TRIANGLES, verts.data(), static_cast<int>(verts.size()));
}

void GLRenderer::DrawCappedCylinder(int nSeg, double dAxisA[3], double dAxisB[3], double dEndA[][3], double dEndB[][3],
                                    double dNormA[][3], double dNormB[][3], double dCapNormA[][3], double dCapNormB[][3])
{
    if (nSeg <= 0)
        return;

    std::vector<ModernVertex> verts;
    verts.reserve(nSeg * 12);
    auto addV = [&](const double pt[3], const double nm[3]) {
        ModernVertex v;
        v.px = static_cast<float>(pt[0]);
        v.py = static_cast<float>(pt[1]);
        v.pz = static_cast<float>(pt[2]);
        v.nx = static_cast<float>(nm[0]);
        v.ny = static_cast<float>(nm[1]);
        v.nz = static_cast<float>(nm[2]);
        v.r = currentColor[0]; v.g = currentColor[1]; v.b = currentColor[2]; v.a = currentColor[3];
        v.u = 0.0f; v.v = 0.0f;
        verts.push_back(v);
    };

    int i;
    for (i = 0; i < nSeg; i++)
    {
        int j = (i + 1 >= nSeg) ? 0 : i + 1;
        addV(dEndA[i], dNormA[i]);
        addV(dEndB[i], dNormB[i]);
        addV(dEndB[j], dNormB[j]);

        addV(dEndA[i], dNormA[i]);
        addV(dEndB[j], dNormB[j]);
        addV(dEndA[j], dNormA[j]);
    }

    // Top A
    for (i = 0; i < nSeg; i++)
    {
        int j = (i + 1 >= nSeg) ? 0 : i + 1;
        addV(dAxisA, dCapNormA[i]);
        addV(dEndA[j], dCapNormA[j]);
        addV(dEndA[i], dCapNormA[i]);
    }

    // Bottom B
    for (i = 0; i < nSeg; i++)
    {
        int j = (i + 1 >= nSeg) ? 0 : i + 1;
        addV(dAxisB, dCapNormB[i]);
        addV(dEndB[i], dCapNormB[i]);
        addV(dEndB[j], dCapNormB[j]);
    }

    ModernShader::drawVertices(GL_TRIANGLES, verts.data(), static_cast<int>(verts.size()));
}

void GLRenderer::DrawMaps(std::vector<EMap*> &maps)
{
    if (maps.size() <= 0)
    {
        return;
    }

    for (unsigned int i = 0; i < maps.size(); i++)
    {
        EMap *emap = (EMap*) maps[i];
        if (emap->Visible())
        {
            drawLines(emap->edges, (int) emap->GetMapLinewidth(), true);

        }
    }
}

void GLRenderer::DrawLabels(std::list<Molecule*> &molecules)
{
    // Collect and sort labels for proper transparency
    float view[16];
    memcpy(view, ModernShader::modelViewMatrix().constData(), 16 * sizeof(float));
    Matrix4<float> viewmat(view);
    viewmat.transpose();

    typedef std::map<float, ATOMLABEL*> LabelOrderMap;
    LabelOrderMap order;
    std::list<Molecule*>::iterator node;
    for (node = molecules.begin(); node != molecules.end(); ++node)
    {
        if ((*node)->LabelsVisible())
        {
            Molecule::AtomLabelList molLabels = (*node)->getAtomLabels();
            Molecule::AtomLabelList::iterator mlIter;
            for (mlIter = molLabels.begin(); mlIter != molLabels.end(); ++mlIter)
            {
                ATOMLABEL *label = *mlIter;
                if (label->isVisible())
                {
                    const MIAtom &atom = *label->atom();
                    Tuple4<float> vec(atom.x(), atom.y(), atom.z(), 1.0f);
                    viewmat.transform(vec);
                    order.insert(std::make_pair(vec.z, label));
                }
            }
        }
    }
    Molecule::AtomLabelList labels;
    labels.reserve(order.size());
    foreach (const LabelOrderMap::value_type& l, order)
    {
        labels.push_back(l.second);
    }
    drawLabels(labels);
}

void GLRenderer::drawText(const char *text, float x, float y, float z)
{
    float inverseRotation[16];
    getInverseRotation(inverseRotation);
    drawText(text, x, y, z, 0.0f, labelsTextScale, inverseRotation, false, 0, white);
}

void GLRenderer::drawText(const char *text, float x, float y, float z, float offset, float scale, float inverseRotation[16], bool displayNumber, int number, float foreground[4])
{
    if (!labelsFont_.get())
        return;

    ModernShader::pushModelView();
    ModernShader::translateModelView(x, y, z);
    ModernShader::multModelView(inverseRotation);
    ModernShader::translateModelView(offset, offset, offset);
    ModernShader::scaleModelView(scale, scale, 1.0f);

    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (displayNumber)
    {
        QString numberString = QString(" %1 ").arg(number);
        float numberWidth = labelsFont_->fontMetrics().horizontalAdvance(numberString);
        float x1 = 0.0f;
        float y1 = -labelsFont_->fontMetrics().descent();
        float x2 = numberWidth;
        float y2 = labelsFont_->fontMetrics().ascent();
        float z1 = -0.02f;

        ModernVertex boxVerts[12] = {
            { x1, y1, z1, 0, 0, 1, 1.0f, 1.0f, 0.0f, 1.0f, 0, 0 },
            { x2, y1, z1, 0, 0, 1, 1.0f, 1.0f, 0.0f, 1.0f, 0, 0 },
            { x2, y2, z1, 0, 0, 1, 1.0f, 1.0f, 0.0f, 1.0f, 0, 0 },
            { x1, y1, z1, 0, 0, 1, 1.0f, 1.0f, 0.0f, 1.0f, 0, 0 },
            { x2, y2, z1, 0, 0, 1, 1.0f, 1.0f, 0.0f, 1.0f, 0, 0 },
            { x1, y2, z1, 0, 0, 1, 1.0f, 1.0f, 0.0f, 1.0f, 0, 0 }
        };
        float xOffset = (x2 - x1) / 10.0f;
        float yOffset = (y2 - y1) / 10.0f;
        float ix1 = x1 + xOffset, iy1 = y1 + yOffset;
        float ix2 = x2 - xOffset, iy2 = y2 - yOffset;
        float z2 = z1 + 0.01f;
        boxVerts[6]  = { ix1, iy1, z2, 0, 0, 1, 1.0f, 0.0f, 0.0f, 1.0f, 0, 0 };
        boxVerts[7]  = { ix2, iy1, z2, 0, 0, 1, 1.0f, 0.0f, 0.0f, 1.0f, 0, 0 };
        boxVerts[8]  = { ix2, iy2, z2, 0, 0, 1, 1.0f, 0.0f, 0.0f, 1.0f, 0, 0 };
        boxVerts[9]  = { ix1, iy1, z2, 0, 0, 1, 1.0f, 0.0f, 0.0f, 1.0f, 0, 0 };
        boxVerts[10] = { ix2, iy2, z2, 0, 0, 1, 1.0f, 0.0f, 0.0f, 1.0f, 0, 0 };
        boxVerts[11] = { ix1, iy2, z2, 0, 0, 1, 1.0f, 0.0f, 0.0f, 1.0f, 0, 0 };
        ModernShader::drawVertices(GL_TRIANGLES, boxVerts, 12);

        labelsFont_->renderText(0, 0, numberString, white);
        ModernShader::translateModelView(numberWidth, 0.0f, 0.0f);
    }

    labelsFont_->renderText(0, 0, QString(text), foreground);

    ModernShader::setLightingEnabled(prevLight);
    ModernShader::popModelView();
}

void GLRenderer::getInverseRotation(float inverseRotation[16])
{
    static GLfloat modelview[16];
    memcpy(modelview, ModernShader::modelViewMatrix().constData(), 16 * sizeof(float));
    Matrix4<float> matrix(modelview);
    Quaternion<float> quat;
    quat.set(matrix);
    quat.negate();
    matrix.set(quat);
    matrix.getInColumnMajorOrder(inverseRotation);
}

void GLRenderer::drawLabels(Molecule::AtomLabelList &labels)
{
    if (labels.size() <= 0)
    {
        return;
    }

    float foreground[4];
    float inverseRotation[16];
    getInverseRotation(inverseRotation);

    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    Molecule::AtomLabelList::iterator iter;
    for (iter = labels.begin(); iter != labels.end(); ++iter)
    {
        ATOMLABEL &label = **iter;
        const MIAtom *atom = label.atom();
        if (hideHydrogens && MIAtom::MIIsHydrogen(atom))
        {
            continue;
        }

        const std::string &text = label.label();
        float x = atom->x();
        float y = atom->y();
        float z = atom->z();
        float offset = atom->getRadius() * style.getBallPercent();
        foreground[0] = label.red() / 255.0f;
        foreground[1] = label.green() / 255.0f;
        foreground[2] = label.blue() / 255.0f;
        foreground[3] = 1.0f;

        drawText(text.c_str(), x, y, z, offset, labelsTextScale, inverseRotation, false, 0, foreground);
    }
    ModernShader::setLightingEnabled(prevLight);
}

void GLRenderer::DrawAnnotations(std::list<Molecule*> &molecules)
{
    std::list<Molecule*>::iterator node = molecules.begin();
    while (node != molecules.end())
    {
        drawAnnotations((*node)->getAnnotations(), (*node)->isDrawAnnotationBox());
        node++;
    }
}

void GLRenderer::drawAnnotations(Molecule::AnnotationList &Annotations, bool drawBox)
{
    if (Annotations.size() <= 0)
    {
        return;
    }

    float inverseRotation[16];
    getInverseRotation(inverseRotation);

    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    int annotationNumber = 0;
    Molecule::AnnotationList::iterator iter = Annotations.begin();
    while (iter != Annotations.end())
    {
        Annotation *annotation = *iter;
        ++iter;
        ++annotationNumber;
        if (annotation->isHidden())
        {
            continue;
        }
        PushedName pushedName(getPickName(*annotation), pickingEnabled);
        const char *text = annotation->m_text.c_str();
        float x = annotation->m_x;
        float y = annotation->m_y;
        float z = annotation->m_z;
        bool displayNumber = drawBox;
        if (annotation->m_type == Annotation::Plane)
        {
            displayNumber = true;
        }
        float *foreground = getColor(annotation->m_color);
        drawText(text, x, y, z, 0.0f, labelsTextScale, inverseRotation, displayNumber, annotationNumber, foreground);
    }
    ModernShader::setLightingEnabled(prevLight);
}

void GLRenderer::circleStackAtoms(Stack *stack)
{
    if (stack->empty())
    {
        return;
    }

    ModernShader::pushModelView();
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    float inverseRotation[16];
    getInverseRotation(inverseRotation);

    glLineWidth(1.0f);
    const int numSegs = 64;
    Stack::DataContainer::const_iterator iter = stack->getData().begin();
    while (iter != stack->getData().end())
    {
        StackItem item = *iter;
        ++iter;
        MIAtom *atom = item.atom;
        if (MIAtom::isValid(atom))
        {
            if (hideHydrogens && MIAtom::MIIsHydrogen(atom))
            {
                continue;
            }
            float cr = (iter == stack->getData().end()) ? 1.0f : 0.7f;
            float cg = (iter == stack->getData().end()) ? 1.0f : 0.7f;
            float cb = 1.0f;

            if (item.molecule->Visible())
            {
                ModernShader::pushModelView();
                ModernShader::translateModelView(atom->x(), atom->y(), atom->z());
                ModernShader::multModelView(inverseRotation);

                float radius = 1.2f * atom->getRadius() * style.getBallPercent();
                std::vector<ModernVertex> circleVerts(numSegs + 1);
                for (int s = 0; s <= numSegs; ++s)
                {
                    float angle = (2.0f * static_cast<float>(M_PI) * s) / numSegs;
                    float x = radius * std::sin(angle);
                    float y = radius * std::cos(angle);
                    circleVerts[s] = { x, y, 0.0f, 0.0f, 0.0f, 1.0f, cr, cg, cb, 1.0f, 0.0f, 0.0f };
                }
                ModernShader::drawVertices(GL_LINE_STRIP, circleVerts.data(), numSegs + 1);
                ModernShader::popModelView();
            }
        }
    }
    glEnable(GL_DEPTH_TEST);
    ModernShader::setLightingEnabled(prevLight);
    ModernShader::popModelView();
    logOpenGLErrors(__FILE__, __LINE__);
}

void GLRenderer::CircleAtoms(MIAtom **atoms, int natoms)
{
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    float inverseRotation[16];
    getInverseRotation(inverseRotation);

    glLineWidth(1.0f);
    logOpenGLErrors(__FILE__, __LINE__);
    const int numSegs = 64;
    for (int index = 0; index < natoms; index++)
    {
        MIAtom *atom = atoms[index];
        if (hideHydrogens && MIAtom::MIIsHydrogen(atom))
        {
            continue;
        }

        if (atom != NULL)
        {
            ModernShader::pushModelView();
            ModernShader::translateModelView(atom->x(), atom->y(), atom->z());
            ModernShader::multModelView(inverseRotation);

            float radius = 1.2f * atom->getRadius() * style.getBallPercent();
            std::vector<ModernVertex> circleVerts(numSegs + 1);
            for (int s = 0; s <= numSegs; ++s)
            {
                float angle = (2.0f * static_cast<float>(M_PI) * s) / numSegs;
                float x = radius * std::sin(angle);
                float y = radius * std::cos(angle);
                circleVerts[s] = { x, y, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
            }
            ModernShader::drawVertices(GL_LINE_STRIP, circleVerts.data(), numSegs + 1);
            ModernShader::popModelView();
        }
    }
    glEnable(GL_DEPTH_TEST);
    ModernShader::setLightingEnabled(prevLight);
    logOpenGLErrors(__FILE__, __LINE__);
}

void GLRenderer::DrawContacts(std::vector<CONTACT> &Contacts)
{
    if (Contacts.size() <= 0)
    {
        return;
    }

    float inverseRotation[16];
    getInverseRotation(inverseRotation);
    char text[20];

    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(1.0f);
    for (unsigned int i = 0; i < Contacts.size(); i++)
    {
        MIAtom &a1 = *Contacts[i].line.getAtom1();
        MIAtom &a2 = *Contacts[i].line.getAtom2();
        if (Contacts[i].color > 0)
        {
            sprintf(text, "%0.2f", Contacts[i].d);
            float *c = getColor(Contacts[i].color);
            ModernVertex line[2] = {
                { a1.x(), a1.y(), a1.z(), 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 },
                { a2.x(), a2.y(), a2.z(), 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 }
            };
            ModernShader::drawVertices(GL_LINES, line, 2);
            float x = (a1.x() + a2.x()) * 0.5f;
            float y = (a1.y() + a2.y()) * 0.5f;
            float z = (a1.z() + a2.z()) * 0.5f;
            drawText(text, x, y, z, 0.0f, labelsTextScale, inverseRotation, false, 0, white);
        }
    }
    ModernShader::setLightingEnabled(prevLight);
}

void GLRenderer::DrawVus(std::vector<PLINE> &Vus, int w)
{
    drawLines(Vus, w, false);
}

void GLRenderer::drawLines(std::vector<PLINE> &Vus, int w, bool withDepthTest)
{
    if (Vus.size() <= 0)
        return;

    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!withDepthTest)
    {
        glDisable(GL_DEPTH_TEST);
    }

    Q_UNUSED(w);
    glLineWidth(1.0f);
    std::vector<ModernVertex> verts;
    verts.reserve(Vus.size() * 2);

    for (unsigned int i = 0; i < Vus.size(); i++)
    {
        APOINT &a1 = Vus[i].p1;
        APOINT &a2 = Vus[i].p2;
        QVector4D color(getColorVector(a1.color, false));
        verts.push_back({ a1.x, a1.y, a1.z, 0.0f, 0.0f, 1.0f, color.x(), color.y(), color.z(), color.w(), 0.0f, 0.0f });
        verts.push_back({ a2.x, a2.y, a2.z, 0.0f, 0.0f, 1.0f, color.x(), color.y(), color.z(), color.w(), 0.0f, 0.0f });
    }

    ModernShader::drawVertices(GL_LINES, verts.data(), static_cast<int>(verts.size()));

    if (!withDepthTest)
    {
        glEnable(GL_DEPTH_TEST);
    }
    ModernShader::setLightingEnabled(prevLight);
}

float*GLRenderer::getColor(int colorIndex, bool adjustForCurrentModel)
{
    static float color[4];
    float scaling = 1.0f;
    if (adjustForCurrentModel && !currentModel)
    {
        scaling = 1.0 - amountToDimNonactiveModels;
    }
    int c = PaletteIndex(colorIndex);
    color[0] = Colors::RPallette[c] / 255.0f * scaling;
    color[1] = Colors::GPallette[c] / 255.0f * scaling;
    color[2] = Colors::BPallette[c] / 255.0f * scaling;
    color[3] = 1.0f;
    return color;
}

QVector4D GLRenderer::getColorVector(int colorIndex, bool adjustForCurrentModel)
{
    float scaling = 1.0f;
    if (adjustForCurrentModel && !currentModel)
        scaling = 1.0 - amountToDimNonactiveModels;
    int c = PaletteIndex(colorIndex);
    return QVector4D(Colors::RPallette[c] / 255.0f * scaling,
                     Colors::GPallette[c] / 255.0f * scaling,
                     Colors::BPallette[c] / 255.0f * scaling,
                     1.0f);
}


float*GLRenderer::getColor(PaletteColor c, bool adjustForCurrentModel)
{
    static float color[4];
    float scaling = 1.0f;
    if (adjustForCurrentModel && !currentModel)
    {
        scaling = 1.0 - amountToDimNonactiveModels;
    }
    color[0] = c.red / 255.0f * scaling;
    color[1] = c.green / 255.0f * scaling;
    color[2] = c.blue / 255.0f * scaling;
    color[3] = 1.0f;
    return color;
}

void GLRenderer::drawAngles(std::vector<ANGLE> &angles)
{
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glLineWidth(1.0f);
    for (unsigned int i = 0; i < angles.size(); i++)
    {
        ANGLE &angle = angles[i];
        MIAtom *a1 = angles[i].getAtom1();
        MIAtom *a2 = angles[i].atom3;
        if (hideHydrogens && (MIAtom::MIIsHydrogen(a1) || MIAtom::MIIsHydrogen(a2)))
        {
            continue;
        }
        PushedName pushedName(getPickName(angle), pickingEnabled);
        ModernVertex line[2] = {
            { a1->x(), a1->y(), a1->z(), 0, 0, 1, currentColor[0], currentColor[1], currentColor[2], currentColor[3], 0, 0 },
            { a2->x(), a2->y(), a2->z(), 0, 0, 1, currentColor[0], currentColor[1], currentColor[2], currentColor[3], 0, 0 }
        };
        ModernShader::drawVertices(GL_LINES, line, 2);
    }
    ModernShader::setLightingEnabled(prevLight);
}

void GLRenderer::drawBonds(std::vector<Bond> &bonds, int *color)
{
    Vector3<float> eye;
    if (camera != NULL)
    {
        eye.set(camera->getEye());
    }
    int lineDepthCueSteps = 4;
    float lineDepthCueIncrement = (frustum->getFarClipping() - frustum->getNearClipping()) / lineDepthCueSteps;
    float lineDepthCueStart = frustum->getNearClipping();

    // Map of bond renderers by line width
    QMap<float, BondRenderer> bondRenders;
    std::vector<Bond>::iterator bondsIter = bonds.begin();
    std::vector<Bond>::iterator bondsEnd = bonds.end();
    Vector3<float> pos1;
    Vector3<float> pos2;
    Vector3<float> diff;
    while (bondsIter != bondsEnd)
    {
        Bond &bond = *bondsIter;
        ++bondsIter;
        MIAtom *a1 = bond.getAtom1();
        MIAtom *a2 = bond.getAtom2();
        if (hideHydrogens && (MIAtom::MIIsHydrogen(a1) || MIAtom::MIIsHydrogen(a2)))
        {
            continue;
        }

        pos1.set(a1->x(), a1->y(), a1->z());
        pos2.set(a2->x(), a2->y(), a2->z());
        diff = pos1 - pos2;
        float separation = diff.length();
        if (a1->color() > 0 && a2->color() > 0)
        {
            if (frustum->sphereInFrustum(pos1, separation) != Frustum::OUTSIDE
                && frustum->sphereInFrustum(pos2, separation) != Frustum::OUTSIDE)
            {

                int c1 = ::getColor(a1);
                int c2 = ::getColor(a2);
                if (color != NULL)
                {
                    c1 = *color;
                    c2 = *color;
                }
                unsigned char bondOrder = bond.getOrder();
                if (!showBondOrders)
                {
                    bondOrder = NORMALBOND;
                }
                if (bond.type == B_POINT || bond.type == B_SYMM_POINT)
                {
                    if (!style.isAtomBall())
                    {
                        float radius = a1->getRadius() * style.getBallPercent();
                        if (radius > 0.0f && a1->color() > 0)
                        {
                            PushedName pushedName(getPickName(bond), pickingEnabled);
                            int c1 = ::getColor(a1);
                            float *col = getColor(c1);
                            memcpy(currentColor, col, 4 * sizeof(float));
                            float p1[3];
                            p1[0] = a1->x();
                            p1[1] = a1->y();
                            p1[2] = a1->z();
                            DrawSphere(p1, radius, 16);
                        }
                    }
                }
                else if (style.isBondCylinder())
                {
                    PushedName pushedName(getPickName(bond), pickingEnabled);
                    float radius = std::min(a1->getRadius(), a2->getRadius());
                    radius *= style.getStickPercent();

                    if (viewVectorSet && (bondOrder == DOUBLEBOND || bondOrder == PARTIALDOUBLEBOND || bondOrder == TRIPLEBOND))
                    {
                        radius /= 2.0f;
                    }
                    if (!viewVectorSet || bondOrder == NORMALBOND || bondOrder == SINGLEBOND || bondOrder == TRIPLEBOND
                        || bondOrder == IONICBOND || bondOrder == METALLIGANDBOND)
                    {
                        drawBondCylinder(pos1, c1, pos2, c2, radius, true);
                    }
                    if (viewVectorSet && (bondOrder == DOUBLEBOND || bondOrder == PARTIALDOUBLEBOND || bondOrder == TRIPLEBOND))
                    {
                        Vector3<float> offset;
                        Vector3<float> bondVector(pos2 - pos1);
                        offset.cross(viewVector, bondVector);
                        float scaleFactor = 2.0f*radius;
                        if (bondOrder == TRIPLEBOND)
                        {
                            scaleFactor *= 1.5;
                        }
                        offset.scale(scaleFactor / offset.length());
                        Vector3<float> p1 = pos1 + offset;
                        Vector3<float> p2 = pos2 + offset;
                        drawBondCylinder(p1, c1, p2, c2, radius, true);
                        p1 = pos1 - offset;
                        p2 = pos2 - offset;
                        if (bondOrder == PARTIALDOUBLEBOND)
                        {
                            bondVector.scale(0.3f);
                            p1.add(bondVector);
                            p2.subtract(bondVector);
                        }
                        drawBondCylinder(p1, c1, p2, c2, radius, true);
                    }
                }
                else if (style.isBondLine())
                {
                    float lineWidth = style.getBondLineWidth();
                    if (lineWidthDepthCued)
                    {
                        float bondDist = std::min(Vector3<float>(pos1 - eye).length(), Vector3<float>(pos2 - eye).length());
                        for (int i = 1; i <= lineDepthCueSteps; ++i)
                        {
                            if (bondDist < (lineDepthCueStart + lineDepthCueIncrement * i))
                            {
                                lineWidth += (float) (lineDepthCueSteps - i);
                                break;
                            }
                        }
                    }
                    PushedName pushedName(getPickName(bond), pickingEnabled && lineWidth > 0.0f);
                    if (bondOrder == NORMALBOND || bondOrder == SINGLEBOND || bondOrder == TRIPLEBOND
                        || bondOrder == IONICBOND || bondOrder == METALLIGANDBOND)
                    {
                        if (!pickingEnabled)
                            bondRenders[lineWidth].addBond(QVector3D(pos1.x, pos1.y, pos1.z), getColorVector(c1, true),
                                                          QVector3D(pos2.x, pos2.y, pos2.z), getColorVector(c2, true));
                        else
                            drawBondLine(pos1, c1, pos2, c2, lineWidth);
                    }
                    if (bondOrder == DOUBLEBOND || bondOrder == PARTIALDOUBLEBOND || bondOrder == TRIPLEBOND)
                    {
                        Vector3<float> offset;
                        Vector3<float> bondVector(pos2 - pos1);
                        offset.cross(viewVector, bondVector);
                        offset.scale(0.08 / offset.length());
                        Vector3<float> p1 = pos1 + offset;
                        Vector3<float> p2 = pos2 + offset;
                        drawBondLine(p1, c1, p2, c2, lineWidth);
                        p1 = pos1 - offset;
                        p2 = pos2 - offset;
                        if (bondOrder == PARTIALDOUBLEBOND)
                        {
                            bondVector.scale(0.3f);
                            p1.add(bondVector);
                            p2.subtract(bondVector);
                        }
                        drawBondLine(p1, c1, p2, c2, lineWidth);
                    }
                }
            }
        }
    }
    if (!pickingEnabled)
    {
        bool prevLight = ModernShader::isLightingEnabled();
        ModernShader::setLightingEnabled(false);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        foreach (float lineWidth, bondRenders.keys())
        {
            glLineWidth(1.0f);
            bondRenders[lineWidth].draw();
        }
        ModernShader::setLightingEnabled(prevLight);
    }
}

void GLRenderer::drawBondCylinder(const Vector3<float> &pos1, int color1, const Vector3<float> &pos2, int color2, float radius, bool capped)
{
    if (radius <= 0.0f)
    {
        return;
    }
    Vector3<float> midPoint = pos1 + pos2;
    midPoint.scale(0.5);

    float p[3];
    {
        PushedName pushedName(1, pickingEnabled);
        memcpy(currentColor, getColor(color1), 4 * sizeof(float));
        DrawCylinder(pos1, midPoint, radius);
        if (capped)
        {
            p[0] = pos1.x;
            p[1] = pos1.y;
            p[2] = pos1.z;
            DrawSphere(p, radius, 16);
        }
    }

    {
        PushedName pushedName(2, pickingEnabled);
        memcpy(currentColor, getColor(color2), 4 * sizeof(float));
        DrawCylinder(midPoint, pos2, radius);
        if (capped)
        {
            p[0] = pos2.x;
            p[1] = pos2.y;
            p[2] = pos2.z;
            DrawSphere(p, radius, 16);
        }
    }
}

void GLRenderer::drawBondLine(const Vector3<float> &pos1, int color1, const Vector3<float> &pos2, int color2, float lineWidth)
{
    if (lineWidth <= 0.0f)
    {
        return;
    }
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);

    if (joinBondsOfSameColor && color1 == color2)
    {
        float *c = getColor(color1, true);
        ModernVertex line[2] = {
            { pos1.x, pos1.y, pos1.z, 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 },
            { pos2.x, pos2.y, pos2.z, 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 }
        };
        ModernShader::drawVertices(GL_LINES, line, 2);
    }
    else
    {
        Vector3<float> midPoint = pos1 + pos2;
        midPoint.scale(0.5f);
        {
            PushedName pushedName(1, pickingEnabled);
            float *c = getColor(color1, true);
            ModernVertex line[2] = {
                { pos1.x, pos1.y, pos1.z, 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 },
                { midPoint.x, midPoint.y, midPoint.z, 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 }
            };
            ModernShader::drawVertices(GL_LINES, line, 2);
        }
        {
            PushedName pushedName(2, pickingEnabled);
            float *c = getColor(color2, true);
            ModernVertex line[2] = {
                { midPoint.x, midPoint.y, midPoint.z, 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 },
                { pos2.x, pos2.y, pos2.z, 0, 0, 1, c[0], c[1], c[2], c[3], 0, 0 }
            };
            ModernShader::drawVertices(GL_LINES, line, 2);
        }
    }
    ModernShader::setLightingEnabled(prevLight);
}

void GLRenderer::drawResidueAtoms(ResidueListIterator res, ResidueListIterator resEnd)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    Vector3<float> pos;
    for (; res != resEnd; ++res)
    {
        for (unsigned int i = 0; i < (unsigned int) res->atomCount(); i++)
        {
            MIAtom *a1 = res->atom(i);
            if (hideHydrogens && MIAtom::MIIsHydrogen(a1))
            {
                continue;
            }
            float radius = a1->getRadius() * style.getBallPercent();
            if (radius > 0.0f && a1->color() > 0)
            {
                pos.set(a1->x(), a1->y(), a1->z());
                if (frustum->sphereInFrustum(pos, radius) != Frustum::OUTSIDE)
                {
                    PushedName pushedName(getPickName(a1), pickingEnabled);
                    int c1 = ::getColor(a1);
                    memcpy(currentColor, getColor(c1), 4 * sizeof(float));
                    float p1[3];
                    p1[0] = a1->x();
                    p1[1] = a1->y();
                    p1[2] = a1->z();
                    DrawSphere(p1, radius, 16);
                }
            }
        }
    }
}

void GLRenderer::drawMolecule(Molecule *molecule)
{
    if (molecule->Visible())
    {
        drawBonds(molecule->getBonds());
        int hBondColor = Colors::WHITE;
        if (molecule->hbonds.size() != molecule->hbondContacts.size())
        {
            std::vector<CONTACT>().swap(molecule->hbondContacts); // was molecule->hbondContacts.clear();
            std::vector<Bond>::iterator hbond = molecule->hbonds.begin();
            while (hbond != molecule->hbonds.end())
            {
                Bond &bond = *hbond;
                ++hbond;
                CONTACT contact;
                contact.line = bond;
                contact.color = hBondColor;
                contact.d = bond.getAtom1()->distance(bond.getAtom2());
                molecule->hbondContacts.push_back(contact);
            }
        }
        DrawContacts(molecule->hbondContacts);
        if (style.isAtomBall())
        {
            drawResidueAtoms(molecule->residuesBegin(), molecule->residuesEnd());
        }
        if (molecule->DotsVisible())
        {
            drawSurface(molecule->getDots());
        }
    }
}

void GLRenderer::drawSymmetryMolecule(Molecule *molecule, bool showSymmetryAsBackbone)
{
    if (molecule->Visible())
    {
        drawBonds(molecule->getSymmetryBonds());
        if (!showSymmetryAsBackbone && style.isAtomBall())
            drawResidueAtoms(molecule->symmResiduesBegin(), molecule->symmResiduesEnd());
    }
}

GLuint GLRenderer::getPickName(MIAtom *atom)
{
    GLuint name = atomPickNameMap.size()+1;
    if (pickingEnabled)
    {
        atomPickNameMap.insert(std::map<GLuint, MIAtom*>::value_type(name, atom));
    }
    return name;
}

GLuint GLRenderer::getPickName(Annotation &annotation)
{
    GLuint name = annotationPickNameMap.size()+1;
    if (pickingEnabled)
    {
        annotationPickNameMap.insert(std::map<GLuint, Annotation*>::value_type(name, &annotation));
    }
    return name;
}

GLuint GLRenderer::getPickName(Bond &bond)
{
    GLuint name = bondPickNameMap.size()+1;
    if (pickingEnabled)
    {
        bondPickNameMap.insert(std::map<GLuint, Bond*>::value_type(name, &bond));
    }
    return name;
}

GLuint GLRenderer::getPickName(ANGLE &angle)
{
    GLuint name = anglePickNameMap.size()+1;
    if (pickingEnabled)
    {
        anglePickNameMap.insert(std::map<GLuint, ANGLE*>::value_type(name, &angle));
    }
    return name;
}

void GLRenderer::DrawCylinder(const Vector3<float> &start, const Vector3<float> &end, float radius)
{
    ModernShader::drawCylinder(
        QVector3D(start.x, start.y, start.z),
        QVector3D(end.x, end.y, end.z),
        radius,
        QVector4D(currentColor[0], currentColor[1], currentColor[2], currentColor[3])
    );
}

void GLRenderer::normalize(float a[3])
{
    // Find length and divide each term
    float len = sqrt((a[0]*a[0] + a[1]*a[1] + a[2]*a[2]));
    if (len > .000001)
    {
        for (int i = 0; i < 3; i++)
        {
            a[i] = a[i]/len;
        }
    }

}

void GLRenderer::cross(float a[3], float b[3], float c[3])
{

    c[0] = a[1]*b[2] - a[2]*b[1];
    c[1] = a[2]*b[0] - a[0]*b[2];
    c[2] = a[0]*b[1] - a[1]*b[0];

}

void GLRenderer::vecmat(float a[3], float mat[3][3], float b[3])
{
    for (int i = 0; i < 3; i++)
    {
        b[i] = a[0]*mat[0][i] + a[1]*mat[1][i] + a[2]*mat[2][i];
    }
}

void GLRenderer::setIdentity(float rotation[])
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

void GLRenderer::DrawSphere(float c[3], float radius, int tess)
{
    ModernShader::drawUnitSphere(
        QVector3D(c[0], c[1], c[2]),
        radius,
        QVector4D(currentColor[0], currentColor[1], currentColor[2], currentColor[3]),
        tess, tess
    );
}

void GLRenderer::DrawSphere2(float c[3], float radius, int tess)
{
    ModernShader::drawUnitSphere(
        QVector3D(c[0], c[1], c[2]),
        radius,
        QVector4D(1.0f, 1.0f, 1.0f, 1.0f),
        std::abs(tess), std::abs(tess)
    );
}

#define sphereoidX 0.525731112119133606f
#define sphereoidZ 0.850650808352039932f

void GLRenderer::RenderSphereoid(float*, float radius[3], int tess)
{
    static float fNormal[12][3] =
    {
        {-sphereoidX, 0.0f, sphereoidZ}, {sphereoidX, 0.0f, sphereoidZ}, {-sphereoidX, 0.0f, -sphereoidZ}, {sphereoidX, 0.0f, -sphereoidZ},
        {0.0f, sphereoidZ, sphereoidX}, {0.0f, sphereoidZ, -sphereoidX}, {0.0f, -sphereoidZ, sphereoidX}, {0.0f, -sphereoidZ, -sphereoidX},
        {sphereoidZ, sphereoidX, 0.0f}, {-sphereoidZ, sphereoidX, 0.0f}, {sphereoidZ, -sphereoidX, 0.0f}, {-sphereoidZ, -sphereoidX, 0.0f}
    };

    // Clockwise front faces
    //  static int tindeces[20][3] = {
    //    {0,4,1}, {0,9,4}, {9,5,4}, {4,5,8}, {4,8,1},
    //    {8,10,1}, {8,3,10}, {5,3,8}, {5,2,3}, {2,7,3},
    //    {7,10,3}, {7,6,10}, {7,11,6}, {11,0,6}, {0,1,6},
    //    {6,1,10}, {9,0,11}, {9,11,2}, {9,2,5}, {7,2,11}
    //  };
    // Counter-clockwise front faces
    static int tindeces[20][3] =
    {
        {1, 4, 0}, {4, 9, 0}, {4, 5, 9}, {8, 5, 4}, {1, 8, 4},
        {1, 10, 8}, {10, 3, 8}, {8, 3, 5}, {3, 2, 5}, {3, 7, 2},
        {3, 10, 7}, {10, 6, 7}, {6, 11, 7}, {6, 0, 11}, {6, 1, 0},
        {10, 1, 6}, {11, 0, 9}, {2, 11, 9}, {5, 2, 9}, {11, 2, 7}
    };
    int i;

    //if tess is negative then draw as a wire frame
    bool bWireFrame = false;
    if (tess < 0)
    {
        tess = -tess;
        bWireFrame = true;
    }

    //Set the normals and adjust for radius
    float fData[12][3];
    float fRadius[3];
    for (i = 0; i < 3; i++)
    {
        fRadius[i] = (float)radius[i];
    }
    for (i = 0; i < 12; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            fData[i][j] = fNormal[i][j]*fRadius[j];
        }
    }

    //Since this is a sphere, all the midpoint vectors will have the same length.
    float v[3];
    for (i = 0; i < 3; i++)
    {
        v[i] = fNormal[0][i] + fNormal[4][i];
    }
    float fnorm = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    int level = 2;
    if (tess < 10)
    {
        level = 1;
    }
    if (bWireFrame)
    {
        for (i = 0; i < 20; i++)
        {
            TriangleRender(bWireFrame, fNormal[tindeces[i][0]], fNormal[tindeces[i][1]], fNormal[tindeces[i][2]],
                           fData[tindeces[i][0]], fData[tindeces[i][1]], fData[tindeces[i][2]]);
        }
    }
    else
    {
        for (i = 0; i < 20; i++)
        {
            Subdivide(level, bWireFrame, fnorm, fRadius,
                      fNormal[tindeces[i][0]], fNormal[tindeces[i][1]], fNormal[tindeces[i][2]],
                      fData[tindeces[i][0]], fData[tindeces[i][1]], fData[tindeces[i][2]]);
        }
    }
}

void GLRenderer::Subdivide(int level, bool bWireFrame, float fnorm, float radius[3],
                           float n1[3], float n2[3], float n3[3],
                           float v1[3], float v2[3], float v3[3])
{
    float v12[3], v23[3], v31[3], n12[3], n23[3], n31[3];
    for (int i = 0; i < 3; i++)
    {
        //This is effectively a normalize since these are all the same lenght.
        n12[i] = (n1[i]+n2[i])/fnorm;
        n23[i] = (n2[i]+n3[i])/fnorm;
        n31[i] = (n3[i]+n1[i])/fnorm;
    }
    /*
       float fnorm12 = sqrt ((double)(n12[0]*n12[0] + n12[1]*n12[1] + n12[2]*n12[2]));
       float fnorm23 = sqrt ((double)(n23[0]*n23[0] + n23[1]*n23[1] + n23[2]*n23[2]));
       float fnorm31 = sqrt ((double)(n31[0]*n31[0] + n31[1]*n31[1] + n31[2]*n31[2]));

       for (i=0; i<3; i++)
       {
        n12[i] = n12[i]/fnorm12;
        n23[i] = n23[i]/fnorm23;
        n31[i] = n31[i]/fnorm31;
       }
     */

    for (int i2 = 0; i2 < 3; i2++)
    {
        v12[i2] = n12[i2]*radius[i2];
        v23[i2] = n23[i2]*radius[i2];
        v31[i2] = n31[i2]*radius[i2];
    }

    if (level == 1)
    {
        TriangleRender(bWireFrame, n1, n12, n31, v1, v12, v31);
        TriangleRender(bWireFrame, n2, n23, n12, v2, v23, v12);
        TriangleRender(bWireFrame, n3, n31, n23, v3, v31, v23);
        TriangleRender(bWireFrame, n12, n23, n31, v12, v23, v31);
    }
    else
    {
        float v[3];
        for (int i = 0; i < 3; i++)
        {
            v[i] = n1[i] + n12[i];
        }
        fnorm = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
        level--;
        Subdivide(level, bWireFrame, fnorm, radius, n1, n12, n31, v1, v12, v31);
        Subdivide(level, bWireFrame, fnorm, radius, n2, n23, n12, v2, v23, v12);
        Subdivide(level, bWireFrame, fnorm, radius, n3, n31, n23, v3, v31, v23);
        Subdivide(level, bWireFrame, fnorm, radius, n12, n23, n31, v12, v23, v31);
    }
}

void GLRenderer::TriangleRender(bool bWireFrame, float n1[3], float n2[3], float n3[3],
                                float v1[3], float v2[3], float v3[3])
{
    ModernVertex tri[3] = {
        { v1[0], v1[1], v1[2], n1[0], n1[1], n1[2], 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },
        { v2[0], v2[1], v2[2], n2[0], n2[1], n2[2], 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },
        { v3[0], v3[1], v3[2], n3[0], n3[1], n3[2], 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f }
    };
    ModernShader::drawVertices(bWireFrame ? GL_LINE_LOOP : GL_TRIANGLES, tri, 3);
}

bool GLRenderer::isFogEnabled()
{
    return fogEnabled;
}

void GLRenderer::setFogEnabled(bool on)
{
    fogEnabled = on;
}

float GLRenderer::getFogStart()
{
    return fogStart;
}

void GLRenderer::setFogStart(float z)
{
    fogStart = z;
}

float GLRenderer::getFogEnd()
{
    return fogEnd;
}

void GLRenderer::setFogEnd(float z)
{
    fogEnd = z;
}

RenderStyle&GLRenderer::getRenderStyle()
{
    return style;
}

void GLRenderer::setRenderStyle(const RenderStyle &style)
{
    this->style.set(style);
}

void GLRenderer::setHideHydrogens(bool on)
{
    hideHydrogens = on;
}

bool GLRenderer::isHideHydrogens()
{
    return hideHydrogens;
}

void GLRenderer::setFrustum(Frustum *frustum)
{
    this->frustum = frustum;
}

void GLRenderer::setCamera(Camera *camera)
{
    this->camera = camera;
}

MIAtom*GLRenderer::getAtom(int id)
{
    MIAtom *atom = NULL;
    if (id > 0)
    {
        std::map<GLuint, MIAtom*>::iterator pos = atomPickNameMap.find(id);
        if (pos != atomPickNameMap.end())
        {
            atom = pos->second;
        }
    }
    return atom;
}

Bond*GLRenderer::getBond(int id)
{
    Bond *bond = NULL;
    if (id > 0)
    {
        std::map<GLuint, Bond*>::iterator pos = bondPickNameMap.find(id);
        if (pos != bondPickNameMap.end())
        {
            bond = pos->second;
        }
    }
    return bond;
}

ANGLE*GLRenderer::getAngle(int id)
{
    ANGLE *angle = NULL;
    if (id > 0)
    {
        std::map<GLuint, ANGLE*>::iterator pos = anglePickNameMap.find(id);
        if (pos != anglePickNameMap.end())
        {
            angle = pos->second;
        }
    }
    return angle;
}

Annotation*GLRenderer::getAnnotation(int id)
{
    Annotation *annotation = NULL;
    if (id > 0)
    {
        std::map<GLuint, Annotation*>::iterator pos = annotationPickNameMap.find(id);
        if (pos != annotationPickNameMap.end())
        {
            annotation = pos->second;
        }
    }
    return annotation;
}

void GLRenderer::clearPickNames()
{
    atomPickNameMap.clear();
    bondPickNameMap.clear();
    anglePickNameMap.clear();
    annotationPickNameMap.clear();
}

bool GLRenderer::isAntialiasLines()
{
    return antialiasLines;
}

void GLRenderer::setAntialiasLines(bool antialiasLines)
{
    this->antialiasLines = antialiasLines;
}

bool GLRenderer::isLineWidthDepthCued()
{
    return lineWidthDepthCued;
}

void GLRenderer::setLineWidthDepthCued(bool lineWidthDepthCued)
{
    this->lineWidthDepthCued = lineWidthDepthCued;
}

bool GLRenderer::isDimNonactiveModels()
{
    return dimNonactiveModels;
}

void GLRenderer::setDimNonactiveModels(bool dimNonactiveModels)
{
    this->dimNonactiveModels = dimNonactiveModels;
}

void GLRenderer::setAmountToDimNonactiveModels(float percent)
{
    amountToDimNonactiveModels = percent;
    percent = 1.0f - percent;
    light2->setAmbientColor(percent, percent, percent, 1.0f);
}

int GLRenderer::getFontSize()
{
    return fontSize;
}

void GLRenderer::setFontSize(int fontSize)
{
    this->fontSize = fontSize;
}

void GLRenderer::drawSurface(std::vector<SURFDOT> &dots)
{
    if (dots.size() <= 0)
    {
        return;
    }
    bool prevLight = ModernShader::isLightingEnabled();
    ModernShader::setLightingEnabled(false);
    glPointSize(2.0f);
    std::vector<ModernVertex> verts;
    verts.reserve(dots.size());
    for (unsigned int i = 0; i < dots.size(); i++)
    {
        SURFDOT &dot = dots[i];
        if (dot.color > 0)
        {
            float *c = getColor(dot.color);
            verts.push_back({ dot.x, dot.y, dot.z, 0.0f, 0.0f, 1.0f, c[0], c[1], c[2], c[3], 0.0f, 0.0f });
        }
    }
    ModernShader::drawVertices(GL_POINTS, verts.data(), static_cast<int>(verts.size()));
    ModernShader::setLightingEnabled(prevLight);
}

void GLRenderer::setJoinBondsOfSameColor(bool on)
{
    joinBondsOfSameColor = on;
}

void GLRenderer::setShowBondOrders(bool on)
{
    showBondOrders = on;
}

void GLRenderer::setPickingEnabled(bool value)
{
    pickingEnabled = value;
}

