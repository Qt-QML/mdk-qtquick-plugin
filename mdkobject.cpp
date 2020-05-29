#include "mdkobject.h"

#ifdef Q_OS_WINDOWS
#include <d3d11.h>
#include <wrl/client.h>
#endif

#ifdef Q_OS_MACOS
#include <Metal/Metal.h>
#endif

#include "mdk/Player.h"
#include "mdk/RenderAPI.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTextureProvider>
#include <QScreen>
#include <QTime>
#include <QtMath>
#if QT_CONFIG(opengl)
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#endif

namespace {

QStringList suffixesToMimeTypes(const QStringList &suffixes) {
    QStringList mimeTypes{};
    const QMimeDatabase db;
    for (auto &&suffix : qAsConst(suffixes)) {
        const QList<QMimeType> typeList = db.mimeTypesForFileName(suffix);
        if (!typeList.isEmpty()) {
            for (auto &&mimeType : qAsConst(typeList)) {
                const QString name = mimeType.name();
                if (!name.isEmpty()) {
                    mimeTypes.append(name);
                }
            }
        }
    }
    if (!mimeTypes.isEmpty()) {
        mimeTypes.removeDuplicates();
    }
    return mimeTypes;
}

QString timeToString(const qint64 ms) {
    return QTime(0, 0).addMSecs(ms).toString(QString::fromUtf8("hh:mm:ss"));
}

std::vector<std::string>
qStringListToStdStringVector(const QStringList &stringList) {
    if (stringList.isEmpty()) {
        return {};
    }
    std::vector<std::string> result{};
    for (auto &&string : qAsConst(stringList)) {
        result.push_back(string.toStdString());
    }
    return result;
}

} // namespace

