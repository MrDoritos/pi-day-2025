#include <signal.h>

#include "common.h"
#include "texture.h"
#include "text.h"
#include "ui_element.h"
#include "ui_text.h"
#include "shader_program.h"
#include "shader.h"

constexpr float H_PI = 0.5f * M_PI;
constexpr float D_PI = 2.0f * M_PI;

struct ui_image_t : public ui_element_t {
    ui_image_t(GLFWwindow *window, glm::vec4 xywh)
    :ui_element_t(window, xywh) {}
};

struct ui_circle_text_t : public ui_text_t {
    ui_circle_text_t(GLFWwindow *window, shaderProgram_t *program, texture_t *texture, glm::vec4 xywh)
    :ui_text_t(window, program, texture, xywh) {}

    protected:
    bool mesh() override {
        currentX = currentY = vertexCount = 0;

        last_used = get_parameters();

        if (string_buffer.size() < 1) {
            modified = false;
            return glsuccess;
        }

        if (!modified)
            return glsuccess;

        text_t *buffer = new text_t[string_buffer.size() * 6];

        float angle = 0.0;
        for (auto ch : string_buffer) {
            if (ch == '\n') {
                currentX = 0;
                currentY++;
                continue;
            }

            glm::vec4 scr, tex;

            get_parameters()->calculate(ch, 0, 0, XYWH, scr, tex);
            
            text_t tmp[6];
            unsigned int cnt = 0;
            add_rect(&tmp[0], cnt, scr, tex);

            glm::mat4 matrix(1.0);
            float center_dist = 0.0f;
            float base_dist = D_PI / 150.0f;

            float base_center_dist = 0.5;
            float base_angle = atan(base_dist / base_center_dist);
            float radii = (angle / D_PI) * 0.05 + base_center_dist;

            float char_angle = atan(base_dist / radii);
            int _angle_int = (1.0/D_PI) * angle;

            //if (angle > D_PI)
            //    center_dist += ((angle - M_PI) / D_PI) * 0.1;
            //if (_angle_int)
            //center_dist += ((angle - M_PI) / D_PI) * 0.05f;
            center_dist += radii;

            glm::vec2 center_pos = (glm::vec2(XYWH) + glm::vec2(XYWH[2], XYWH[3])) * 0.5f;
            //matrix = glm::translate(matrix, glm::vec3(XYWH.x, XYWH.y, 0.0f));
            matrix = glm::rotate(matrix, -angle, glm::vec3(0,0,1.0));
            glm::mat4 m2 = glm::rotate(glm::mat4(1.0), -angle + H_PI, glm::vec3(0,0,1.0));
            //matrix = glm::translate(matrix, glm::vec3(1.0,0,0));

            glm::vec2 r_pos = tmp[0].coords();
            glm::vec2 ch_pos = glm::vec2(scr) + (glm::vec2(scr[2], scr[3]) * 0.5f);
            glm::vec2 s_pos = tmp[0].coords() - glm::vec2(XYWH);
            s_pos = s_pos * glm::mat2(matrix);
            glm::vec2 offset = center_pos - ch_pos;

            for (int i = 0; i < sizeof tmp / sizeof tmp[0]; i++) {
                auto coords = tmp[i].coords() - ch_pos;
                //coords += (coords * 0.5f);
                //coords -= glm::vec2(XYWH);
                auto r = glm::vec2(glm::vec4(coords.x, coords.y, 0, 1) * matrix);// * glm::translate(matrix, glm::vec3(1.0,0,0));
                //coords += glm::vec2(XYWH);
                //r *= 1.27f;
                r *= sqrt(2.0f);
                r += glm::vec2(glm::vec4(center_dist,0,0,0) * m2);
                r += offset;
                tmp[i].coords() = (r + ch_pos);
            }

            memcpy(&buffer[vertexCount], &tmp[0], cnt * sizeof tmp[0]);
            
            vertexCount += cnt;
            angle += char_angle;
            currentX++;
        }

        modified = false;

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        glBufferData(GL_ARRAY_BUFFER, vertexCount * sizeof * buffer, buffer, GL_STATIC_DRAW);

        buffer->set_attrib_pointers();

        delete [] buffer;

        return glsuccess;
    }
};

