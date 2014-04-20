#include <stdlib.h>
#include <stdio.h>

#include "render/render.h"
#include "render/shader.h"
#include "render/command.h"
#include "render/tape.h"

// Global variables shared for the renderer
GLFWwindow* window;

// Shader programs for eval and blit stages
GLuint eval_program;
GLuint blit_program;

int gl_init(char* shader_dir)
{
    // Initialize the library
    if (!glfwInit())    return -1;

    glfwWindowHint(GLFW_SAMPLES, 8);    // multisampling!
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(
            640, 480, "context", NULL, NULL);

    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Make sure that we made a 3.3+ context.
    glfwMakeContextCurrent(window);
    int major, minor;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);

    if (major < 3 || (major == 3 && minor < 3))
    {
        glfwTerminate();
        return -1;
    }
    printf("OpenGL initialized!\n");

    GLuint  tex = shader_compile_vert(shader_dir, "texture.vert");
    GLuint eval = shader_compile_frag(shader_dir, "eval.frag");
    GLuint blit = shader_compile_frag(shader_dir, "blit.frag");

    eval_program = shader_make_program(tex, eval);
    blit_program = shader_make_program(tex, blit);
    return 0;
}

void gl_tex_defaults(GLenum tex_enum)
{
    glTexParameterf(tex_enum, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(tex_enum, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(tex_enum, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(tex_enum, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void render_eval(const RenderCommand* const command,
                 const RenderTape* const tape)
{
    const GLuint program = eval_program;
    glUseProgram(program);

    // Bind tape texture to 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, tape->instructions);
    glUniform1i(glGetUniformLocation(program, "tape"), 0);

    // Bind atlas texture to 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, command->atlas);
    glUniform1i(glGetUniformLocation(program, "atlas"), 1);

    // Bind xyz data to 2
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_1D, command->xyz);
    glUniform1i(glGetUniformLocation(program, "xyz"), 2);

    // Load relevant uniforms
    glUniform1i(glGetUniformLocation(program, "block_size"),
                command->block_size);
    glUniform1i(glGetUniformLocation(program, "block_count"),
                command->block_count);

    // Target the swap texture with the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, command->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, command->swap, 0);

    // Prepare the viewport
    glViewport(0, 0, command->block_size + 1, tape->node_count + 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Load triangles that draw a flat rectangle from -1, -1, to 1, 1
    glBindBuffer(GL_ARRAY_BUFFER, command->rect);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), 0);

    // Draw the full rectangle into the FBO
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void render_blit(const RenderCommand* const command,
                 const RenderTape* const tape,
                 const GLint start_slot)
{
    const GLuint program = blit_program;
    glUseProgram(program);

    // Bind tape texture to 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, command->swap);
    glUniform1i(glGetUniformLocation(program, "values"), 0);

    // Load relevant uniforms
    glUniform1i(glGetUniformLocation(program, "block_size"),
                command->block_size);
    glUniform1i(glGetUniformLocation(program, "block_count"),
                command->block_count);
    glUniform1i(glGetUniformLocation(program, "start_slot"),
                start_slot);
    glUniform1i(glGetUniformLocation(program, "slot_count"),
                tape->node_count);

    // Target the swap texture with the framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, command->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, command->atlas, 0);

    // Prepare the viewport (contains the whole texture atlas)
    glViewport(0, 0, command->block_size * command->block_count + 1,
               command->node_count + 1);
    if (start_slot == 0)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Load triangles that draw a flat rectangle from -1, -1, to 1, 1
    glBindBuffer(GL_ARRAY_BUFFER, command->rect);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), 0);

    // Draw the full rectangle into the FBO
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void print_swap(RenderCommand* command)
{
    printf("Swap: \n");
    unsigned count = command->node_max * command->block_size;
    float tex[count];
    glBindTexture(GL_TEXTURE_2D, command->swap);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, &tex);

    for (int i=0; i < count; ++i)
    {
        printf("%f ", tex[i]);
    }
    printf("\n");
}

static void print_tape(RenderTape* tape)
{
    printf("Tape: ");
    float tex[tape->node_count*3];
    glBindTexture(GL_TEXTURE_1D, tape->instructions);
    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGB, GL_FLOAT, &tex);
    for (int i=0; i < tape->node_count; ++i)
    {
        printf("%u ", ((uint32_t*)tex)[3*i]);
        printf("%f ", tex[3*i+1]);
        printf("%f ", tex[3*i+2]);
    }
    printf("\n");
}

static void print_xyz(RenderCommand* command)
{
    printf("XYZ: ");
    float tex[command->block_size*3];
    glBindTexture(GL_TEXTURE_1D, command->xyz);
    glGetTexImage(GL_TEXTURE_1D, 0, GL_RGB, GL_FLOAT, &tex);
    for (int i=0; i < command->block_size*3; ++i)
        printf("%f ", tex[i]);
    printf("\n");
}

static void print_atlas(RenderCommand* command)
{
    printf("Atlas:\n");
    float tex[command->atlas_rows * command->atlas_cols];
    glBindTexture(GL_TEXTURE_2D, command->atlas);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_FLOAT, &tex);
    int q=0;
    for (int i=0; i < command->atlas_rows; ++i) {
        for (int j=0; j < command->atlas_cols; ++j) {
            printf("%.2f ", tex[q++]);
        }
        printf("\n");
    }
}

void render_command(RenderCommand* command, float* xyz)
{
    glfwMakeContextCurrent(window);

    // Copy xyz data to the xyz texture.
    glBindTexture(GL_TEXTURE_1D, command->xyz);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB32F, command->block_size,
                 0, GL_RGB, GL_FLOAT, xyz);

    printf("XYZ: ");
    print_xyz(command);

    glBindVertexArray(command->vao);

    RenderTape* tape = command->tape;
    GLint current_slot = 0;
    while (tape)
    {
        print_tape(tape);
        render_eval(command, tape);
        print_swap(command);
        render_blit(command, tape, current_slot);
        print_atlas(command);
        current_slot += tape->node_count;
        tape = tape->next;
    }

    // Switch back to the default framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