class VideoTextureNode : public QSGTextureProvider,
                         public QSGSimpleTextureNode {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(VideoTextureNode)

public:
    explicit VideoTextureNode(MdkObject *item)
        : m_item(item), m_player(item->m_player) {
        Q_ASSERT(m_item && !m_player.isNull());
        m_window = m_item->window();
        connect(m_window, &QQuickWindow::beforeRendering, this,
                &VideoTextureNode::render);
        connect(m_window, &QQuickWindow::screenChanged, this,
                [this](QScreen *screen) {
                    Q_UNUSED(screen)
                    if (m_window->effectiveDevicePixelRatio() != m_dpr) {
                        m_item->update();
                    }
                });
    }

    ~VideoTextureNode() override {
        delete texture();
        // Release gfx resources.
#if QT_CONFIG(opengl)
        fbo_gl.reset();
#endif
        qDebug().noquote() << "Renderer destroyed.";
    }

    QSGTexture *texture() const override {
        return QSGSimpleTextureNode::texture();
    }

    void sync() {
        m_dpr = m_window->effectiveDevicePixelRatio();
        const QSizeF newSize = m_item->size() * m_dpr;
        bool needsNew = false;
        if (!texture()) {
            needsNew = true;
        }
        if (newSize != m_size) {
            needsNew = true;
            m_size = {qRound(newSize.width()), qRound(newSize.height())};
        }
        if (!needsNew) {
            return;
        }
        delete texture();
        const auto player = m_player.lock();
        if (!player) {
            return;
        }
        QSGRendererInterface *rif = m_window->rendererInterface();
        switch (rif->graphicsApi()) {
        case QSGRendererInterface::Direct3D11Rhi: {
            // Direct3D: Qt RHI's default backend on Windows. Not supported on
            // all other platforms.
#ifdef Q_OS_WINDOWS
            auto dev = (ID3D11Device *)rif->getResource(
                m_window, QSGRendererInterface::DeviceResource);
            D3D11_TEXTURE2D_DESC desc = CD3D11_TEXTURE2D_DESC(
                DXGI_FORMAT_R8G8B8A8_UNORM, m_size.width(), m_size.height(), 1,
                1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET,
                D3D11_USAGE_DEFAULT, 0, 1, 0, 0);
            if (FAILED(
                    dev->CreateTexture2D(&desc, nullptr, &m_texture_d3d11))) {
                qCritical().noquote() << "Failed to create 2D texture!";
            }
            QSGTexture *wrapper = m_window->createTextureFromNativeObject(
                QQuickWindow::NativeObjectTexture,
                m_texture_d3d11.GetAddressOf(), 0, m_size);
            setTexture(wrapper);
            qDebug() << "Got QSGTexture wrapper" << wrapper
                     << "for an D3D11 texture of size" << m_size;
            MDK_NS::D3D11RenderAPI ra{};
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx;
            dev->GetImmediateContext(&ctx);
            ra.context = ctx.Get();
            dev->CreateRenderTargetView(m_texture_d3d11.Get(), nullptr, &m_rtv);
            ra.rtv = m_rtv.Get();
            player->setRenderAPI(&ra);
#endif
        } break;
        case QSGRendererInterface::VulkanRhi: {
            // Vulkan: Qt RHI's default backend on Linux (Android). Supported on
            // Windows and macOS as well.
        } break;
        case QSGRendererInterface::MetalRhi: {
            // Metal: Qt RHI's default backend on macOS. Not supported on all
            // other platforms.
#ifdef Q_OS_MACOS
            auto dev = (__bridge id<MTLDevice>)rif->getResource(
                m_window, QSGRendererInterface::DeviceResource);
            Q_ASSERT(dev);
            MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
            desc.textureType = MTLTextureType2D;
            desc.pixelFormat = MTLPixelFormatRGBA8Unorm;
            desc.width = m_size.width();
            desc.height = m_size.height();
            desc.mipmapLevelCount = 1;
            desc.resourceOptions = MTLResourceStorageModePrivate;
            desc.storageMode = MTLStorageModePrivate;
            desc.usage =
                MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
            m_texture_mtl = [dev newTextureWithDescriptor:desc];
            QSGTexture *wrapper = m_window->createTextureFromNativeObject(
                QQuickWindow::NativeObjectTexture, &m_texture_mtl, 0, m_size);
            setTexture(wrapper);
            qDebug().noquote() << "Got QSGTexture wrapper" << wrapper
                               << "for an MTLTexture of size" << m_size;
            MDK_NS::MetalRenderAPI ra{};
            ra.texture = (__bridge void *)m_texture_mtl;
            ra.device = (__bridge void *)dev;
            ra.cmdQueue = rif->getResource(
                m_window, QSGRendererInterface::CommandQueueResource);
            player->setRenderAPI(&ra);
#endif
        } break;
        case QSGRendererInterface::OpenGL:
        case QSGRendererInterface::OpenGLRhi: {
            // OpenGL: The legacy backend, to keep compatibility of Qt 5.
            // Supported on all mainstream platforms.
#if QT_CONFIG(opengl)
            fbo_gl.reset(new QOpenGLFramebufferObject(m_size));
            auto tex = fbo_gl->texture();
            QSGTexture *wrapper = m_window->createTextureFromNativeObject(
                QQuickWindow::NativeObjectTexture, &tex, 0, m_size);
            setTexture(wrapper);
            qDebug().noquote()
                << "Got QSGTexture wrapper" << wrapper
                << "for an OpenGL texture '" << tex << "' of size" << m_size;
            // Flip y.
            player->scale(1.0f, -1.0f);
#endif
        } break;
        default:
            qCritical().noquote()
                << "QSGRendererInterface reports unknown graphics API:"
                << rif->graphicsApi();
            break;
        }
        player->setVideoSurfaceSize(m_size.width(), m_size.height());
    }

private Q_SLOTS:
    // This is hooked up to beforeRendering() so we can start our own render
    // command encoder. If we instead wanted to use the scenegraph's render
    // command encoder (targeting the window), it should be connected to
    // beforeRenderPassRecording() instead.
    void render() {
#if QT_CONFIG(opengl)
        GLuint prevFbo = 0;
        if (fbo_gl) {
            auto f = QOpenGLContext::currentContext()->functions();
            f->glGetIntegerv(GL_FRAMEBUFFER_BINDING,
                             reinterpret_cast<GLint *>(&prevFbo));
            fbo_gl->bind();
        }
#endif
        const auto player = m_player.lock();
        if (!player) {
            return;
        }
        player->renderVideo();
#if QT_CONFIG(opengl)
        if (fbo_gl) {
            auto f = QOpenGLContext::currentContext()->functions();
            f->glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
        }
#endif
    }

private:
    QQuickItem *m_item = nullptr;
    QQuickWindow *m_window = nullptr;
    QSize m_size = {};
    qreal m_dpr = 1.0;
#if QT_CONFIG(opengl)
    QScopedPointer<QOpenGLFramebufferObject> fbo_gl;
#endif
    QWeakPointer<MDK_NS::Player> m_player;
#ifdef Q_OS_WINDOWS
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture_d3d11;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv;
#endif
#ifdef Q_OS_MACOS
    id<MTLTexture> m_texture_mtl = nil;
#endif
};

