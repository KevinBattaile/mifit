#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include "graphlib.h"
#include "asplib.h"
#include "Application.h"
#include <opengl/ModernShader.h>
#include <opengl/Text.h>
#include <QPainter>

#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSettings>

#define LABEL_ROOM (4*fheight)
#define TITLE_ROOM (3*fheight)

GraphWindow::GraphWindow(QWidget *parent)
    : QOpenGLWidget(parent)
{
    /* initialize graph data */
    xlabel = "X";
    ylabel = "Y";
    title = "Graph";
    xlabelsize = 10;
    ylabelsize = 10;
    titlesize = 14;
    xdivisions = 5;
    ydivisions = 5;
    xminor = 5;
    yminor = 5;
    minx = asplib::GR_AUTO;
    maxx = asplib::GR_AUTO;
    miny = asplib::GR_AUTO;
    maxy = asplib::GR_AUTO;
    graphstyle = 0;
    bigFont = QFont();
    bigFont.setWeight(QFont::Light);
    rubber_x1 = rubber_x2 = rubber_y1 = rubber_y2 = 0.0f;
    _last_close_id = -1;

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

int Twidth(QFont &font, const std::string &c)
{
    QFontMetrics fm(font);
    return fm.horizontalAdvance(c.c_str());
}

int Theight(QFont &font)
{
    QFontMetrics fm(font);
    return fm.height();
}



void GraphWindow::keyPressEvent(QKeyEvent *key_evt)
{
    emit keyPress(key_evt->key(),
                  (key_evt->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier)));
}


bool GraphWindow::mouseHelper(QMouseEvent *e, float &fx, float &fy, float &xrange, float &yrange)
{

    QPoint pt = e->pos();
    int x = width(), y = height();
    int xorigin, yorigin, xend, yend;
    int fheight = Theight(bigFont);

    /*  add border */
    xend = x-20;
    /* this leaves LABEL_ROOM for labels below graph */
    yorigin = y-LABEL_ROOM-2*fheight;
    xorigin = 20 + Twidth(bigFont, ylabel);
    yend = TITLE_ROOM;

    xrange = (maxx-minx);
    yrange = (maxy-miny);

    fx = ((pt.x() - xorigin)*xrange/(xend-xorigin)) + minx;
    fy = ((pt.y() - yorigin)*yrange/(yend-yorigin)) + miny;

    return true;
}


void GraphWindow::finishMouseEvent(QMouseEvent *evt, float fx, float fy)
{
    std::vector<GR_POINT> close_pts;
    std::vector<unsigned int> close_ids;
    for (unsigned int i = 0; i < data.size(); ++i)
    {
        if (data[i].flag & (asplib::GR_SMALLBOX | asplib::GR_SMALLCIRCLE))
        {
            if (fabs(fx-data[i].x) < 5 && fabs(fy-data[i].y) < 5)
            {
                close_ids.push_back(i);
                close_pts.push_back(data[i]);
            }
        }
    }

    int closest = 0;
    float mindist = 999.99f;
    for (unsigned int i = 0; i < close_pts.size(); ++i)
    {
        float xsq = (fx-close_pts[i].x);
        xsq *= xsq;
        float ysq = (fy-close_pts[i].y);
        ysq *= ysq;
        float distsq = xsq+ysq;
        if (distsq < mindist)
        {
            closest = i;
            mindist = distsq;
        }
    }

    if (close_pts.size())
    {
        emit mouseOver(close_ids[closest]);
        if (evt->button() & Qt::LeftButton)
        {
            if (_last_close_id != -1 && _last_close_id < (int)data.size())
            {
                data[_last_close_id].flag &= ~asplib::GR_BOX;
            }
            _last_close_id = close_ids[closest];
            data[_last_close_id].flag |= asplib::GR_BOX;
                emit pick(close_pts[closest]);
        }
    }
}

void GraphWindow::mousePressEvent(QMouseEvent *evt)
{
    float xrange, yrange, fx, fy;
    if (!mouseHelper(evt, fx, fy, xrange, yrange))
    {
        return;
    }

    if (evt->button() & (Qt::LeftButton | Qt::RightButton))
    {
        if ((fx > maxx || fx < minx || fy > maxy || fy < miny))
        {
            graph_pop_scale();
            rubber_x1 = rubber_x2 = fx;
            rubber_y1 = rubber_y2 = fy;
            redraw();
            return;
        }
    }

    if (evt->button() & Qt::RightButton)
    {
        rubber_x1 = fx-fabs(fx*0.05);
        rubber_y1 = fy+fabs(fy*0.05);
        rubber_x2 = fx;
        rubber_y2 = fy;
        redraw();
        return;
    }

    finishMouseEvent(evt, fx, fy);
}

