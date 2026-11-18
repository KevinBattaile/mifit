#include <opengl/ModernShader.h>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <cmath>
#include <map>

namespace mi
{
namespace opengl
{

namespace
{

struct ContextResources
{
    QOpenGLShaderProgram *program = nullptr;
    QOpenGLVertexArrayObject *vao = nullptr;
    QOpenGLBuffer *vbo = nullptr;
    QOpenGLBuffer *ebo = nullptr;

    QOpenGLVertexArrayObject *sphereVao = nullptr;
    QOpenGLBuffer *sphereVbo = nullptr;
    QOpenGLBuffer *sphereEbo = nullptr;
    int sphereIndexCount = 0;

    QOpenGLVertexArrayObject *cylVao = nullptr;
    QOpenGLBuffer *cylVbo = nullptr;
    QOpenGLBuffer *cylEbo = nullptr;
    int cylIndexCount = 0;

    bool initialized = false;
};

static std::map<QOpenGLContext*, ContextResources> s_contextMap;

static QMatrix4x4 s_projection;
static std::stack<QMatrix4x4> s_projStack;

static QMatrix4x4 s_modelView;
static std::stack<QMatrix4x4> s_mvStack;

static bool s_lightingEnabled = false;
static QVector4D s_lightPosEye(0.0f, 20.0f, 10.0f, 0.0f);
static QVector4D s_lightAmbient(0.6f, 0.6f, 0.6f, 1.0f);
static QVector4D s_lightDiffuse(1.0f, 1.0f, 1.0f, 1.0f);
static QVector4D s_lightSpecular(1.0f, 1.0f, 1.0f, 1.0f);

static bool s_fogEnabled = false;
static float s_fogStart = 10.0f;
static float s_fogEnd = 100.0f;
static QVector4D s_fogColor(0.2f, 0.2f, 0.2f, 1.0f);

static bool s_clipEnabled[2] = { false, false };
static QVector4D s_clipPlaneEye[2];

static bool s_pickingMode = false;
static std::vector<unsigned int> s_pickNameStack;

static const char *s_vertexShaderSrc = R"(
#version 150 core
in vec3 aPos;
in vec3 aNormal;
in vec4 aColor;
in vec2 aTexCoord;

uniform mat4 uModelView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

out vec3 vEyePos;
out vec3 vNormal;
out vec4 vColor;
out vec2 vTexCoord;

void main()
{
    vec4 eyePos = uModelView * vec4(aPos, 1.0);
    vEyePos = eyePos.xyz;
    vNormal = uNormalMatrix * aNormal;
    vColor = aColor;
    vTexCoord = aTexCoord;
    gl_Position = uProjection * eyePos;
}
)";

static const char *s_fragmentShaderSrc = R"(
#version 150 core
in vec3 vEyePos;
in vec3 vNormal;
in vec4 vColor;
in vec2 vTexCoord;

uniform bool uUseLighting;
uniform bool uUseTexture;
uniform bool uUseFog;
uniform bool uPickingMode;
uniform vec4 uPickColor;

uniform vec4 uLightPos;
uniform vec4 uLightAmbient;
uniform vec4 uLightDiffuse;
uniform vec4 uLightSpecular;

uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

uniform bool uClipEnabled0;
uniform bool uClipEnabled1;
uniform vec4 uClipPlane0;
uniform vec4 uClipPlane1;

uniform sampler2D uTexture;

out vec4 FragColor;

void main()
{
    if (uClipEnabled0 && dot(vec4(vEyePos, 1.0), uClipPlane0) < 0.0)
        discard;
    if (uClipEnabled1 && dot(vec4(vEyePos, 1.0), uClipPlane1) < 0.0)
        discard;

    if (uPickingMode)
    {
        FragColor = uPickColor;
        return;
    }

    vec4 baseColor = vColor;
    if (uUseTexture)
    {
        float alpha = texture(uTexture, vTexCoord).a;
        baseColor.a *= alpha;
        if (baseColor.a < 0.01)
            discard;
    }

    vec4 finalColor = baseColor;
    if (uUseLighting)
    {
        vec3 N = normalize(vNormal);
        if (!gl_FrontFacing)
            N = -N;
        vec3 L;
        if (uLightPos.w == 0.0)
            L = normalize(uLightPos.xyz);
        else
            L = normalize(uLightPos.xyz - vEyePos);
        float NdotL = max(dot(N, L), 0.0);
        vec3 ambient = uLightAmbient.rgb * baseColor.rgb;
        vec3 diffuse = uLightDiffuse.rgb * baseColor.rgb * NdotL;
        vec3 spec = vec3(0.0);
        if (NdotL > 0.0)
        {
            vec3 V = normalize(-vEyePos);
            vec3 H = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);
            spec = uLightSpecular.rgb * pow(NdotH, 16.0) * 0.25;
        }
        finalColor = vec4(clamp(ambient + diffuse + spec, 0.0, 1.0), baseColor.a);
    }