MdkObject::MdkObject(QQuickItem *parent) : QQuickItem(parent) {
    // Disable status messages as they are quite annoying.
    qputenv("MDK_LOG_STATUS", "0");
    m_player.reset(new MDK_NS::Player);
    Q_ASSERT(!m_player.isNull());
    setFlag(ItemHasContents, true);
    qRegisterMetaType<ChapterInfo>();
    m_player->setRenderCallback(
        [this](void *) { QMetaObject::invokeMethod(this, "update"); });
    // MUST set before setMedia() because setNextMedia() is called when media is
    // changed
    m_player->setPreloadImmediately(false);
    m_snapshotDirectory =
        QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    connect(this, &MdkObject::sourceChanged, this, &MdkObject::fileNameChanged);
    connect(this, &MdkObject::sourceChanged, this, &MdkObject::pathChanged);
    connect(this, &MdkObject::positionChanged, this,
            &MdkObject::positionTextChanged);
    connect(this, &MdkObject::durationChanged, this,
            &MdkObject::durationTextChanged);
    initMdkHandlers();
    startTimer(50);
}

MdkObject::~MdkObject() = default;

// The beauty of using a true QSGNode: no need for complicated cleanup
// arrangements, unlike in other examples like metalunderqml, because the
// scenegraph will handle destroying the node at the appropriate time.
// Called on the render thread when the scenegraph is invalidated.
void MdkObject::invalidateSceneGraph() { m_node = nullptr; }

// Called on the gui thread if the item is removed from scene.
void MdkObject::releaseResources() { m_node = nullptr; }

QSGNode *MdkObject::updatePaintNode(QSGNode *node, UpdatePaintNodeData *data) {
    Q_UNUSED(data)
    auto n = static_cast<VideoTextureNode *>(node);
    if (!n && (width() <= 0 || height() <= 0)) {
        return nullptr;
    }
    if (!n) {
        m_node = new VideoTextureNode(this);
        n = m_node;
    }
    m_node->sync();
    n->setTextureCoordinatesTransform(QSGSimpleTextureNode::NoTransform);
    n->setFiltering(QSGTexture::Linear);
    n->setRect(0, 0, width(), height());
    // Ensure getting to beforeRendering() at some point.
    window()->update();
    return n;
}

void MdkObject::geometryChanged(const QRectF &newGeometry,
                                const QRectF &oldGeometry) {
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        update();
    }
}

QUrl MdkObject::source() const { return isStopped() ? QUrl() : m_source; }

