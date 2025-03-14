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

struct ui_pi_slider_t : public ui_slider_t {
    ui_pi_slider_t(ui_slider_t &&sld):ui_slider_t(sld) {
        this->arrange_text();
    }

    std::string vtos(ui_slider_v v) override {
        const int buflen = 100;
        char buf[buflen];
        snprintf(buf, buflen, "%.3f", v);
        return std::string(buf);
    }
};

struct ui_circle_text_t : public ui_text_t {
    ui_circle_text_t(GLFWwindow *window, shaderProgram_t *program, texture_t *texture, glm::vec4 xywh)
    :ui_text_t(window, program, texture, xywh) {
        base_chars = 150.0;
        base_center_dist = 0.5;
        char_scale = 1.0;
        reverse_dir = true;
        radii_scale_1 = 1.0;
        radii_scale_2 = 1.0;
        calc();
    }

    void calc() {
        base_dist = D_PI / base_chars;
        //base_angle = atan(base_dist / base_center_dist);
        modified = true;
    }

    float base_chars;
    float base_dist;
    float base_center_dist;
    float base_angle;
    float char_scale;
    bool reverse_dir;
    float radii_scale_1;
    float radii_scale_2;

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

        
        float angle = base_angle;
        
        float max_radii = 0;

        if (reverse_dir) {
            do {
                for (int i = 1; i < count; i++) {
                    float radii = ((angle / D_PI) * base_dist + base_center_dist) * radii_scale_2;
                    max_radii = radii;
                    float char_angle = atan(base_dist / radii);
                    float second_scale = abs(pow(radii, radii_scale_1) - radii + 1);
                    angle += char_angle * second_scale;
                }
            } while (max_radii > .75);        
        }

        printf("angle: %f, max_radii: %f, radii_scale_1: %f, radii_scale_2: %f, base_dist: %f, base_center: %f\n",
            angle, max_radii, radii_scale_1, radii_scale_2, base_dist, base_center_dist);


        //angle += base_angle;

        auto b_it = string_buffer.begin();
        auto e_it = string_buffer.end();

        if (reverse_dir)
            std::swap(b_it, e_it);

        auto it = b_it;

        while (true) {
            if (reverse_dir) {
                if (it == e_it)
                    break;
                it--;
            }
            
            auto ch = *it;

            if (!reverse_dir && it++ == e_it)
                break;
                
            // Generate character verticies and texture coordinates
            glm::vec4 scr, tex;

            get_parameters()->calculate(ch, 0, 0, XYWH, scr, tex);
            
            text_t tmp[6];
            unsigned int cnt = 0;
            add_rect(&tmp[0], cnt, scr, tex);


            // Rotate around center
            float center_dist = 0.0f;
            float radii = ((angle / D_PI) * base_dist + base_center_dist) * radii_scale_2;
            float second_scale = abs(pow(radii, radii_scale_1) - radii + 1);
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
                r *= sqrt(2.0f) * char_scale * second_scale;
                r += glm::vec2(center_dist, 0) * glm::mat2(m2);
                
                r += center_pos;
                //if center_pos changes use r += offset; r += ch_pos; ?
                
                tmp[i].coords() = r;
            }

            memcpy(&buffer[vertexCount], &tmp[0], cnt * sizeof tmp[0]);
            
            vertexCount += cnt;
            angle += char_angle * (reverse_dir ? -1 : 1) * second_scale;
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

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
            circle_text->add_string("11223344556677889900");
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
        ui_slider_t *slider = new ui_pi_slider_t(ui_slider_t(window, text_program, text_texture, XYWH, min, max, value, title, limit, callback, skip_text));
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
            create_slider(sliderPos,sliderSize,0.5,0,1,"Center",[](st*m,sv v){circle_text->base_center_dist=v;circle_text->modified=true;}),
            create_slider(sliderPos,sliderSize,150,20,500,"Chars",[](st*m,sv v){circle_text->base_chars=v;circle_text->calc();}),
            create_slider(sliderPos,sliderSize,0.04,-0.2,0.2,"Angle",[](st*m,sv v){circle_text->base_angle=v;circle_text->modified=true;}),
            create_slider(sliderPos,sliderSize,1,0,2,"Radii Scale 1",[](st*m,sv v){circle_text->radii_scale_1=v;circle_text->modified=true;}),
            create_slider(sliderPos,sliderSize,1,0,2,"Radii Scale 2",[](st*m,sv v){circle_text->radii_scale_2=v;circle_text->modified=true;}),
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

        const char *pi300 = "3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273";
        const char *pi1000 = "3.1415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679821480865132823066470938446095505822317253594081284811174502841027019385211055596446229489549303819644288109756659334461284756482337867831652712019091456485669234603486104543266482133936072602491412737245870066063155881748815209209628292540917153643678925903600113305305488204665213841469519415116094330572703657595919530921861173819326117931051185480744623799627495673518857527248912279381830119491298336733624406566430860213949463952247371907021798609437027705392171762931767523846748184676694051320005681271452635608277857713427577896091736371787214684409012249534301465495853710507922796892589235420199561121290219608640344181598136297747713099605187072113499999983729780499510597317328160963185950244594553469083026425223082533446850352619311881710100031378387528865875332083814206171776691473035982534904287554687311595628638823537875937519577818577805321712268066130019278766111959092164201989";

        circle_text->set_string(pi1000);

        handle_buffersize(window, 800, 800);

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