    if (uUseFog)
    {
        float dist = length(vEyePos);
        float fogFactor = clamp((uFogEnd - dist) / max(uFogEnd - uFogStart, 0.0001), 0.0, 1.0);
        finalColor.rgb = mix(uFogColor.rgb, finalColor.rgb, fogFactor);
    }

    FragColor = finalColor;
}
)";

static const char *s_vertexShaderFallbackSrc = R"(
#version 120
attribute vec3 aPos;
attribute vec3 aNormal;
attribute vec4 aColor;
attribute vec2 aTexCoord;

uniform mat4 uModelView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;

varying vec3 vEyePos;
varying vec3 vNormal;
varying vec4 vColor;
varying vec2 vTexCoord;

void main()
{
    vec4 eyePos = uModelView * vec4(aPos, 1.0);
    vEyePos = eyePos.xyz;
    vNormal = uNormalMatrix * aNormal;
    vColor = aColor;
    vTexCoord = aTexCoord;
    gl_Position = uProjection * eyePos;
}
)";

static const char *s_fragmentShaderFallbackSrc = R"(
#version 120
varying vec3 vEyePos;
varying vec3 vNormal;
varying vec4 vColor;
varying vec2 vTexCoord;

uniform bool uUseLighting;
uniform bool uUseTexture;
uniform bool uUseFog;
uniform bool uPickingMode;
uniform vec4 uPickColor;

uniform vec4 uLightPos;
uniform vec4 uLightAmbient;
uniform vec4 uLightDiffuse;
uniform vec4 uLightSpecular;

uniform float uFogStart;
uniform float uFogEnd;
uniform vec4 uFogColor;

uniform bool uClipEnabled0;
uniform bool uClipEnabled1;
uniform vec4 uClipPlane0;
uniform vec4 uClipPlane1;

uniform sampler2D uTexture;

void main()
{
    if (uClipEnabled0 && dot(vec4(vEyePos, 1.0), uClipPlane0) < 0.0)
        discard;
    if (uClipEnabled1 && dot(vec4(vEyePos, 1.0), uClipPlane1) < 0.0)
        discard;

    if (uPickingMode)
    {
        gl_FragColor = uPickColor;
        return;
    }

    vec4 baseColor = vColor;
    if (uUseTexture)
    {
        float alpha = texture2D(uTexture, vTexCoord).a;
        baseColor.a *= alpha;
        if (baseColor.a < 0.01)
            discard;
    }

    vec4 finalColor = baseColor;
    if (uUseLighting)
    {
        vec3 N = normalize(vNormal);
        if (!gl_FrontFacing)
            N = -N;
        vec3 L;
        if (uLightPos.w == 0.0)
            L = normalize(uLightPos.xyz);
        else
            L = normalize(uLightPos.xyz - vEyePos);
        float NdotL = max(dot(N, L), 0.0);
        vec3 ambient = uLightAmbient.rgb * baseColor.rgb;
        vec3 diffuse = uLightDiffuse.rgb * baseColor.rgb * NdotL;
        vec3 spec = vec3(0.0);
        if (NdotL > 0.0)
        {
            vec3 V = normalize(-vEyePos);
            vec3 H = normalize(L + V);
            float NdotH = max(dot(N, H), 0.0);
            spec = uLightSpecular.rgb * pow(NdotH, 16.0) * 0.25;
        }
        finalColor = vec4(clamp(ambient + diffuse + spec, 0.0, 1.0), baseColor.a);
    }

    if (uUseFog)
    {
        float dist = length(vEyePos);
        float fogFactor = clamp((uFogEnd - dist) / max(uFogEnd - uFogStart, 0.0001), 0.0, 1.0);
        finalColor.rgb = mix(uFogColor.rgb, finalColor.rgb, fogFactor);
    }

    gl_FragColor = finalColor;
}
)";