void MdkObject::setSource(const QUrl &value) {
    if (!value.isValid() || (value == m_source) || !isMedia(value)) {
        return;
    }
    m_player->setNextMedia(nullptr, -1);
    m_player->setState(MDK_NS::PlaybackState::Stopped);
    m_player->waitFor(MDK_NS::PlaybackState::Stopped);
    // 1st url may be the same as current url
    m_player->setMedia(nullptr);
    m_player->setMedia(
        value.isLocalFile()
            ? qUtf8Printable(QDir::toNativeSeparators(value.toLocalFile()))
            : qUtf8Printable(value.url()));
    m_source = value;
    m_player->prepare(0, [this](int64_t position, bool * /*unused*/) {
        Q_UNUSED(position)
        const auto &info = m_player->mediaInfo();
        m_hasVideo = !info.video.empty();
        m_hasAudio = !info.audio.empty();
        // ### TODO: m_hasSubtitle = ...
        m_hasChapters = !info.chapters.empty();
        m_hasMetaData = !info.metadata.empty();
        if (m_hasVideo) {
            videoReConfig();
        }
        if (m_hasAudio) {
            audioReConfig();
        }
        // ### TODO: if (m_hasSubtitle) { ... }
        if (m_hasChapters) {
            m_chapters.clear();
            const auto &chapters = info.chapters;
            for (auto &&chapter : qAsConst(chapters)) {
                ChapterInfo info{};
                info.beginTime = chapter.start_time;
                info.endTime = chapter.end_time;
                info.title = QString::fromStdString(chapter.title);
                m_chapters.append(info);
            }
            Q_EMIT chaptersChanged();
        }
        if (m_hasMetaData) {
            m_metaData.clear();
            const auto &metaData = info.metadata;
            for (auto &&data : qAsConst(metaData)) {
                m_metaData.insert(QString::fromStdString(data.first),
                                  QString::fromStdString(data.second));
            }
            Q_EMIT metaDataChanged();
        }
        return true;
    });
    m_player->setState(MDK_NS::PlaybackState::Playing);
    m_player->waitFor(MDK_NS::PlaybackState::Playing);
    Q_EMIT sourceChanged();
    Q_EMIT positionChanged();
    Q_EMIT durationChanged();
    Q_EMIT seekableChanged();
    Q_EMIT formatChanged();
    Q_EMIT fileSizeChanged();
    Q_EMIT bitRateChanged();
    Q_EMIT loaded();
}

QString MdkObject::fileName() const {
    return m_source.isValid()
        ? (m_source.isLocalFile() ? m_source.fileName()
                                  : m_source.toDisplayString())
        : QString();
}

QString MdkObject::path() const {
    return m_source.isValid()
        ? (m_source.isLocalFile()
               ? QDir::toNativeSeparators(m_source.toLocalFile())
               : m_source.toDisplayString())
        : QString();
}

qint64 MdkObject::position() const {
    return isStopped() ? 0 : m_player->position();
}

void MdkObject::setPosition(const qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    seek(value);
}

qint64 MdkObject::duration() const {
    return isStopped() ? 0 : m_player->mediaInfo().duration;
}

QSize MdkObject::videoSize() const {
    if (isStopped()) {
        return {};
    }
    const auto &codec = m_player->mediaInfo().video.at(0).codec;
    return QSize(codec.width, codec.height);
}

qreal MdkObject::volume() const { return m_volume; }

void MdkObject::setVolume(const qreal value) {
    if (qFuzzyCompare(value, volume())) {
        return;
    }
    m_player->setVolume(value);
    m_volume = value;
    Q_EMIT volumeChanged();
}

bool MdkObject::mute() const { return m_mute; }

void MdkObject::setMute(const bool value) {
    if (value == mute()) {
        return;
    }
    m_player->setMute(value);
    m_mute = value;
    Q_EMIT muteChanged();
}

bool MdkObject::seekable() const {
    // Local files are always seekable, in theory.
    return (isLoaded() && m_source.isLocalFile());
}

MdkObject::PlaybackState MdkObject::playbackState() const {
    switch (m_player->state()) {
    case MDK_NS::PlaybackState::Playing:
        return PlaybackState::Playing;
    case MDK_NS::PlaybackState::Paused:
        return PlaybackState::Paused;
    case MDK_NS::PlaybackState::Stopped:
        return PlaybackState::Stopped;
    }
    return PlaybackState::Stopped;
}

void MdkObject::setPlaybackState(const MdkObject::PlaybackState value) {
    if (isStopped() || (value == playbackState())) {
        return;
    }
    switch (value) {
    case PlaybackState::Playing: {
        m_player->setState(MDK_NS::PlaybackState::Playing);
        m_player->waitFor(MDK_NS::PlaybackState::Playing);
    } break;
    case PlaybackState::Paused: {
        m_player->setState(MDK_NS::PlaybackState::Paused);
        m_player->waitFor(MDK_NS::PlaybackState::Paused);
    } break;
    case PlaybackState::Stopped: {
        m_player->setState(MDK_NS::PlaybackState::Stopped);
        m_player->waitFor(MDK_NS::PlaybackState::Stopped);
    } break;
    }
}

