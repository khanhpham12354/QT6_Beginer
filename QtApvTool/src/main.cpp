#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

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

    std::string filePath = "/Users/khanh12354/WorkPlace/pattern1_yuv422p10le_320x240_25fps.y4m";
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return -1;

    std::string header;
    std::getline(file, header);
    int width = 320, height = 240; 

    GLFWwindow* window = glfwCreateWindow(width, height, "YUV 10-bit HDR Optimized (25 FPS)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int program = glCreateProgram();
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // Cấp phát bộ nhớ 1 lần duy nhất ở đây (nullptr có nghĩa là chưa nạp dữ liệu)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, w, h, 0, GL_RED, GL_UNSIGNED_SHORT, nullptr);
        glUniform1i(glGetUniformLocation(program, name), unit);
    };

    // Khởi tạo vùng nhớ cho 3 Plane (Y, U, V)
    initTex(0, 0, "tex_y", width, height);
    initTex(1, 1, "tex_u", width / 2, height);
    initTex(2, 2, "tex_v", width / 2, height);

    size_t y_size = width * height;
    size_t uv_size = (width / 2) * height;
    std::vector<unsigned char> frameData((y_size + 2 * uv_size) * 2);

    double lastTime = glfwGetTime();
    double frameDuration = 1.0 / 25.0;

    while (!glfwWindowShouldClose(window)) {
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= frameDuration) {
            lastTime = currentTime;

            char marker[6];
            if (!file.read(marker, 6)) {
                file.clear();
                file.seekg(header.length() + 1);
                continue;
            }

            file.read((char*)frameData.data(), frameData.size());

            glPixelStorei(GL_UNPACK_ALIGNMENT, 2); 

            // Cập nhật nội dung Plane Y
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures[0]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_SHORT, frameData.data());

            // Cập nhật nội dung Plane U
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textures[1]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height, GL_RED, GL_UNSIGNED_SHORT, frameData.data() + y_size * 2);

            // Cập nhật nội dung Plane V
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, textures[2]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width / 2, height, GL_RED, GL_UNSIGNED_SHORT, frameData.data() + (y_size + uv_size) * 2);

            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glfwSwapBuffers(window);
        }
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