void GraphWindow::mouseReleaseEvent(QMouseEvent *evt)
{
    float xrange, yrange, fx, fy;
    if (!mouseHelper(evt, fx, fy, xrange, yrange))
    {
        return;
    }

    if (evt->button() & Qt::RightButton)
    {
        if (rubber_x2 < rubber_x1)
        {
            float tmp = rubber_x1;
            rubber_x1 = rubber_x2;
            rubber_x2 = tmp;
        }
        if (rubber_y2 < rubber_y1)
        {
            float tmp = rubber_y1;
            rubber_y1 = rubber_y2;
            rubber_y2 = tmp;
        }
        if (fabs(rubber_x1-rubber_x2) > xrange/20.0f  && fabs(rubber_y1-rubber_y2) > yrange/20.0f)
        {
            graph_push_scale(rubber_x1, rubber_x2, rubber_y1, rubber_y2);
        }
        rubber_x1 = rubber_x2 = rubber_y1 = rubber_y2 = 0.0f;
        redraw();
        return;
    }
    finishMouseEvent(evt, fx, fy);
}

void GraphWindow::mouseMoveEvent(QMouseEvent *evt)
{
    float xrange, yrange, fx, fy;
    if (!mouseHelper(evt, fx, fy, xrange, yrange))
    {
        return;
    }

    if (evt->buttons() & Qt::RightButton)
    {
        rubber_x2 = fx;
        rubber_y2 = fy;
        redraw();
        return;
    }
    finishMouseEvent(evt, fx, fy);
}

