#include <QApplication>
#include <QWidget>
#include "oapv.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QDebug>

class VideoGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    VideoGLWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {}

    void setFrameData(const QByteArray &data, int w, int h) {
        m_data = data;
        m_width = w;
        m_height = h;
        update();
    }

protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glEnable(GL_TEXTURE_2D);

        const char *vsrc =
                "attribute vec4 vertexIn; \n"
                "attribute vec2 textureIn; \n"
                "varying vec2 textureOut; \n"
                "void main(void) { \n"
                "    gl_Position = vertexIn; \n"
                "    textureOut = textureIn; \n"
                "} \n";

        // FRAGMENT SHADER: Sửa lỗi màu xanh bằng cách nhân hệ số 64.0
        const char *fsrc =
                "varying vec2 textureOut; \n"
                "uniform sampler2D tex_y; \n"
                "uniform sampler2D tex_u; \n"
                "uniform sampler2D tex_v; \n"
                "void main(void) { \n"
                "    float y, u, v; \n"
                "    // Nhân 64.0616 để đưa dải 10-bit về 1.0 (65535/1023) \n"
                "    y = texture2D(tex_y, textureOut).r * 64.0616; \n"
                "    u = texture2D(tex_u, textureOut).r * 64.0616 - 0.5; \n"
                "    v = texture2D(tex_v, textureOut).r * 64.0616 - 0.5; \n"
                "    \n"
                "    // Công thức BT.709 Limited Range chuẩn \n"
                "    y = (y - 0.0627) * 1.1643; \n"
                "    vec3 rgb; \n"
                "    rgb.r = y + 1.5748 * v; \n"
                "    rgb.g = y - 0.1873 * u - 0.4681 * v; \n"
                "    rgb.b = y + 1.8556 * u; \n"
                "    gl_FragColor = vec4(rgb, 1.0); \n"
                "} \n";

        m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);
        m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);
        m_program.link();
        glGenTextures(3, m_textures);

        for(int i=0; i<3; i++) {
            glBindTexture(GL_TEXTURE_2D, m_textures[i]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }

    void paintGL() override {
        if (m_data.isEmpty() || m_width <= 0) return;

        m_program.bind();
        const unsigned short* pData = (const unsigned short*)m_data.data();

        // 4:2:2 10-bit: Y là W*H, U và V là (W/2)*H
        int y_size = m_width * m_height;
        int uv_size = (m_width / 2) * m_height;

        renderPlane(0, pData, m_width, m_height);                        // Y
        renderPlane(1, pData + y_size, m_width / 2, m_height);           // U
        renderPlane(2, pData + y_size + uv_size, m_width / 2, m_height);  // V

        static const GLfloat vertices[] = { -1,1, 1,1, -1,-1, 1,-1 };
        static const GLfloat texCoords[] = { 0,0, 1,0, 0,1, 1,1 };

        m_program.setAttributeArray("vertexIn", GL_FLOAT, vertices, 2);
        m_program.enableAttributeArray("vertexIn");
        m_program.setAttributeArray("textureIn", GL_FLOAT, texCoords, 2);
        m_program.enableAttributeArray("textureIn");

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        m_program.release();
    }

private:
    void renderPlane(int index, const unsigned short* data, int w, int h) {
        glActiveTexture(GL_TEXTURE0 + index);
        glBindTexture(GL_TEXTURE_2D, m_textures[index]);
        // GL_R16: Mỗi pixel chiếm 2 bytes (16 bit), OpenGL lấy kênh Red
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_RED, GL_UNSIGNED_SHORT, data);
        glUniform1i(m_program.uniformLocation(index == 0 ? "tex_y" : (index == 1 ? "tex_u" : "tex_v")), index);
    }

    QOpenGLShaderProgram m_program;
    GLuint m_textures[3];
    QByteArray m_data;
    int m_width = 0, m_height = 0;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget w;
    QVBoxLayout *layout = new QVBoxLayout(&w);
    VideoGLWidget *videoWidget = new VideoGLWidget();
    layout->addWidget(videoWidget);

    QString filePath = "/Users/khanh12354/WorkPlace/pattern1_yuv422p10le_320x240_25fps.y4m";
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) return -1;

    QByteArray header = file->readLine();
    int width = 0, height = 0;
    QList<QByteArray> tokens = header.split(' ');
    for (const QByteArray &t : tokens) {
        if (t.startsWith('W')) width = t.mid(1).toInt();
        if (t.startsWith('H')) height = t.mid(1).toInt();
    }

    // YUV422 10-bit: (W*H + W/2*H + W/2*H) * 2 bytes = W*H*4
    qint64 frameSize = (qint64)width * height * 4;

    QTimer *timer = new QTimer(&w);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        QByteArray frameMarker = file->read(6);
        if (frameMarker.size() < 6) {
            file->seek(0);
            file->readLine();
            return;
        }

        QByteArray frameData = file->read(frameSize);
        if (frameData.size() == (int)frameSize) {
            videoWidget->setFrameData(frameData, width, height);
        }
    });

    timer->start(40);
    w.resize(320, 240);
    w.show();
    return app.exec();
}