MdkObject::MediaStatus MdkObject::mediaStatus() const {
    switch (m_player->mediaStatus()) {
    case MDK_NS::MediaStatus::NoMedia:
        return MediaStatus::NoMedia;
    case MDK_NS::MediaStatus::Loading:
        return MediaStatus::Loading;
    case MDK_NS::MediaStatus::Loaded:
        return MediaStatus::Loaded;
    case MDK_NS::MediaStatus::Stalled:
        return MediaStatus::Stalled;
    case MDK_NS::MediaStatus::Buffering:
        return MediaStatus::Buffering;
    case MDK_NS::MediaStatus::Buffered:
        return MediaStatus::Buffered;
    case MDK_NS::MediaStatus::End:
        return MediaStatus::End;
    case MDK_NS::MediaStatus::Invalid:
        return MediaStatus::Invalid;
    default:
        return MediaStatus::Unknown;
    }
}

MdkObject::LogLevel MdkObject::logLevel() const {
    switch (MDK_NS::logLevel()) {
    case MDK_NS::LogLevel::Off:
        return LogLevel::Off;
    case MDK_NS::LogLevel::Debug:
        return LogLevel::Debug;
    case MDK_NS::LogLevel::Warning:
        return LogLevel::Warning;
    case MDK_NS::LogLevel::Error:
        return LogLevel::Critical;
    case MDK_NS::LogLevel::Info:
        return LogLevel::Info;
    default:
        return LogLevel::Debug;
    }
}

void MdkObject::setLogLevel(const MdkObject::LogLevel value) {
    if (value == logLevel()) {
        return;
    }
    switch (value) {
    case LogLevel::Off:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Off);
        break;
    case LogLevel::Debug:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Debug);
        break;
    case LogLevel::Warning:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Warning);
        break;
    case LogLevel::Critical:
    case LogLevel::Fatal:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Error);
        break;
    case LogLevel::Info:
        MDK_NS::setLogLevel(MDK_NS::LogLevel::Info);
        break;
    }
    Q_EMIT logLevelChanged();
}

qreal MdkObject::playbackRate() const {
    return isStopped() ? 1.0 : static_cast<qreal>(m_player->playbackRate());
}

void MdkObject::setPlaybackRate(const qreal value) {
    if (isStopped() || (value == playbackRate())) {
        return;
    }
    m_player->setPlaybackRate(value);
    Q_EMIT playbackRateChanged();
}

qreal MdkObject::aspectRatio() const { return static_cast<qreal>(16.0 / 9.0); }

void MdkObject::setAspectRatio(const qreal value) {
    if (isStopped() || (value == aspectRatio())) {
        return;
    }
    m_player->setAspectRatio(value);
    Q_EMIT aspectRatioChanged();
}

QString MdkObject::snapshotDirectory() const {
    return QDir::toNativeSeparators(m_snapshotDirectory);
}

void MdkObject::setSnapshotDirectory(const QString &value) {
    if (value.isEmpty() || (value == snapshotDirectory())) {
        return;
    }
    const QString val = QDir::toNativeSeparators(value);
    if (val == snapshotDirectory()) {
        return;
    }
    m_snapshotDirectory = val;
    Q_EMIT snapshotDirectoryChanged();
}

QString MdkObject::snapshotFormat() const { return m_snapshotFormat; }

void MdkObject::setSnapshotFormat(const QString &value) {
    if (value.isEmpty() || (value == snapshotFormat())) {
        return;
    }
    m_snapshotFormat = value;
    Q_EMIT snapshotFormatChanged();
}

QString MdkObject::snapshotTemplate() const { return m_snapshotTemplate; }

void MdkObject::setSnapshotTemplate(const QString &value) {
    if (value.isEmpty() || (value == snapshotTemplate())) {
        return;
    }
    m_snapshotTemplate = value;
    Q_EMIT snapshotTemplateChanged();
}

QStringList MdkObject::videoMimeTypes() {
    return suffixesToMimeTypes(videoSuffixes());
}

QStringList MdkObject::audioMimeTypes() {
    return suffixesToMimeTypes(audioSuffixes());
}

QString MdkObject::positionText() const {
    return isStopped() ? QString() : timeToString(position());
}

QString MdkObject::durationText() const {
    return isStopped() ? QString() : timeToString(duration());
}

