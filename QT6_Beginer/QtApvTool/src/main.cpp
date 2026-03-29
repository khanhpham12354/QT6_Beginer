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
    // 1. Lấy dữ liệu 10-bit từ texture 16-bit (GL_R16)
    // Dữ liệu yuv422p10le thường nằm ở 10 bit thấp (0-1023) trong 16 bit
    // texture().r trả về giá trị chuẩn hóa [0.0, 1.0], tương ứng [0, 65535]
    float raw_y = texture(tex_y, TexCoord).r * 65535.0;
    float raw_u = texture(tex_u, TexCoord).r * 65535.0;
    float raw_v = texture(tex_v, TexCoord).r * 65535.0;

    // 2. Chuyển đổi YUV Limited Range (10-bit) sang Normalized Float
    // Y: [64, 940], U/V: [64, 960]
    float y = (raw_y - 64.0) / 876.0;
    float u = (raw_u - 512.0) / 896.0;
    float v = (raw_v - 512.0) / 896.0;

    // 3. Ma trận chuyển đổi YUV -> RGB (Chuẩn BT.2020 cho HDR)
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

    // Cấu hình OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    // THIẾT LẬP ĐẦU RA 10-BIT/HDR
    glfwWindowHint(GLFW_RED_BITS, 10);
    glfwWindowHint(GLFW_GREEN_BITS, 10);
    glfwWindowHint(GLFW_BLUE_BITS, 10);
    glfwWindowHint(GLFW_ALPHA_BITS, 2);

    // Yêu cầu Framebuffer chất lượng cao (giúp ích cho HDR trên macOS)
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

#ifdef GLFW_HDR_ENABLED
    glfwWindowHint(GLFW_HDR_ENABLED, GLFW_TRUE);
#endif

    // Mở file Y4M
    std::string filePath = "/Users/khanh12354/WorkPlace/pattern1_yuv422p10le_320x240_25fps.y4m";
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return -1;

    std::string header;
    std::getline(file, header);
    int width = 320, height = 240; // Default hoặc parse từ header

    GLFWwindow* window = glfwCreateWindow(width, height, "YUV 10-bit HDR Display", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    unsigned int shader = glCreateProgram();
    glAttachShader(shader, vs);
    glAttachShader(shader, fs);
    glLinkProgram(shader);
    glUseProgram(shader);

    // Khởi tạo VAO/VBO (giản lược)
    float vertices[] = { -1,1,0,0,0, 1,1,0,1,0, -1,-1,0,0,1, 1,-1,0,1,1 };
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);

    // Texture Setup
    unsigned int textures[3];
    glGenTextures(3, textures);
    auto initTex = [&](int i, int unit, const char* name) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(glGetUniformLocation(shader, name), unit);
    };

    initTex(0, 0, "tex_y");
    initTex(1, 1, "tex_u");
    initTex(2, 2, "tex_v");

    size_t y_size = width * height;
    size_t uv_size = (width / 2) * height;
    std::vector<unsigned char> frameData((y_size + 2 * uv_size) * 2);

    while (!glfwWindowShouldClose(window)) {
        char marker[6];
        file.read(marker, 6);
        if (file.gcount() < 6) {
            file.clear();
            file.seekg(header.length()+1);
            continue;
        }

        file.read((char*)frameData.data(), frameData.size());

//        glPixelStorei(GL_UNPACK_ALIGNMENT, 2); // Quan trọng cho dữ liệu 16-bit

        // Upload Plane Y
        glActiveTexture(GL_TEXTURE0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, width, height, 0, GL_RED, GL_UNSIGNED_SHORT, frameData.data());

        // Upload Plane U
        glActiveTexture(GL_TEXTURE1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, width/2, height, 0, GL_RED, GL_UNSIGNED_SHORT, frameData.data() + y_size*2);

        // Upload Plane V
        glActiveTexture(GL_TEXTURE2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16, width/2, height, 0, GL_RED, GL_UNSIGNED_SHORT, frameData.data() + (y_size + uv_size)*2);

        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