void setupVertexAttribs(QOpenGLFunctions *f)
{
    const int stride = sizeof(ModernVertex);
    f->glEnableVertexAttribArray(0);
    f->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(ModernVertex, px)));
    f->glEnableVertexAttribArray(1);
    f->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(ModernVertex, nx)));
    f->glEnableVertexAttribArray(2);
    f->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(ModernVertex, r)));
    f->glEnableVertexAttribArray(3);
    f->glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(offsetof(ModernVertex, u)));
}

void buildUnitSphere(ContextResources &res, QOpenGLFunctions *f)
{
    const int stacks = 16;
    const int slices = 16;
    std::vector<ModernVertex> verts;
    std::vector<unsigned int> indices;

    for (int i = 0; i <= stacks; ++i)
    {
        float phi = static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(stacks);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int j = 0; j <= slices; ++j)
        {
            float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(j) / static_cast<float>(slices);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            ModernVertex v;
            v.nx = cosTheta * sinPhi;
            v.ny = sinTheta * sinPhi;
            v.nz = cosPhi;
            v.px = v.nx;
            v.py = v.ny;
            v.pz = v.nz;
            v.r = 1.0f; v.g = 1.0f; v.b = 1.0f; v.a = 1.0f;
            v.u = static_cast<float>(j) / slices;
            v.v = static_cast<float>(i) / stacks;
            verts.push_back(v);
        }
    }

    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            unsigned int first = i * (slices + 1) + j;
            unsigned int second = first + slices + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    res.sphereVao = new QOpenGLVertexArrayObject();
    res.sphereVao->create();
    res.sphereVao->bind();

    res.sphereVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    res.sphereVbo->create();
    res.sphereVbo->bind();
    res.sphereVbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    res.sphereVbo->allocate(verts.data(), static_cast<int>(verts.size() * sizeof(ModernVertex)));

    res.sphereEbo = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    res.sphereEbo->create();
    res.sphereEbo->bind();
    res.sphereEbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    res.sphereEbo->allocate(indices.data(), static_cast<int>(indices.size() * sizeof(unsigned int)));

    setupVertexAttribs(f);
    res.sphereVao->release();
    res.sphereIndexCount = static_cast<int>(indices.size());
}

void buildUnitCylinder(ContextResources &res, QOpenGLFunctions *f)
{
    const int slices = 12;
    std::vector<ModernVertex> verts;
    std::vector<unsigned int> indices;

    for (int i = 0; i <= 1; ++i)
    {
        float z = static_cast<float>(i);
        for (int j = 0; j <= slices; ++j)
        {
            float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(j) / static_cast<float>(slices);
            float c = std::cos(theta);
            float s = std::sin(theta);

            ModernVertex v;
            v.nx = c;
            v.ny = s;
            v.nz = 0.0f;
            v.px = c;
            v.py = s;
            v.pz = z;
            v.r = 1.0f; v.g = 1.0f; v.b = 1.0f; v.a = 1.0f;
            v.u = static_cast<float>(j) / slices;
            v.v = z;
            verts.push_back(v);
        }
    }

    for (int j = 0; j < slices; ++j)
    {
        unsigned int p0 = j;
        unsigned int p1 = j + 1;
        unsigned int p2 = (slices + 1) + j;
        unsigned int p3 = (slices + 1) + j + 1;

        indices.push_back(p0);
        indices.push_back(p1);
        indices.push_back(p2);

        indices.push_back(p1);
        indices.push_back(p3);
        indices.push_back(p2);
    }

    res.cylVao = new QOpenGLVertexArrayObject();
    res.cylVao->create();
    res.cylVao->bind();

    res.cylVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    res.cylVbo->create();
    res.cylVbo->bind();
    res.cylVbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    res.cylVbo->allocate(verts.data(), static_cast<int>(verts.size() * sizeof(ModernVertex)));

    res.cylEbo = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
    res.cylEbo->create();
    res.cylEbo->bind();
    res.cylEbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
    res.cylEbo->allocate(indices.data(), static_cast<int>(indices.size() * sizeof(unsigned int)));

    setupVertexAttribs(f);
    res.cylVao->release();
    res.cylIndexCount = static_cast<int>(indices.size());
}