QString MdkObject::format() const {
    return isStopped() ? QString()
                       : QString::fromUtf8(m_player->mediaInfo().format);
}

qint64 MdkObject::fileSize() const {
    return isStopped() ? 0 : m_player->mediaInfo().size;
}

qint64 MdkObject::bitRate() const {
    return isStopped() ? 0 : m_player->mediaInfo().bit_rate;
}

MdkObject::Chapters MdkObject::chapters() const {
    return isStopped() ? Chapters{} : m_chapters;
}

MdkObject::MetaData MdkObject::metaData() const {
    return isStopped() ? MetaData{} : m_metaData;
}

bool MdkObject::hardwareDecoding() const { return m_hardwareDecoding; }

void MdkObject::setHardwareDecoding(const bool value) {
    if (m_hardwareDecoding != value) {
        m_hardwareDecoding = value;
        if (m_hardwareDecoding) {
            setVideoDecoders(defaultVideoDecoders());
        } else {
            setVideoDecoders({QString::fromUtf8("FFmpeg")});
        }
        Q_EMIT hardwareDecodingChanged();
    }
}

QStringList MdkObject::videoDecoders() const { return m_videoDecoders; }

void MdkObject::setVideoDecoders(const QStringList &value) {
    if (m_videoDecoders != value) {
        m_videoDecoders =
            value.isEmpty() ? QStringList{QString::fromUtf8("FFmpeg")} : value;
        m_player->setVideoDecoders(
            qStringListToStdStringVector(m_videoDecoders));
        Q_EMIT videoDecodersChanged();
    }
}

QStringList MdkObject::audioDecoders() const { return m_audioDecoders; }

void MdkObject::setAudioDecoders(const QStringList &value) {
    if (m_audioDecoders != value) {
        // ### FIXME: value.isEmpty() ?
        m_audioDecoders = value;
        m_player->setAudioDecoders(
            qStringListToStdStringVector(m_audioDecoders));
        Q_EMIT audioDecodersChanged();
    }
}

QStringList MdkObject::audioBackends() const { return m_audioBackends; }

void MdkObject::setAudioBackends(const QStringList &value) {
    if (m_audioBackends != value) {
        // ### FIXME: value.isEmpty() ?
        m_audioBackends = value;
        m_player->setAudioBackends(
            qStringListToStdStringVector(m_audioBackends));
        Q_EMIT audioBackendsChanged();
    }
}

void MdkObject::open(const QUrl &value) {
    if (!value.isValid()) {
        return;
    }
    if ((value != m_source) && isMedia(value)) {
        setSource(value);
    }
    if (!isPlaying()) {
        play();
    }
}

void MdkObject::play() {
    if (!isPaused() || !m_source.isValid()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Playing);
    m_player->waitFor(MDK_NS::PlaybackState::Playing);
}

void MdkObject::play(const QUrl &value) {
    if (!value.isValid()) {
        return;
    }
    if ((value == m_source) && !isPlaying()) {
        play();
    }
    if ((value != m_source) && isMedia(value)) {
        open(value);
    }
}

void MdkObject::pause() {
    if (!isPlaying()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Paused);
    m_player->waitFor(MDK_NS::PlaybackState::Paused);
}

void MdkObject::stop() {
    if (isStopped()) {
        return;
    }
    m_player->setState(MDK_NS::PlaybackState::Stopped);
    m_player->waitFor(MDK_NS::PlaybackState::Stopped);
    m_source.clear();
    m_hasVideo = false;
    m_hasAudio = false;
    m_hasSubtitle = false;
    m_hasChapters = false;
    m_hasMetaData = false;
    m_chapters.clear();
    m_metaData.clear();
    Q_EMIT sourceChanged();
    Q_EMIT positionChanged();
    Q_EMIT durationChanged();
    Q_EMIT seekableChanged();
    Q_EMIT chaptersChanged();
    Q_EMIT metaDataChanged();
}

void MdkObject::seek(const qint64 value) {
    if (isStopped() || (value == position())) {
        return;
    }
    m_player->seek(value);
}

void MdkObject::rotateImage(const int value) {
    if (isStopped()) {
        return;
    }
    m_player->rotate(value);
}

