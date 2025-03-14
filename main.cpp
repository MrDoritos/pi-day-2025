#include <signal.h>

#include "common.h"
#include "texture.h"
#include "text.h"
#include "ui_element.h"
#include "ui_text.h"
#include "ui_slider.h"
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
    :ui_text_t(window, program, texture, xywh) {
        base_chars = 150.0;
        base_center_dist = 0.5;
        char_scale = 1.0;
        calc();
    }

    void calc() {
        base_dist = D_PI / base_chars;
        base_angle = atan(base_dist / base_center_dist);
        modified = true;
    }

    float base_chars;
    float base_dist;
    float base_center_dist;
    float base_angle;
    float char_scale;

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

        int count = string_buffer.size();
        text_t *buffer = new text_t[count * 6];

        
        float angle = 0.0;
        for (int i = 0; i < count; i++)
            angle += atan(base_dist / ((angle / D_PI) * base_dist + base_center_dist));

        //for (auto ch : string_buffer) {
        for (auto it = string_buffer.end(); it != string_buffer.begin(); it--) {
            auto ch = *it;
            // Generate character verticies and texture coordinates
            glm::vec4 scr, tex;

            get_parameters()->calculate(ch, 0, 0, XYWH, scr, tex);
            
            text_t tmp[6];
            unsigned int cnt = 0;
            add_rect(&tmp[0], cnt, scr, tex);


            // Rotate around center
            float center_dist = 0.0f;
            float radii = (angle / D_PI) * base_dist + base_center_dist;
            float char_angle = atan(base_dist / radii);

            center_dist += radii;

            glm::vec2 center_pos = (glm::vec2(XYWH) + glm::vec2(XYWH[2], XYWH[3])) * 0.5f;

            glm::mat4 m1 = glm::rotate(glm::mat4(1.0), -angle, glm::vec3(0,0,1.0));
            glm::mat4 m2 = glm::rotate(glm::mat4(1.0), -angle + H_PI, glm::vec3(0,0,1.0));
            
            glm::vec2 ch_pos = glm::vec2(scr) + (glm::vec2(scr[2], scr[3]) * 0.5f);
            glm::vec2 offset = center_pos - ch_pos;

            for (int i = 0; i < sizeof tmp / sizeof tmp[0]; i++) {
                glm::vec2 r = tmp[i].coords();
                
                r -= ch_pos;

                r = r * glm::mat2(m1);
                r *= sqrt(2.0f) * char_scale;
                r += glm::vec2(center_dist, 0) * glm::mat2(m2);
                
                r += center_pos;
                //if center_pos changes use r += offset; r += ch_pos; ?
                
                tmp[i].coords() = r;
            }

            memcpy(&buffer[vertexCount], &tmp[0], cnt * sizeof tmp[0]);
            
            vertexCount += cnt;
            //angle += char_angle;
            angle -= char_angle;
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

struct shader_text_program_t : public shaderProgram_t {
    shader_text_program_t(shaderProgram_t &&prg):shaderProgram_t(prg){}

    float mixFactor = 0.0;

    /*
    bool load() override {
        bool v = shaderProgram_t::load();
        if (v) return v;
        this->set_f("mixFactor", mixFactor);
    }
    */

    void use() override {
        shaderProgram_t::use();
        this->set_f("mixFactor", mixFactor);
    }
};

namespace program {
    shader_t *text_vertex;
    shader_t *text_fragment;
    shader_text_program_t *text_program;
    ui_circle_text_t *circle_text;
    ui_image_t *pi_image;
    texture_t *text_texture;
    texture_t *pi_texture;
    glm::mat4 perspective_matrix(1.0f);
    std::vector<ui_slider_t*> sliders;
    glm::vec4 sliderPos(0.45, -0.95, 0.5, 0.1);
    glm::vec4 sliderSize(0.0,0.2,0,0);
    ui_element_t *ui_base;

    void handle_keyboard(GLFWwindow *window, double delta_time) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            hint_exit();
    }

    void handle_buffersize(GLFWwindow *window, int width, int height) {
        ui_base->onFramebuffer(width, height);
        //int offsetx = 0, offsety = 0, width, height;
        //glfwGetWindowSize(window, &width, &height);

        glm::vec2 screen = glm::vec2(width, height);
        glm::vec2 scale = glm::normalize(screen);
        perspective_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(scale * 2.0f, 1.0f));
        perspective_matrix = glm::inverse(perspective_matrix);
        glViewport(0,0,width,height);
    }

    void handle_cursorpos(GLFWwindow *window, double x, double y) {
        ui_base->onCursor(x, y);
    }

    void handle_mousebutton(GLFWwindow *window, int button, int action, int mods) {
        ui_base->onMouse(button, action, mods);
    }

    int init_context() {
        if (!glfwInit())
            handle_error("Failed to init glfw");

        window = glfwCreateWindow(800, 800, "Pi Day 2025", 0, 0);

        if (!window)
            handle_error("Failed to create glfw window");

        glfwMakeContextCurrent(window);
        glfwSetFramebufferSizeCallback(window, handle_buffersize);
        glfwSetCursorPosCallback(window, handle_cursorpos);
        glfwSetMouseButtonCallback(window, handle_mousebutton);
        glfwSwapInterval(1);

        signal(SIGINT, handle_signal);

        return glsuccess;
    }

    ui_slider_t *create_slider(glm::vec4 &XYWH, glm::vec4 &size, float value, float min = -1, float max = 1, const std::string &title = "", ui_slider_t::callback_t callback = ui_slider_t::callback_t(), bool limit = false, bool skip_text = false) {
        ui_slider_t *slider = new ui_slider_t(window, text_program, text_texture, XYWH, min, max, value, title, limit, callback, skip_text);
        XYWH += size;
        return slider;
    }

    int init() {
        text_vertex = new shader_t(GL_VERTEX_SHADER);
        text_fragment = new shader_t(GL_FRAGMENT_SHADER);
        text_program = new shader_text_program_t(shaderProgram_t(text_vertex, text_fragment));
        text_texture = new texture_t;
        pi_texture = new texture_t;
        circle_text = new ui_circle_text_t(window, text_program, text_texture, {-1.0,-1.0,1.0,1.0});
        pi_image = new ui_image_t(window, {-1.0,-1.0,1.0,1.0});

        using st = ui_slider_t;
        using sv = st::ui_slider_v;

        ui_base = new ui_element_t(window, {-1.0,-1.0,1.0,1.0});

        ui_base->children = {
            create_slider(sliderPos,sliderSize,1,0,2,"Scale",[](st*m,sv v){circle_text->char_scale=v;circle_text->modified=true;}),
            create_slider(sliderPos,sliderSize,0.04,-0.2,0.2,"Dist",[](st*m,sv v){circle_text->base_dist=v;circle_text->modified=true;}),
            create_slider(sliderPos,sliderSize,0.5,0,1,"Center",[](st*m,sv v){circle_text->base_center_dist=v;circle_text->calc();}),
            create_slider(sliderPos,sliderSize,150,20,500,"Chars",[](st*m,sv v){circle_text->base_chars=v;circle_text->calc();}),
            create_slider(sliderPos,sliderSize,0,-1,1,"MixFactor",[](st*m,sv v){text_program->mixFactor=v;}),
        };

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
        ui_base->load();

        circle_text->set_string(" 3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273");

        return glsuccess;
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
        circle_text->render();
        pi_image->render();
        text_program->set_m4("projection", glm::mat4(1.0));
        ui_base->render();

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