ContextResources *getResources()
{
    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx)
        return nullptr;

    ContextResources &res = s_contextMap[ctx];
    if (!res.initialized)
    {
        QOpenGLFunctions *f = ctx->functions();
        res.program = new QOpenGLShaderProgram();
        bool ok = res.program->addShaderFromSourceCode(QOpenGLShader::Vertex, s_vertexShaderSrc);
        if (ok)
            ok = res.program->addShaderFromSourceCode(QOpenGLShader::Fragment, s_fragmentShaderSrc);
        if (!ok)
        {
            res.program->removeAllShaders();
            res.program->addShaderFromSourceCode(QOpenGLShader::Vertex, s_vertexShaderFallbackSrc);
            res.program->addShaderFromSourceCode(QOpenGLShader::Fragment, s_fragmentShaderFallbackSrc);
        }
        res.program->bindAttributeLocation("aPos", 0);
        res.program->bindAttributeLocation("aNormal", 1);
        res.program->bindAttributeLocation("aColor", 2);
        res.program->bindAttributeLocation("aTexCoord", 3);
        res.program->link();

        res.vao = new QOpenGLVertexArrayObject();
        res.vao->create();
        res.vao->bind();

        res.vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        res.vbo->create();
        res.vbo->bind();
        res.vbo->setUsagePattern(QOpenGLBuffer::DynamicDraw);

        res.ebo = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
        res.ebo->create();
        res.ebo->bind();
        res.ebo->setUsagePattern(QOpenGLBuffer::DynamicDraw);

        setupVertexAttribs(f);
        res.vao->release();

        buildUnitSphere(res, f);
        buildUnitCylinder(res, f);

        QObject::connect(ctx, &QOpenGLContext::aboutToBeDestroyed, [ctx]() {
            auto it = s_contextMap.find(ctx);
            if (it != s_contextMap.end())
            {
                delete it->second.program;
                delete it->second.vao;
                delete it->second.vbo;
                delete it->second.ebo;
                delete it->second.sphereVao;
                delete it->second.sphereVbo;
                delete it->second.sphereEbo;
                delete it->second.cylVao;
                delete it->second.cylVbo;
                delete it->second.cylEbo;
                s_contextMap.erase(it);
            }
        });

        res.initialized = true;
    }
    return &res;
}

void bindUniforms(QOpenGLShaderProgram *prog, bool useTexture)
{
    prog->setUniformValue("uModelView", s_modelView);
    prog->setUniformValue("uProjection", s_projection);
    prog->setUniformValue("uNormalMatrix", s_modelView.normalMatrix());

    prog->setUniformValue("uUseLighting", s_lightingEnabled && !s_pickingMode);
    prog->setUniformValue("uUseTexture", useTexture && !s_pickingMode);
    prog->setUniformValue("uUseFog", s_fogEnabled && !s_pickingMode);
    prog->setUniformValue("uPickingMode", s_pickingMode);
    if (s_pickingMode)
    {
        prog->setUniformValue("uPickColor", ModernShader::currentPickColor());
    }

    prog->setUniformValue("uLightPos", s_lightPosEye);
    prog->setUniformValue("uLightAmbient", s_lightAmbient);
    prog->setUniformValue("uLightDiffuse", s_lightDiffuse);
    prog->setUniformValue("uLightSpecular", s_lightSpecular);

    prog->setUniformValue("uFogStart", s_fogStart);
    prog->setUniformValue("uFogEnd", s_fogEnd);
    prog->setUniformValue("uFogColor", s_fogColor);

    prog->setUniformValue("uClipEnabled0", s_clipEnabled[0]);
    prog->setUniformValue("uClipEnabled1", s_clipEnabled[1]);
    prog->setUniformValue("uClipPlane0", s_clipPlaneEye[0]);
    prog->setUniformValue("uClipPlane1", s_clipPlaneEye[1]);
    prog->setUniformValue("uTexture", 0);
}

} // anonymous namespace

ModernShader *ModernShader::instance()
{
    static ModernShader inst;
    return &inst;
}

void ModernShader::setProjectionMatrix(const QMatrix4x4 &proj)
{
    s_projection = proj;
}

QMatrix4x4 ModernShader::projectionMatrix()
{
    return s_projection;
}

void ModernShader::pushProjection()
{
    s_projStack.push(s_projection);
}

