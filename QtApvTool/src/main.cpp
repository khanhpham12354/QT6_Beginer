#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

// Include OpenAPV headers
#include "oapv.h"
#include "oapv_app_args.h"
#include "oapv_app_util.h"
#include "oapv_app_y4m.h"

// Shader sources
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D tex_y;
uniform sampler2D tex_u;
uniform sampler2D tex_v;

void main() {
    float raw_y = texture(tex_y, TexCoord).r * 65535.0;
    float raw_u = texture(tex_u, TexCoord).r * 65535.0;
    float raw_v = texture(tex_v, TexCoord).r * 65535.0;

    float y = (raw_y - 64.0) / 876.0;
    float u = (raw_u - 512.0) / 896.0;
    float v = (raw_v - 512.0) / 896.0;

    vec3 rgb;
    rgb.r = y + 1.4746 * v;
    rgb.g = y - 0.16455 * u - 0.57135 * v;
    rgb.b = y + 1.8814 * u;

    FragColor = vec4(clamp(rgb, 0.0, 1.0), 1.0);
}
)";

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &source, nullptr);
    glCompileShader(id);
    return id;
}

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_RED_BITS, 10);
    glfwWindowHint(GLFW_GREEN_BITS, 10);
    glfwWindowHint(GLFW_BLUE_BITS, 10);
    glfwWindowHint(GLFW_ALPHA_BITS, 2);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

    const char* filePath = "/Users/khanh12354/WorkPlace/pattern1_yuv422p10le_320x240_25fps.y4m";
    FILE* fp = fopen(filePath, "rb");
    if (!fp) return -1;

    y4m_params_t y4m_params;
    if (y4m_header_parser(fp, &y4m_params) < 0) {
        fclose(fp);
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(y4m_params.w, y4m_params.h, "OpenAPV 10-bit Player", nullptr, nullptr);
    if (!window) { fclose(fp); glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    int cs = OAPV_CS_SET(y4m_params.color_format, y4m_params.bit_depth, 0);
    oapv_imgb_t* imgb = imgb_create(y4m_params.w, y4m_params.h, cs);
    if (!imgb) { fclose(fp); return -1; }

    unsigned int program = glCreateProgram();
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glUseProgram(program);

    float vertices[] = { -1,1,0,0,0, 1,1,0,1,0, -1,-1,0,0,1, 1,-1,0,1,1 };
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    unsigned int textures[3];
    glGenTextures(3, textures);
    auto initTex = [&](int i, int unit, const char* name, int w, int h) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_RED, GL_UNSIGNED_SHORT, nullptr);
        glUniform1i(glGetUniformLocation(program, name), unit);
    };

    // Sử dụng imgb->w và imgb->h cho tất cả các texture để đảm bảo tính nhất quán
    initTex(0, 0, "tex_y", imgb->w[0], imgb->h[0]);
    initTex(1, 1, "tex_u", imgb->w[1], imgb->h[1]);
    initTex(2, 2, "tex_v", imgb->w[2], imgb->h[2]);

    double lastTime = glfwGetTime();
    double frameDuration = (double)y4m_params.fps_den / y4m_params.fps_num;

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= frameDuration) {
            lastTime = currentTime;

            if (imgb_read(fp, imgb, y4m_params.w, y4m_params.h, 1) < 0) {
                fseek(fp, 0, SEEK_SET);
                y4m_header_parser(fp, &y4m_params);
                continue;
            }

            glPixelStorei(GL_UNPACK_ALIGNMENT, 2); 

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[0]);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, imgb->s[0] / 2);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgb->w[0], imgb->h[0], GL_RED, GL_UNSIGNED_SHORT, imgb->a[0]);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textures[1]);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, imgb->s[1] / 2);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgb->w[1], imgb->h[1], GL_RED, GL_UNSIGNED_SHORT, imgb->a[1]);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, textures[2]);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, imgb->s[2] / 2);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, imgb->w[2], imgb->h[2], GL_RED, GL_UNSIGNED_SHORT, imgb->a[2]);

            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glfwSwapBuffers(window);
        }
        glfwPollEvents();
    }

    imgb->release(imgb);
    fclose(fp);
    glfwTerminate();
    return 0;
}
