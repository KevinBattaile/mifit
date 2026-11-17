#ifndef QGLFORMAT_COMPAT_H
#define QGLFORMAT_COMPAT_H

#include <QSurfaceFormat>
#include <QFlags>

namespace QGL {
    enum FormatOption {
        DoubleBuffer      = 0x0001,
        DepthBuffer       = 0x0002,
        Rgba              = 0x0004,
        AlphaChannel      = 0x0008,
        AccumBuffer       = 0x0010,
        StencilBuffer     = 0x0020,
        StereoBuffers     = 0x0040,
        DirectRendering   = 0x0080,
        HasOverlay        = 0x0100,
        SampleBuffers     = 0x0200,
        DeprecatedFunctions = 0x0400,
        SingleBuffer      = DoubleBuffer    << 16,
        NoDepthBuffer     = DepthBuffer     << 16,
        ColorIndex        = Rgba            << 16,
        NoAlphaChannel    = AlphaChannel    << 16,
        NoAccumBuffer     = AccumBuffer     << 16,
        NoStencilBuffer   = StencilBuffer   << 16,
        NoStereoBuffers   = StereoBuffers   << 16,
        IndirectRendering = DirectRendering << 16,
        NoOverlay         = HasOverlay      << 16,
        NoSampleBuffers   = SampleBuffers   << 16
    };
    Q_DECLARE_FLAGS(FormatOptions, FormatOption)
}
Q_DECLARE_OPERATORS_FOR_FLAGS(QGL::FormatOptions)

class QGLFormat {
public:
    enum OpenGLVersionFlag {
        OpenGL_Version_None               = 0x0000,
        OpenGL_Version_1_1                = 0x0001,
        OpenGL_Version_1_2                = 0x0002,
        OpenGL_Version_1_3                = 0x0004,
        OpenGL_Version_1_4                = 0x0008,
        OpenGL_Version_1_5                = 0x0010,
        OpenGL_Version_2_0                = 0x0020,
        OpenGL_Version_2_1                = 0x0040,
        OpenGL_ES_Common_Version_1_0      = 0x0080,
        OpenGL_ES_CommonLite_Version_1_0  = 0x0100,
        OpenGL_ES_Common_Version_1_1      = 0x0200,
        OpenGL_ES_CommonLite_Version_1_1  = 0x0400,
        OpenGL_ES_Version_2_0             = 0x0800,
        OpenGL_Version_3_0                = 0x1000,
        OpenGL_Version_3_1                = 0x2000,
        OpenGL_Version_3_2                = 0x4000,
        OpenGL_Version_3_3                = 0x8000,
        OpenGL_Version_4_0                = 0x10000
    };
    Q_DECLARE_FLAGS(OpenGLVersionFlags, OpenGLVersionFlag)

    enum OpenGLContextProfile {
        NoProfile,
        CoreProfile,
        CompatibilityProfile
    };

    QGLFormat() = default;
    QGLFormat(QGL::FormatOptions options, int plane = 0) : plane_(plane) { setOption(options); }

    bool doubleBuffer() const { return doubleBuffer_; }
    void setDoubleBuffer(bool enable) { doubleBuffer_ = enable; }

    bool depth() const { return depth_; }
    void setDepth(bool enable) { depth_ = enable; }
    int depthBufferSize() const { return depthSize_; }
    void setDepthBufferSize(int size) { depthSize_ = size; }

    bool rgba() const { return rgba_; }
    void setRgba(bool enable) { rgba_ = enable; }

    bool alpha() const { return alpha_; }
    void setAlpha(bool enable) { alpha_ = enable; }
    int alphaBufferSize() const { return alphaSize_; }
    void setAlphaBufferSize(int size) { alphaSize_ = size; }

    bool accum() const { return accum_; }
    void setAccum(bool enable) { accum_ = enable; }
    int accumBufferSize() const { return accumSize_; }
    void setAccumBufferSize(int size) { accumSize_ = size; }

    bool stencil() const { return stencil_; }
    void setStencil(bool enable) { stencil_ = enable; }
    int stencilBufferSize() const { return stencilSize_; }
    void setStencilBufferSize(int size) { stencilSize_ = size; }

    bool stereo() const { return stereo_; }
    void setStereo(bool enable) { stereo_ = enable; }

    bool directRendering() const { return directRendering_; }
    void setDirectRendering(bool enable) { directRendering_ = enable; }

    bool hasOverlay() const { return overlay_; }
    void setOverlay(bool enable) { overlay_ = enable; }

    bool sampleBuffers() const { return sampleBuffers_; }
    void setSampleBuffers(bool enable) { sampleBuffers_ = enable; }
    int samples() const { return samples_; }
    void setSamples(int numSamples) { samples_ = numSamples; }

    int redBufferSize() const { return redSize_; }
    void setRedBufferSize(int size) { redSize_ = size; }
    int greenBufferSize() const { return greenSize_; }
    void setGreenBufferSize(int size) { greenSize_ = size; }
    int blueBufferSize() const { return blueSize_; }
    void setBlueBufferSize(int size) { blueSize_ = size; }