void ModernShader::popProjection()
{
    if (!s_projStack.empty())
    {
        s_projection = s_projStack.top();
        s_projStack.pop();
    }
}

void ModernShader::setModelViewMatrix(const QMatrix4x4 &mv)
{
    s_modelView = mv;
}

QMatrix4x4 ModernShader::modelViewMatrix()
{
    return s_modelView;
}

void ModernShader::loadIdentityModelView()
{
    s_modelView.setToIdentity();
}

void ModernShader::multModelView(const QMatrix4x4 &m)
{
    s_modelView = s_modelView * m;
}

void ModernShader::multModelView(const float *colMajor16)
{
    // QMatrix4x4 constructor takes row-major values, so transpose column-major array
    QMatrix4x4 m(
        colMajor16[0], colMajor16[4], colMajor16[8],  colMajor16[12],
        colMajor16[1], colMajor16[5], colMajor16[9],  colMajor16[13],
        colMajor16[2], colMajor16[6], colMajor16[10], colMajor16[14],
        colMajor16[3], colMajor16[7], colMajor16[11], colMajor16[15]
    );
    s_modelView = s_modelView * m;
}

void ModernShader::translateModelView(float x, float y, float z)
{
    s_modelView.translate(x, y, z);
}

void ModernShader::scaleModelView(float x, float y, float z)
{
    s_modelView.scale(x, y, z);
}

void ModernShader::pushModelView()
{
    s_mvStack.push(s_modelView);
}

void ModernShader::popModelView()
{
    if (!s_mvStack.empty())
    {
        s_modelView = s_mvStack.top();
        s_mvStack.pop();
    }
}

void ModernShader::setLightingEnabled(bool enabled)
{
    s_lightingEnabled = enabled;
}

bool ModernShader::isLightingEnabled()
{
    return s_lightingEnabled;
}

void ModernShader::setLight(const QVector4D &posEye, const QVector4D &ambient,
                            const QVector4D &diffuse, const QVector4D &specular)
{
    s_lightPosEye = posEye;
    s_lightAmbient = ambient;
    s_lightDiffuse = diffuse;
    s_lightSpecular = specular;
}

void ModernShader::setFog(bool enabled, float start, float end, const QVector4D &color)
{
    s_fogEnabled = enabled;
    s_fogStart = start;
    s_fogEnd = end;
    s_fogColor = color;
}

void ModernShader::setClipPlane(int index, bool enabled, const QVector4D &worldPlane)
{
    if (index < 0 || index > 1)
        return;
    s_clipEnabled[index] = enabled;
    if (enabled)
    {
        // Transform world-space plane equation to eye space: p_eye = (M^-1)^T * p_world
        QMatrix4x4 invT = s_modelView.inverted().transposed();
        s_clipPlaneEye[index] = invT * worldPlane;
    }
}

void ModernShader::disableClipPlanes()
{
    s_clipEnabled[0] = false;
    s_clipEnabled[1] = false;
}

void ModernShader::setPickingMode(bool enabled)
{
    s_pickingMode = enabled;
    if (enabled)
        s_pickNameStack.clear();
}

bool ModernShader::isPickingMode()
{
    return s_pickingMode;
}

void ModernShader::pushPickName(unsigned int name)
{
    s_pickNameStack.push_back(name);
}

void ModernShader::popPickName()
{
    if (!s_pickNameStack.empty())
        s_pickNameStack.pop_back();
}

void ModernShader::clearPickNames()
{
    s_pickNameStack.clear();
}

QVector4D ModernShader::currentPickColor()
{
    unsigned int primary = s_pickNameStack.size() >= 1 ? s_pickNameStack[0] : 0;
    unsigned int secondary = s_pickNameStack.size() >= 2 ? s_pickNameStack[1] : 0;
    unsigned char r = static_cast<unsigned char>(primary & 0xFF);
    unsigned char g = static_cast<unsigned char>((primary >> 8) & 0xFF);
    unsigned char b = static_cast<unsigned char>(((primary >> 16) & 0x3F) | ((secondary & 0x03) << 6));
    return QVector4D(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
}

void ModernShader::decodePickColor(unsigned char r, unsigned char g, unsigned char b,
                                   unsigned int &primaryId, unsigned int &secondaryId)
{
    primaryId = static_cast<unsigned int>(r)
              | (static_cast<unsigned int>(g) << 8)
              | (static_cast<unsigned int>(b & 0x3F) << 16);
    secondaryId = static_cast<unsigned int>((b >> 6) & 0x03);
}

void ModernShader::drawVertices(GLenum mode, const ModernVertex *vertices, int count,
                                bool useTexture, GLuint textureId)
{
    if (count <= 0 || !vertices)
        return;

    ContextResources *res = getResources();
    if (!res)
        return;

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    res->program->bind();
    bindUniforms(res->program, useTexture);

    if (useTexture && textureId != 0)
    {
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, textureId);
    }

    res->vao->bind();
    res->vbo->bind();
    res->vbo->allocate(vertices, count * static_cast<int>(sizeof(ModernVertex)));
    setupVertexAttribs(f);

    f->glDrawArrays(mode, 0, count);

    res->vbo->release();
    res->vao->release();
    res->program->release();
}