void MdkObject::scaleImage(const qreal x, const qreal y) {
    if (isStopped()) {
        return;
    }
    m_player->scale(x, y);
}

void MdkObject::snapshot() {
    if (isStopped()) {
        return;
    }
    MDK_NS::Player::SnapshotRequest snapshotRequest{};
    m_player->snapshot(
        &snapshotRequest,
        [this](MDK_NS::Player::SnapshotRequest *ret, double frameTime) {
            Q_UNUSED(ret)
            const QString path =
                QString::fromUtf8("%1%2%3.%4")
                    .arg(snapshotDirectory(), QDir::separator(),
                         QString::number(frameTime), snapshotFormat());
            qDebug().noquote() << "Taking snapshot:" << path;
            return path.toStdString();
        });
}

bool MdkObject::isVideo(const QUrl &value) {
    if (value.isValid()) {
        return videoSuffixes().contains(
            QString::fromUtf8("*.") + QFileInfo(value.fileName()).suffix(),
            Qt::CaseInsensitive);
    }
    return false;
}

bool MdkObject::isAudio(const QUrl &value) {
    if (value.isValid()) {
        return audioSuffixes().contains(
            QString::fromUtf8("*.") + QFileInfo(value.fileName()).suffix(),
            Qt::CaseInsensitive);
    }
    return false;
}

bool MdkObject::isMedia(const QUrl &value) {
    return (isVideo(value) || isAudio(value));
}

bool MdkObject::currentIsVideo() const {
    return isStopped() ? false : isVideo(m_source);
}

bool MdkObject::currentIsAudio() const {
    return isStopped() ? false : isAudio(m_source);
}

bool MdkObject::currentIsMedia() const {
    return isStopped() ? false : (currentIsVideo() || currentIsAudio());
}

void MdkObject::timerEvent(QTimerEvent *event) {
    QQuickItem::timerEvent(event);
    Q_EMIT positionChanged();
}

void MdkObject::initMdkHandlers() {
    MDK_NS::setLogHandler([](MDK_NS::LogLevel level, const char *msg) {
        switch (level) {
        case MDK_NS::LogLevel::Info:
            qInfo().noquote() << msg;
            break;
        case MDK_NS::LogLevel::All:
        case MDK_NS::LogLevel::Debug:
            qDebug().noquote() << msg;
            break;
        case MDK_NS::LogLevel::Warning:
            qWarning().noquote() << msg;
            break;
        case MDK_NS::LogLevel::Error:
            qCritical().noquote() << msg;
            break;
        default:
            break;
        }
    });
    m_player->currentMediaChanged([this] {
        qDebug().noquote() << "Current media changed:" << m_player->url();
        Q_EMIT sourceChanged();
    });
    m_player->onMediaStatusChanged([this](MDK_NS::MediaStatus s) {
        Q_UNUSED(s)
        Q_EMIT mediaStatusChanged();
        return true;
    });
    m_player->onEvent([](const MDK_NS::MediaEvent &e) {
        qDebug().noquote() << "Media event:" << e.category.data()
                           << e.detail.data();
        return false;
    });
    m_player->onLoop([](int count) {
        qDebug().noquote() << "onLoop:" << count;
        return false;
    });
    m_player->onStateChanged([this](MDK_NS::PlaybackState s) {
        Q_UNUSED(s)
        Q_EMIT playbackStateChanged();
        if (isPlaying()) {
            Q_EMIT playing();
        }
        if (isPaused()) {
            Q_EMIT paused();
        }
        if (isStopped()) {
            Q_EMIT stopped();
        }
    });
}

void MdkObject::videoReConfig() { Q_EMIT videoSizeChanged(); }

void MdkObject::audioReConfig() {
    // ### TODO
}

bool MdkObject::isLoaded() const { return !isStopped(); }

bool MdkObject::isPlaying() const {
    return (m_player->state() == MDK_NS::PlaybackState::Playing);
}

bool MdkObject::isPaused() const {
    return (m_player->state() == MDK_NS::PlaybackState::Paused);
}

bool MdkObject::isStopped() const {
    return (m_player->state() == MDK_NS::PlaybackState::Stopped);
}

#include "mdkobject.moc"