    int swapInterval() const { return swapInterval_; }
    void setSwapInterval(int interval) { swapInterval_ = interval; }

    int plane() const { return plane_; }
    void setPlane(int plane) { plane_ = plane; }

    void setOption(QGL::FormatOptions opt) {
        if (opt & QGL::DoubleBuffer) doubleBuffer_ = true;
        if (opt & QGL::SingleBuffer) doubleBuffer_ = false;
        if (opt & QGL::DepthBuffer) depth_ = true;
        if (opt & QGL::NoDepthBuffer) depth_ = false;
        if (opt & QGL::Rgba) rgba_ = true;
        if (opt & QGL::ColorIndex) rgba_ = false;
        if (opt & QGL::AlphaChannel) alpha_ = true;
        if (opt & QGL::NoAlphaChannel) alpha_ = false;
        if (opt & QGL::AccumBuffer) accum_ = true;
        if (opt & QGL::NoAccumBuffer) accum_ = false;
        if (opt & QGL::StencilBuffer) stencil_ = true;
        if (opt & QGL::NoStencilBuffer) stencil_ = false;
        if (opt & QGL::StereoBuffers) stereo_ = true;
        if (opt & QGL::NoStereoBuffers) stereo_ = false;
        if (opt & QGL::DirectRendering) directRendering_ = true;
        if (opt & QGL::IndirectRendering) directRendering_ = false;
        if (opt & QGL::HasOverlay) overlay_ = true;
        if (opt & QGL::NoOverlay) overlay_ = false;
        if (opt & QGL::SampleBuffers) sampleBuffers_ = true;
        if (opt & QGL::NoSampleBuffers) sampleBuffers_ = false;
    }

    bool testOption(QGL::FormatOptions opt) const {
        if (opt & QGL::DoubleBuffer) return doubleBuffer_;
        if (opt & QGL::DepthBuffer) return depth_;
        if (opt & QGL::Rgba) return rgba_;
        if (opt & QGL::AlphaChannel) return alpha_;
        if (opt & QGL::AccumBuffer) return accum_;
        if (opt & QGL::StencilBuffer) return stencil_;
        if (opt & QGL::StereoBuffers) return stereo_;
        if (opt & QGL::DirectRendering) return directRendering_;
        if (opt & QGL::HasOverlay) return overlay_;
        if (opt & QGL::SampleBuffers) return sampleBuffers_;
        return false;
    }

    static QGLFormat defaultFormat() {
        static QGLFormat def;
        return def;
    }
    static void setDefaultFormat(const QGLFormat &f) {
        QSurfaceFormat sf = QSurfaceFormat::defaultFormat();
        sf.setDepthBufferSize(f.depth() ? f.depthBufferSize() : 0);
        sf.setStencilBufferSize(f.stencil() ? f.stencilBufferSize() : 0);
        sf.setAlphaBufferSize(f.alpha() ? f.alphaBufferSize() : 0);
        sf.setSamples(f.sampleBuffers() ? f.samples() : 0);
        sf.setStereo(f.stereo());
        sf.setSwapBehavior(f.doubleBuffer() ? QSurfaceFormat::DoubleBuffer : QSurfaceFormat::SingleBuffer);
        QSurfaceFormat::setDefaultFormat(sf);
    }

    static QGLFormat defaultOverlayFormat() { return QGLFormat(); }
    static void setDefaultOverlayFormat(const QGLFormat &) {}
    static bool hasOpenGL() { return true; }
    static bool hasOpenGLOverlays() { return false; }
    static OpenGLVersionFlags openGLVersionFlags() {
        return OpenGLVersionFlags(OpenGL_Version_2_1) | OpenGL_Version_2_0 | OpenGL_Version_1_5 |
               OpenGL_Version_1_4 | OpenGL_Version_1_3 | OpenGL_Version_1_2 | OpenGL_Version_1_1;
    }

    friend bool operator==(const QGLFormat &, const QGLFormat &) { return true; }
    friend bool operator!=(const QGLFormat &, const QGLFormat &) { return false; }

private:
    bool doubleBuffer_ = true;
    bool depth_ = true;
    bool rgba_ = true;
    bool alpha_ = false;
    bool accum_ = false;
    bool stencil_ = true;
    bool stereo_ = false;
    bool directRendering_ = true;
    bool overlay_ = false;
    bool sampleBuffers_ = false;
    int depthSize_ = 24;
    int alphaSize_ = 8;
    int accumSize_ = 0;
    int stencilSize_ = 8;
    int samples_ = 4;
    int redSize_ = 8;
    int greenSize_ = 8;
    int blueSize_ = 8;
    int swapInterval_ = -1;
    int plane_ = 0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGLFormat::OpenGLVersionFlags)

#endif // QGLFORMAT_COMPAT_H