void ModernShader::drawIndexed(GLenum mode, const ModernVertex *vertices, int vertexCount,
                               const unsigned int *indices, int indexCount,
                               bool useTexture, GLuint textureId)
{
    if (vertexCount <= 0 || indexCount <= 0 || !vertices || !indices)
        return;

    ContextResources *res = getResources();
    if (!res)
        return;

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();
    res->program->bind();
    bindUniforms(res->program, useTexture);

    if (useTexture && textureId != 0)
    {
        f->glActiveTexture(GL_TEXTURE0);
        f->glBindTexture(GL_TEXTURE_2D, textureId);
    }

    res->vao->bind();
    res->vbo->bind();
    res->vbo->allocate(vertices, vertexCount * static_cast<int>(sizeof(ModernVertex)));
    setupVertexAttribs(f);

    res->ebo->bind();
    res->ebo->allocate(indices, indexCount * static_cast<int>(sizeof(unsigned int)));

    f->glDrawElements(mode, indexCount, GL_UNSIGNED_INT, nullptr);

    res->vbo->release();
    res->vao->release();
    res->program->release();
}

void ModernShader::drawUnitSphere(const QVector3D &center, float radius, const QVector4D &color, int, int)
{
    ContextResources *res = getResources();
    if (!res)
        return;

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    pushModelView();
    translateModelView(center.x(), center.y(), center.z());
    scaleModelView(radius, radius, radius);

    res->program->bind();
    bindUniforms(res->program, false);

    res->sphereVao->bind();
    res->sphereEbo->bind();
    f->glDisableVertexAttribArray(2);
    f->glVertexAttrib4f(2, color.x(), color.y(), color.z(), color.w());

    f->glDrawElements(GL_TRIANGLES, res->sphereIndexCount, GL_UNSIGNED_INT, nullptr);

    f->glEnableVertexAttribArray(2);
    res->sphereVao->release();
    res->program->release();

    popModelView();
}

void ModernShader::drawCylinder(const QVector3D &start, const QVector3D &end, float radius, const QVector4D &color)
{
    ContextResources *res = getResources();
    if (!res)
        return;

    QVector3D axis = end - start;
    float len = axis.length();
    if (len < 1e-6f)
        return;

    QVector3D a = axis / len;
    QVector3D b(std::fabs(a.x()) < 0.9f ? 1.0f : 0.0f,
                std::fabs(a.x()) < 0.9f ? 0.0f : 1.0f,
                0.0f);
    QVector3D c = QVector3D::crossProduct(a, b).normalized();
    b = QVector3D::crossProduct(c, a).normalized();

    QMatrix4x4 cylTransform(
        c.x() * radius, b.x() * radius, a.x() * len, start.x(),
        c.y() * radius, b.y() * radius, a.y() * len, start.y(),
        c.z() * radius, b.z() * radius, a.z() * len, start.z(),
        0.0f,           0.0f,           0.0f,        1.0f
    );

    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    pushModelView();
    multModelView(cylTransform);

    res->program->bind();
    bindUniforms(res->program, false);

    res->cylVao->bind();
    res->cylEbo->bind();
    f->glDisableVertexAttribArray(2);
    f->glVertexAttrib4f(2, color.x(), color.y(), color.z(), color.w());

    f->glDrawElements(GL_TRIANGLES, res->cylIndexCount, GL_UNSIGNED_INT, nullptr);

    f->glEnableVertexAttribArray(2);
    res->cylVao->release();
    res->program->release();

    popModelView();
}

} // namespace opengl
} // namespace mi
