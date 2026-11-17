// Isolate Qt/Wayland/Vulkan lifetime bugs from the reader and MuPDF.
#include <QApplication>
#include <QTimer>
#include <QRhiWidget>
#include <rhi/qrhi.h>

class Canvas final : public QRhiWidget {
public:
    Canvas() {
        setApi(Api::Vulkan);
        setDebugLayerEnabled(qEnvironmentVariableIntValue("SIOYEK_VULKAN_VALIDATION") != 0);
        resize(320, 240);
        connect(this, &QRhiWidget::renderFailed, this, [] { qFatal("Qt Vulkan initialization failed"); });
    }
    int frames = 0;
    void render(QRhiCommandBuffer* cb) override {
        cb->beginPass(renderTarget(), Qt::white, {1, 0});
        cb->endPass();
        if (++frames == 3)
            QTimer::singleShot(0, this, &QWidget::close);
        else
            update();
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    Canvas canvas;
    canvas.show();
    const int result = app.exec();
    if (result != 0 || canvas.frames < 3)
        return 1;
    qInfo("PASS: isolated Qt Vulkan create, render and close");
    return 0;
}