void GraphWindow::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(1.0, 1.0, 1.0, 1.0);

    // line rendering with the depth test enabled produces "interesting"
    // artifacts.  when two line segments start and end at the same
    // coordinates, the actual point of joining is not rendered, leading to a
    // "dotted" appearance.  You can see the same effect in the main GL
    // window, and I've seen this effect multiple times in other applications
    // glEnable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GraphWindow::resizeGL(int w, int h)
{
    glViewport(0, 0, (GLint)w, (GLint)h);
    QMatrix4x4 proj;
    proj.ortho(0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    mi::opengl::ModernShader::setProjectionMatrix(proj);
    mi::opengl::ModernShader::loadIdentityModelView();
}

void GraphWindow::redraw()
{
    update();
}

void GraphWindow::graph_scale(double x1, double x2, double y1, double y2)
{
    minx = x1;
    miny = y1;
    maxx = x2;
    maxy = y2;
}

void GraphWindow::graph_push_scale(double x1, double x2, double y1, double y2)
{
    scale_stack.push_back(minx);
    scale_stack.push_back(miny);
    scale_stack.push_back(maxx);
    scale_stack.push_back(maxy);
    minx = x1;
    miny = y1;
    maxx = x2;
    maxy = y2;
}

void GraphWindow::graph_pop_scale()
{
    if (scale_stack.size() < 4)
    {
        return;
    }
    maxy = scale_stack.back();
    scale_stack.pop_back();
    maxx = scale_stack.back();
    scale_stack.pop_back();
    miny = scale_stack.back();
    scale_stack.pop_back();
    minx = scale_stack.back();
    scale_stack.pop_back();
}

int GraphWindow::graph_point(float x, float y, int flag, GraphColor color, int width)
{
    GR_POINT gr;
    gr.x = x;
    gr.y = y;
    gr.flag = flag;
    gr.color = color;
    gr.width = width;
    data.push_back(gr);
    return data.size()-1;
}

int GraphWindow::graph_point(float x, float y, int flag, void *user_data, GraphColor color, int width)
{
    GR_POINT gr;
    gr.x = x;
    gr.y = y;
    gr.flag = flag;
    gr.data = user_data;
    gr.color = color;
    gr.width = width;
    data.push_back(gr);
    return data.size()-1;
}

bool GraphWindow::graph_replacepoint(int i, float x, float y, int flag)
{
    if (data.size() <= (unsigned int)i)
    {
        return false;
    }
    data[i].x = x;
    data[i].y = y;
    data[i].flag = flag;
    return true;
}

bool GraphWindow::graph_labelpoint(int i, const char *string)
{
    if (data.size() <= (unsigned int)i)
    {
        return false;
    }
    if (!string)
    {
        data[i].label = "";
        data[i].flag &= ~asplib::GR_LABEL;
    }
    else
    {
        data[i].label = string;
        data[i].flag |= asplib::GR_LABEL;
    }
    return (true);
}

void GraphWindow::graph_removepointsafter(int i)
{
    data.resize(i);
}

void GraphWindow::graph_curvelabel(int /* i */, const char *label, int flag, GraphColor color)
{
    CURVELABEL l;
    //if(i >= curvelabel.size() ) curvelabel.reserve(i+1);
    l.label = label;
    l.flag = flag;
    l.color = color;
    curvelabel.push_back(l);
}


static void stringSplit(std::string str, std::string delim, std::vector<std::string> &results)
{
    size_t cutAt;
    while ( (cutAt = str.find_first_of(delim)) != str.npos)
    {
        if (cutAt > 0)
        {
            results.push_back(str.substr(0, cutAt));
        }
        str = str.substr(cutAt+1);
    }
    if (str.length() > 0)
    {
        results.push_back(str);
    }
}


namespace {
static float s_graphColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

static void setGraphColor(float r, float g, float b)
{
    s_graphColor[0] = r;
    s_graphColor[1] = g;
    s_graphColor[2] = b;
    s_graphColor[3] = 1.0f;
}

static void setGraphColorUB(unsigned char r, unsigned char g, unsigned char b)
{
    setGraphColor(r / 255.0f, g / 255.0f, b / 255.0f);
}

static mi::opengl::ModernVertex makeGraphVertex(float x, float y)
{
    mi::opengl::ModernVertex v = {
        x, y, 0.0f,
        0.0f, 0.0f, 1.0f,
        s_graphColor[0], s_graphColor[1], s_graphColor[2], s_graphColor[3],
        0.0f, 0.0f
    };
    return v;
}

static void drawGraphLine(float x1, float y1, float x2, float y2)
{
    mi::opengl::ModernVertex v[2] = { makeGraphVertex(x1, y1), makeGraphVertex(x2, y2) };
    mi::opengl::ModernShader::drawVertices(GL_LINES, v, 2);
}

static void drawGraphPoint(float x, float y)
{
    mi::opengl::ModernVertex v = makeGraphVertex(x, y);
    mi::opengl::ModernShader::drawVertices(GL_POINTS, &v, 1);
}

static void drawGraphRect(float x1, float y1, float x2, float y2, bool outline = false)
{
    if (outline)
    {
        mi::opengl::ModernVertex v[4] = {
            makeGraphVertex(x1, y1), makeGraphVertex(x2, y1),
            makeGraphVertex(x2, y2), makeGraphVertex(x1, y2)
        };
        mi::opengl::ModernShader::drawVertices(GL_LINE_LOOP, v, 4);
    }
    else
    {
        mi::opengl::ModernVertex v[6] = {
            makeGraphVertex(x1, y1), makeGraphVertex(x2, y1), makeGraphVertex(x2, y2),
            makeGraphVertex(x1, y1), makeGraphVertex(x2, y2), makeGraphVertex(x1, y2)
        };
        mi::opengl::ModernShader::drawVertices(GL_TRIANGLES, v, 6);
    }
}
} // anonymous namespace

void GraphWindow::myRenderText(int x, int y, const QString &str, const QFont &font, int /*listBase*/)
{
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(str);
    Q_UNUSED(font);
}

void GraphWindow::myRenderText(double x, double y, double z, const QString &str, const QFont &font,
                               int /*listBase*/)
{
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(z);
    Q_UNUSED(str);
    Q_UNUSED(font);
}



void GraphWindow::DrawMultiLineString(float x, float y, const std::string &c)
{
    std::vector<std::string> results;
    std::string delim("\n");
    std::string str(c.c_str());

    stringSplit(str, delim, results);

    int lineSpacing = Theight(bigFont);
    for (unsigned int i = 0; i < results.size(); ++i)
    {
        myRenderText(x, y, 0.0, results[i].c_str());
        y += lineSpacing;
    }
}

void drawCircle (int x, int y, int r)
{
    std::vector<mi::opengl::ModernVertex> verts;
    mi::opengl::ModernVertex center = makeGraphVertex((float)x, (float)y);
    for (int i = 0; i < 360; i += 18)
    {
        float cx1 = (float)r * cos(i*DEG2RAD) + x;
        float cy1 = (float)r * sin(i*DEG2RAD) + y;
        float cx2 = (float)r * cos((i + 18)*DEG2RAD) + x;
        float cy2 = (float)r * sin((i + 18)*DEG2RAD) + y;
        verts.push_back(center);
        verts.push_back(makeGraphVertex(cx1, cy1));
        verts.push_back(makeGraphVertex(cx2, cy2));
    }
    mi::opengl::ModernShader::drawVertices(GL_TRIANGLES, verts.data(), verts.size());
}



#define String(a, b, c) { DrawMultiLineString((a), (b), (c)); }

#define Point(a, b) { drawGraphPoint((a), (b)); }

#define Line(x1, y1, x2, y2) { drawGraphLine((x1), (y1), (x2), (y2)); }

#define DashLine(a, b, c, d) { Line((a), (b), (c), (d)); }
#define DotLine(a, b, c, d)  { Line((a), (b), (c), (d)); }

#define Label(a, b, c) {Line((a), (b)+8, (a), (b)+14); String((a), (b)+14, (c));}

#define Cross(a, b)      {Line((a)-5, (b), (a)+5, (b)); Line((a), (b)-5, (a), (b)+5);}
#define SmallCross(a, b) {Line((a)-3, (b), (a)+3, (b)); Line((a), (b)-3, (a), (b)+3);}

#define Box(a, b)      {drawGraphRect((a)-4, (b)-4, (a)+4, (b)+4);}
#define SmallBox(a, b) {drawGraphRect((a)-2, (b)-2, (a)+2, (b)+2); }

#define Circle(a, b)      {drawCircle((a), (b), 5);}
#define SmallCircle(a, b) {drawCircle((a), (b), 3);}


void GraphWindow::paintGL()
{
    if (!isVisible())
        return;

    char buff[200];
    int xorigin, yorigin, xend, yend;
    float xint, yint, x1, y1, x2, y2;
    float xmin = FLT_MAX, xmax = -FLT_MAX, ymin = FLT_MAX, ymax = -FLT_MAX;
    int i;
    float res;
    int ndata = data.size();
    int fheight = Theight(bigFont);
    float ydiv, xdiv;
    int x = width(), y = height();

    resizeGL(x, y); // should be unnecessary, but isn't.  something's screwing up the matrix and this resets it.

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    mi::opengl::ModernShader::loadIdentityModelView();
    mi::opengl::ModernShader::setLightingEnabled(false);
    mi::opengl::ModernShader::setFog(false, 0.0f, 1.0f, QVector4D(0.0f, 0.0f, 0.0f, 1.0f));
    setGraphColor(0.0f, 0.0f, 0.0f);
    glLineWidth(1.0);

    /*  add border */
    xend = x-20;
    /* this leaves LABEL_ROOM for labels below graph */
    yorigin = y-LABEL_ROOM-2*fheight;
    xorigin = 20 + Twidth(bigFont, ylabel);
    yend = TITLE_ROOM;

    String(xorigin, yend-TITLE_ROOM/2, title);

    /* draw axes */
    Line(xorigin, yorigin, xorigin, yend);
    Line(xorigin, yorigin, xend, yorigin);
    Line(xend, yorigin, xend, yend);
    Line(xorigin, yend, xend, yend);
    /* label axes */
    x1 = xorigin-10-Twidth(bigFont, ylabel);
    /* if even ydivisions then label is moved to avoid collision */
    y1 = yorigin/2+yend/2 - (ydivisions+1)%2*fheight;
    String(x1, y1, ylabel);
    x1 = xorigin/2.+xend/2.-Twidth(bigFont, xlabel)/2.;
    y1 = yorigin+2*fheight;
    String(x1, y1, xlabel);
    if (ndata > 0)
    {
        for (i = 0; i < ndata; i++)
        {
            if (xmax < data[i].x)
            {
                xmax = data[i].x;
            }
            if (ymax < data[i].y)
            {
                ymax = data[i].y;
            }
            if (xmin > data[i].x)
            {
                xmin = data[i].x;
            }
            if (ymin > data[i].y)
            {
                ymin = data[i].y;
            }
        }
        if (maxx != asplib::GR_AUTO)
        {
            xmax = maxx;
        }
        if (maxy != asplib::GR_AUTO)
        {
            ymax = maxy;
        }
        if (minx != asplib::GR_AUTO)
        {
            xmin = minx;
        }
        if (miny != asplib::GR_AUTO)
        {
            ymin = miny;
        }
        if (xmax != xmin) /* i.e. if there is only 1 data point */
        {
            xint = (xend-xorigin)/(xmax-xmin); /* interval for 1 in x */
        }
        else
        {
            xint = 1.0;
        }
        if (ymax != ymin)
        {
            yint = (yend-yorigin)/(ymax-ymin);
        }
        else
        {
            yint = 1.0;
        }
        ydiv = ydivisions;
        xdiv = xdivisions;
        for (i = 0; i <= xdivisions; i++)
        {
            if (!(graphstyle&asplib::GR_CUBEDTORES))
            {
                sprintf(buff, "%0.3g", xmin+(float)i*(xmax-xmin)/xdiv);
            }
            else
            {
                res = xmin+(float)i*(xmax-xmin)/xdiv;
                res = 0.5/ pow(res, 1.0f/3.0f);
                sprintf(buff, "%0.3g", res);
            }
            x1 = xorigin+i*(xend-xorigin)/xdiv;
            y1 = yorigin + fheight;
            String(x1-Twidth(bigFont, buff)/2.0, y1, buff);
            x2 = x1;
            y1 = yorigin;
            y2 = yend;
            DashLine(x1, y1, x2, y2);
        }
        for (i = 0; i <= ydivisions; i++)
        {
            sprintf(buff, "%0.3g", ymin+(float)i*(ymax-ymin)/ydiv);
            y1 = yorigin+i*(yend-yorigin)/ydiv;
            x1 = xorigin-2-Twidth(bigFont, buff);
            String(x1, y1  + fheight/2.0, buff);
            y2 = y1;
            x1 = xorigin;
            x2 = xend;
            DashLine(x1, y1, x2, y2);
        }
        /* minor x marks */
        ydiv = ydivisions*yminor;
        xdiv = xdivisions*xminor;
        for (i = 0; i < xdiv; i++)
        {
            x1 = x2 = xorigin+i*(xend-xorigin)/xdiv;
            y1 = yorigin;
            y2 = yorigin-4;
            Line((int)x1, (int)y1, (int)x2, (int)y2);
            y1 = yend;
            y2 = yend+4;
            Line((int)x1, (int)y1, (int)x2, (int)y2);
        }
        for (i = 0; i < ydiv; i++)
        {
            y1 = y2 = yorigin+i*(yend-yorigin)/ydiv;
            x1 = xorigin;
            x2 = xorigin+4;
            Line((int)x1, (int)y1, (int)x2, (int)y2);
            x1 = xend;
            x2 = xend-4;
            Line((int)x1, (int)y1, (int)x2, (int)y2);
        }

        if (ndata > 1)
        {
            for (i = 0; i < ndata-1; i++)
            {
                setGraphColorUB(data[i].color._r, data[i].color._g, data[i].color._b);
                glLineWidth(1.0f);

                // implement clipping
                if (data[i].x < minx || data[i].x > maxx
                    || data[i].y < miny || data[i].y > maxy)
                {
                    continue;
                }


                x1 = xorigin + (data[i].x - xmin)*xint;
                y1 = yorigin + (data[i].y - ymin)*yint;
                x2 = xorigin + (data[i+1].x - xmin)*xint;
                y2 = yorigin + (data[i+1].y - ymin)*yint;
                if (!(data[i].flag & asplib::GR_BREAK))
                {
                    if (data[i].flag&asplib::GR_DASH)
                    {
                        DashLine(x1, y1, x2, y2);
                    }
                    else
                    {
                        Line((int)x1, (int)y1, (int)x2, (int)y2);
                    }
                }
                if (data[i].flag & asplib::GR_CROSS)
                {
                    Cross(x1, y1);
                }
                if (data[i].flag & asplib::GR_BOX)
                {
                    Box(x1, y1);
                }
                if (data[i].flag & asplib::GR_CIRCLE)
                {
                    Circle(x1, y1);
                }
                if (data[i].flag & asplib::GR_SMALLCROSS)
                {
                    SmallCross(x1, y1);
                }
                if (data[i].flag & asplib::GR_SMALLBOX)
                {
                    SmallBox(x1, y1);
                }
                if (data[i].flag & asplib::GR_SMALLCIRCLE)
                {
                    SmallCircle(x1, y1);
                }
                if (data[i].flag & asplib::GR_POINT)
                {
                    Point(x1, y1);
                }
                if (data[i].flag & asplib::GR_LABEL)
                {
                    Label(x1, y1, data[i].label);
                }
            }
        }
        /* do last point's cross */
        i = ndata -1;
        x1 = xorigin + (data[i].x - xmin)*xint;
        y1 = yorigin + (data[i].y - ymin)*yint;
        if (data[i].flag & asplib::GR_CROSS)
        {
            Cross(x1, y1);
        }
        if (data[i].flag & asplib::GR_BOX)
        {
            Box(x1, y1);
        }
        if (data[i].flag & asplib::GR_CIRCLE)
        {
            Circle(x1, y1);
        }
        if (data[i].flag & asplib::GR_SMALLCIRCLE)
        {
            SmallCircle(x1, y1);
        }
        if (data[i].flag & asplib::GR_SMALLCROSS)
        {
            SmallCross(x1, y1);
        }
        if (data[i].flag & asplib::GR_SMALLBOX)
        {
            SmallBox(x1, y1);
        }
        if (data[i].flag & asplib::GR_POINT)
        {
            Point(x1, y1);
        }
        if (data[i].flag & asplib::GR_LABEL)
        {
            Label(x1, y1, data[i].label);
        }

        setGraphColor(0.0f, 0.0f, 0.0f);

        /* draw any curve labels */
        x1 = xorigin;
        y1 = yorigin+2.5*fheight;

        for (i = 0; (unsigned int)i < curvelabel.size(); i++)
        {
            setGraphColorUB(curvelabel[i].color._r, curvelabel[i].color._g, curvelabel[i].color._b);

            float x3, y3;
            if (curvelabel[i].label.c_str()[0] != 0)
            {
                //		if(!(curvelabel[i].flag&asplib::GR_BREAK)){
                //			if(curvelabel[i].flag&asplib::GR_DASH){
                //				DashLine(x1,y1,x2,y2);
                //			}else {Line((int)x1,(int)y1,(int)x2,(int)y2);}}
                if (curvelabel[i].flag & asplib::GR_CROSS)
                {
                    Cross(x1, y1);
                }
                if (curvelabel[i].flag & asplib::GR_BOX)
                {
                    Box(x1, y1);
                }
                if (curvelabel[i].flag & asplib::GR_CIRCLE)
                {
                    Circle(x1, y1);
                }
                if (curvelabel[i].flag & asplib::GR_SMALLCROSS)
                {
                    SmallCross(x1, y1);
                }
                if (curvelabel[i].flag & asplib::GR_SMALLBOX)
                {
                    SmallBox(x1, y1);
                }
                if (curvelabel[i].flag & asplib::GR_SMALLCIRCLE)
                {
                    SmallCircle(x1, y1);
                }
                if (curvelabel[i].flag & asplib::GR_POINT)
                {
                    Point(x1, y1);
                }
                x3 = x1+8;
                y3 = y1+fheight/4.0;
                String(x3, y3, curvelabel[i].label);
                y1 += fheight*3/2;
                if (y1 > y-fheight)
                {
                    x1 += (xend-xorigin)/2.0;
                    y1 = yorigin+2.5*fheight;
                }
            }
        }


        // rubber band
        if (rubber_x1 != rubber_x2 && rubber_y1 != rubber_y2)
        {
            setGraphColor(0.5f, 0.5f, 0.5f);

            x1 = xorigin + (rubber_x1 - xmin)*xint;
            y1 = yorigin + (rubber_y1 - ymin)*yint;
            x2 = xorigin + (rubber_x2 - xmin)*xint;
            y2 = yorigin + (rubber_y2 - ymin)*yint;
            drawGraphRect(x1, y1, x2, y2, true);
        }
    }
}