namespace program {
    shader_t *text_vertex;
    shader_t *text_fragment;
    shaderProgram_t *text_program;
    ui_circle_text_t *circle_text;
    ui_image_t *pi_image;
    texture_t *text_texture;
    texture_t *pi_texture;
    glm::mat4 perspective_matrix(1.0f);    

    int init_context() {
        if (!glfwInit())
            handle_error("Failed to init glfw");

        window = glfwCreateWindow(800, 800, "Pi Day 2025", 0, 0);

        if (!window)
            handle_error("Failed to create glfw window");

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSwapInterval(1);

        signal(SIGINT, handle_signal);

        return glsuccess;
    }

    int init() {
        text_vertex = new shader_t(GL_VERTEX_SHADER);
        text_fragment = new shader_t(GL_FRAGMENT_SHADER);
        text_program = new shaderProgram_t(text_vertex, text_fragment);
        text_texture = new texture_t;
        pi_texture = new texture_t;
        circle_text = new ui_circle_text_t(window, text_program, text_texture, {-1.0,-1.0,1.0,1.0});
        pi_image = new ui_image_t(window, {-1.0,-1.0,1.0,1.0});

        return glsuccess;
    }

    int load() {
        if (text_vertex->load("shaders/text_vertex.glsl") ||
            text_fragment->load("shaders/text_fragment.glsl"))
            handle_error("Failed to load shaders");
        
        if (text_program->load())
            handle_error("Failed to compile shaders");

        if (text_texture->load("assets/text.png") ||
            pi_texture->load("assets/text.png"))
            handle_error("Failed to load assets");

        pi_image->load();
        circle_text->load();

        circle_text->set_string("***Pi Day 2025 Pi Day 2025 rararrarrararrarrarrararPi025 Pi Day 2025 rararrarrararrarrarrararPi025 Pi Day 2025 rararrarrararrarrarrararPi025 Pi Day 2025 rararrarrararrarrarrararPi025 Pi Day 2025 rararrarrararrarrarrararPi025 Pi Day 2025 rararrarrararrarrarrararPi Day 2025 Pi Day 2025 Pi Day 2025 Pi Day 2025 ");

        return glsuccess;
    }

    void handle_keyboard(GLFWwindow *window, double delta_time) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            hint_exit();
    }

    void handle_buffersize(GLFWwindow *window) {
        int offsetx = 0, offsety = 0, width, height;
        glfwGetWindowSize(window, &width, &height);

        glm::vec2 screen = glm::vec2(width, height);
        glm::vec2 scale = glm::normalize(screen);
        perspective_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale * 2.0f, 1.0f));
        perspective_matrix = glm::inverse(perspective_matrix);
        glViewport(0,0,width,height);
    }
}

int main() {
    using namespace program;
    if (init_context() || init() || load())
        handle_error("Failed to start program");

    while (!glfwWindowShouldClose(window)) {
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        program::handle_keyboard(window, 1/60);

        glEnable(GL_ALPHA_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        text_program->use();
        text_program->set_m4("projection", glm::mat4(1.0) * program::perspective_matrix);
        text_program->set_f("mixFactor", -1.0);
        circle_text->render();
        pi_image->render();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    safe_exit(0);
}

void handle_signal(int signal) {
    fprintf(stderr, "Signal caught %i\n", signal);
    hint_exit();
}

void reset() {

}

void destroy() {
    glfwTerminate();
}

void safe_exit(int errcode) {
    destroy();
    exit(errcode);
}

void handle_error(const char *errstr, int errcode) {
    fprintf(stderr, "Error: %s\n", errstr);
    safe_exit(errcode);
}

void hint_exit() {
    glfwSetWindowShouldClose(window, 1);
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    program::handle_buffersize(window);
}