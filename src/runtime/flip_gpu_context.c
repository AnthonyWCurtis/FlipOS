#define _GNU_SOURCE

#include "flip_gpu_context.h"

#include "flip_video_layout.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef void *EGLDisplay;
typedef void *EGLContext;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef char GLchar;

struct gbm_device;

#define EGL_NONE 0x3038
#define EGL_VERSION 0x3054
#define EGL_VENDOR 0x3053
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_OPENGL_ES2_BIT 0x0004
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT 0x0001
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_PLATFORM_GBM_KHR 0x31D7

#define GL_FALSE 0
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_NEAREST 0x2600
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FLOAT 0x1406
#define GL_TRIANGLE_STRIP 0x0005
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TEXTURE0 0x84C0

typedef struct gbm_device *(*PFN_gbm_create_device)(int);
typedef void (*PFN_gbm_device_destroy)(struct gbm_device *);

typedef void *(*PFN_eglGetProcAddress)(const char *);
typedef EGLDisplay (*PFNEGLGETPLATFORMDISPLAYEXTPROC)(
    EGLenum platform,
    void *native_display,
    const EGLint *attributes);
typedef EGLBoolean (*PFN_eglInitialize)(
    EGLDisplay display,
    EGLint *major,
    EGLint *minor);
typedef const char *(*PFN_eglQueryString)(
    EGLDisplay display,
    EGLint name);
typedef EGLBoolean (*PFN_eglBindAPI)(EGLenum api);
typedef EGLBoolean (*PFN_eglChooseConfig)(
    EGLDisplay display,
    const EGLint *attributes,
    EGLConfig *configs,
    EGLint config_size,
    EGLint *config_count);
typedef EGLContext (*PFN_eglCreateContext)(
    EGLDisplay display,
    EGLConfig config,
    EGLContext share_context,
    const EGLint *attributes);
typedef EGLSurface (*PFN_eglCreatePbufferSurface)(
    EGLDisplay display,
    EGLConfig config,
    const EGLint *attributes);
typedef EGLBoolean (*PFN_eglMakeCurrent)(
    EGLDisplay display,
    EGLSurface draw,
    EGLSurface read,
    EGLContext context);
typedef EGLBoolean (*PFN_eglDestroySurface)(
    EGLDisplay display,
    EGLSurface surface);
typedef EGLBoolean (*PFN_eglDestroyContext)(
    EGLDisplay display,
    EGLContext context);
typedef EGLBoolean (*PFN_eglTerminate)(EGLDisplay display);
typedef EGLint (*PFN_eglGetError)(void);

typedef const GLubyte *(*PFN_glGetString)(GLenum name);
typedef void (*PFN_glGenTextures)(GLsizei count, GLuint *textures);
typedef void (*PFN_glDeleteTextures)(GLsizei count, const GLuint *textures);
typedef void (*PFN_glBindTexture)(GLenum target, GLuint texture);
typedef void (*PFN_glTexParameteri)(GLenum target, GLenum name, GLint value);
typedef void (*PFN_glPixelStorei)(GLenum name, GLint value);
typedef void (*PFN_glTexImage2D)(
    GLenum target,
    GLint level,
    GLint internal_format,
    GLsizei width,
    GLsizei height,
    GLint border,
    GLenum format,
    GLenum type,
    const void *pixels);
typedef void (*PFN_glTexSubImage2D)(
    GLenum target,
    GLint level,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    const void *pixels);
typedef GLuint (*PFN_glCreateShader)(GLenum type);
typedef void (*PFN_glShaderSource)(
    GLuint shader,
    GLsizei count,
    const GLchar *const *strings,
    const GLint *lengths);
typedef void (*PFN_glCompileShader)(GLuint shader);
typedef void (*PFN_glGetShaderiv)(
    GLuint shader,
    GLenum name,
    GLint *value);
typedef void (*PFN_glGetShaderInfoLog)(
    GLuint shader,
    GLsizei buffer_size,
    GLsizei *length,
    GLchar *log);
typedef void (*PFN_glDeleteShader)(GLuint shader);
typedef GLuint (*PFN_glCreateProgram)(void);
typedef void (*PFN_glAttachShader)(GLuint program, GLuint shader);
typedef void (*PFN_glBindAttribLocation)(
    GLuint program,
    GLuint index,
    const GLchar *name);
typedef void (*PFN_glLinkProgram)(GLuint program);
typedef void (*PFN_glGetProgramiv)(
    GLuint program,
    GLenum name,
    GLint *value);
typedef void (*PFN_glGetProgramInfoLog)(
    GLuint program,
    GLsizei buffer_size,
    GLsizei *length,
    GLchar *log);
typedef void (*PFN_glDeleteProgram)(GLuint program);
typedef void (*PFN_glUseProgram)(GLuint program);
typedef GLint (*PFN_glGetUniformLocation)(
    GLuint program,
    const GLchar *name);
typedef void (*PFN_glUniform1i)(GLint location, GLint value);
typedef void (*PFN_glActiveTexture)(GLenum texture);
typedef void (*PFN_glViewport)(
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height);
typedef void (*PFN_glClearColor)(
    GLfloat red,
    GLfloat green,
    GLfloat blue,
    GLfloat alpha);
typedef void (*PFN_glClear)(GLenum mask);
typedef void (*PFN_glVertexAttribPointer)(
    GLuint index,
    GLint size,
    GLenum type,
    GLboolean normalized,
    GLsizei stride,
    const void *pointer);
typedef void (*PFN_glEnableVertexAttribArray)(GLuint index);
typedef void (*PFN_glDrawArrays)(
    GLenum mode,
    GLint first,
    GLsizei count);
typedef void (*PFN_glFinish)(void);

struct flip_gpu_context {
    void *mali;
    int drm_fd;
    struct gbm_device *gbm;
    EGLDisplay display;
    EGLContext context;
    EGLSurface surface;

    unsigned display_width;
    unsigned display_height;

    PFN_gbm_device_destroy gbm_device_destroy;
    PFN_eglDestroySurface eglDestroySurface;
    PFN_eglDestroyContext eglDestroyContext;
    PFN_eglTerminate eglTerminate;

    PFN_glDeleteTextures glDeleteTextures;
    PFN_glBindTexture glBindTexture;
    PFN_glPixelStorei glPixelStorei;
    PFN_glTexImage2D glTexImage2D;
    PFN_glTexSubImage2D glTexSubImage2D;
    PFN_glDeleteProgram glDeleteProgram;
    PFN_glUseProgram glUseProgram;
    PFN_glUniform1i glUniform1i;
    PFN_glActiveTexture glActiveTexture;
    PFN_glViewport glViewport;
    PFN_glClearColor glClearColor;
    PFN_glClear glClear;
    PFN_glVertexAttribPointer glVertexAttribPointer;
    PFN_glEnableVertexAttribArray glEnableVertexAttribArray;
    PFN_glDrawArrays glDrawArrays;
    PFN_glFinish glFinish;

    GLuint texture;
    GLuint program;
    GLint sampler_location;
    GLint swap_rb_location;

    unsigned texture_width;
    unsigned texture_height;
    flip_gpu_format_t texture_format;

    unsigned char *packed;
    size_t packed_capacity;

    flip_gpu_context_info_t info;
};

static void *load_symbol(void *library, const char *name)
{
    void *result = dlsym(library, name);
    if (!result)
        fprintf(stderr, "FLIP_GPU: missing symbol %s: %s\n", name, dlerror());
    return result;
}

static int ensure_packed(
    flip_gpu_context_t *context,
    size_t bytes)
{
    if (bytes <= context->packed_capacity)
        return 0;

    unsigned char *next = realloc(context->packed, bytes);
    if (!next)
        return -1;

    context->packed = next;
    context->packed_capacity = bytes;
    return 0;
}

static GLuint compile_shader(
    PFN_glCreateShader glCreateShader,
    PFN_glShaderSource glShaderSource,
    PFN_glCompileShader glCompileShader,
    PFN_glGetShaderiv glGetShaderiv,
    PFN_glGetShaderInfoLog glGetShaderInfoLog,
    PFN_glDeleteShader glDeleteShader,
    GLenum type,
    const char *source)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;

    const GLchar *sources[] = { source };
    glShaderSource(shader, 1, sources, NULL);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        GLsizei length = 0;
        memset(log, 0, sizeof(log));
        glGetShaderInfoLog(
            shader,
            (GLsizei)sizeof(log) - 1,
            &length,
            log);
        fprintf(stderr, "FLIP_GPU: shader compile failed: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint create_program(
    flip_gpu_context_t *context,
    PFN_glCreateShader glCreateShader,
    PFN_glShaderSource glShaderSource,
    PFN_glCompileShader glCompileShader,
    PFN_glGetShaderiv glGetShaderiv,
    PFN_glGetShaderInfoLog glGetShaderInfoLog,
    PFN_glDeleteShader glDeleteShader,
    PFN_glCreateProgram glCreateProgram,
    PFN_glAttachShader glAttachShader,
    PFN_glBindAttribLocation glBindAttribLocation,
    PFN_glLinkProgram glLinkProgram,
    PFN_glGetProgramiv glGetProgramiv,
    PFN_glGetProgramInfoLog glGetProgramInfoLog,
    PFN_glDeleteProgram glDeleteProgram)
{
    static const char vertex_source[] =
        "attribute vec2 a_position;\n"
        "attribute vec2 a_texcoord;\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "  gl_Position = vec4(a_position, 0.0, 1.0);\n"
        "  v_texcoord = a_texcoord;\n"
        "}\n";

    static const char fragment_source[] =
        "precision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "uniform int u_swap_rb;\n"
        "varying vec2 v_texcoord;\n"
        "void main() {\n"
        "  vec4 c = texture2D(u_texture, v_texcoord);\n"
        "  if (u_swap_rb != 0) c = vec4(c.b, c.g, c.r, 1.0);\n"
        "  gl_FragColor = c;\n"
        "}\n";

    GLuint vertex = compile_shader(
        glCreateShader,
        glShaderSource,
        glCompileShader,
        glGetShaderiv,
        glGetShaderInfoLog,
        glDeleteShader,
        GL_VERTEX_SHADER,
        vertex_source);
    if (!vertex)
        return 0;

    GLuint fragment = compile_shader(
        glCreateShader,
        glShaderSource,
        glCompileShader,
        glGetShaderiv,
        glGetShaderInfoLog,
        glDeleteShader,
        GL_FRAGMENT_SHADER,
        fragment_source);
    if (!fragment) {
        glDeleteShader(vertex);
        return 0;
    }

    GLuint program = glCreateProgram();
    if (!program) {
        glDeleteShader(fragment);
        glDeleteShader(vertex);
        return 0;
    }

    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_texcoord");
    glLinkProgram(program);

    GLint ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        GLsizei length = 0;
        memset(log, 0, sizeof(log));
        glGetProgramInfoLog(
            program,
            (GLsizei)sizeof(log) - 1,
            &length,
            log);
        fprintf(stderr, "FLIP_GPU: program link failed: %s\n", log);
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(fragment);
    glDeleteShader(vertex);

    (void)context;
    return program;
}

flip_gpu_context_t *flip_gpu_context_create(
    unsigned display_width,
    unsigned display_height)
{
    if (display_width == 0 || display_height == 0)
        return NULL;

    flip_gpu_context_t *context = calloc(1, sizeof(*context));
    if (!context)
        return NULL;

    context->drm_fd = -1;
    context->display_width = display_width;
    context->display_height = display_height;

    context->mali = dlopen(
        "/usr/lib/libmali.so.1",
        RTLD_NOW | RTLD_LOCAL);
    if (!context->mali) {
        fprintf(stderr, "FLIP_GPU: libmali load failed: %s\n", dlerror());
        goto fail;
    }

    PFN_gbm_create_device gbm_create_device =
        (PFN_gbm_create_device)load_symbol(
            context->mali,
            "gbm_create_device");
    context->gbm_device_destroy =
        (PFN_gbm_device_destroy)load_symbol(
            context->mali,
            "gbm_device_destroy");

    PFN_eglGetProcAddress eglGetProcAddress =
        (PFN_eglGetProcAddress)load_symbol(
            context->mali,
            "eglGetProcAddress");
    PFN_eglInitialize eglInitialize =
        (PFN_eglInitialize)load_symbol(
            context->mali,
            "eglInitialize");
    PFN_eglQueryString eglQueryString =
        (PFN_eglQueryString)load_symbol(
            context->mali,
            "eglQueryString");
    PFN_eglBindAPI eglBindAPI =
        (PFN_eglBindAPI)load_symbol(
            context->mali,
            "eglBindAPI");
    PFN_eglChooseConfig eglChooseConfig =
        (PFN_eglChooseConfig)load_symbol(
            context->mali,
            "eglChooseConfig");
    PFN_eglCreateContext eglCreateContext =
        (PFN_eglCreateContext)load_symbol(
            context->mali,
            "eglCreateContext");
    PFN_eglCreatePbufferSurface eglCreatePbufferSurface =
        (PFN_eglCreatePbufferSurface)load_symbol(
            context->mali,
            "eglCreatePbufferSurface");
    PFN_eglMakeCurrent eglMakeCurrent =
        (PFN_eglMakeCurrent)load_symbol(
            context->mali,
            "eglMakeCurrent");

    context->eglDestroySurface =
        (PFN_eglDestroySurface)load_symbol(
            context->mali,
            "eglDestroySurface");
    context->eglDestroyContext =
        (PFN_eglDestroyContext)load_symbol(
            context->mali,
            "eglDestroyContext");
    context->eglTerminate =
        (PFN_eglTerminate)load_symbol(
            context->mali,
            "eglTerminate");

    PFN_eglGetError eglGetError =
        (PFN_eglGetError)load_symbol(
            context->mali,
            "eglGetError");

    if (!gbm_create_device ||
        !context->gbm_device_destroy ||
        !eglGetProcAddress ||
        !eglInitialize ||
        !eglQueryString ||
        !eglBindAPI ||
        !eglChooseConfig ||
        !eglCreateContext ||
        !eglCreatePbufferSurface ||
        !eglMakeCurrent ||
        !context->eglDestroySurface ||
        !context->eglDestroyContext ||
        !context->eglTerminate ||
        !eglGetError) {
        goto fail;
    }

    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        eglGetProcAddress("eglGetPlatformDisplayEXT");
    if (!eglGetPlatformDisplayEXT) {
        fprintf(stderr, "FLIP_GPU: eglGetPlatformDisplayEXT unavailable\n");
        goto fail;
    }

    context->drm_fd = open(
        "/dev/dri/card0",
        O_RDWR | O_CLOEXEC);
    if (context->drm_fd < 0) {
        perror("FLIP_GPU: open card0");
        goto fail;
    }

    context->gbm =
        gbm_create_device(context->drm_fd);
    if (!context->gbm) {
        fprintf(stderr, "FLIP_GPU: GBM device creation failed\n");
        goto fail;
    }

    context->display =
        eglGetPlatformDisplayEXT(
            EGL_PLATFORM_GBM_KHR,
            context->gbm,
            NULL);
    if (!context->display) {
        fprintf(
            stderr,
            "FLIP_GPU: EGL display creation failed err=0x%x\n",
            eglGetError());
        goto fail;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(
            context->display,
            &major,
            &minor)) {
        fprintf(
            stderr,
            "FLIP_GPU: EGL init failed err=0x%x\n",
            eglGetError());
        goto fail;
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        fprintf(
            stderr,
            "FLIP_GPU: EGL GLES bind failed err=0x%x\n",
            eglGetError());
        goto fail;
    }

    const EGLint config_attributes[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_NONE
    };

    EGLConfig config = NULL;
    EGLint config_count = 0;

    if (!eglChooseConfig(
            context->display,
            config_attributes,
            &config,
            1,
            &config_count) ||
        config_count < 1) {

        fprintf(
            stderr,
            "FLIP_GPU: EGL config failed err=0x%x\n",
            eglGetError());
        goto fail;
    }

    const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    context->context = eglCreateContext(
        context->display,
        config,
        NULL,
        context_attributes);
    if (!context->context) {
        fprintf(
            stderr,
            "FLIP_GPU: EGL context failed err=0x%x\n",
            eglGetError());
        goto fail;
    }

    const EGLint surface_attributes[] = {
        EGL_WIDTH, (EGLint)display_width,
        EGL_HEIGHT, (EGLint)display_height,
        EGL_NONE
    };

    context->surface = eglCreatePbufferSurface(
        context->display,
        config,
        surface_attributes);
    if (!context->surface) {
        fprintf(
            stderr,
            "FLIP_GPU: EGL pbuffer failed err=0x%x\n",
            eglGetError());
        goto fail;
    }

    if (!eglMakeCurrent(
            context->display,
            context->surface,
            context->surface,
            context->context)) {

        fprintf(
            stderr,
            "FLIP_GPU: EGL make-current failed err=0x%x\n",
            eglGetError());
        goto fail;
    }

#define LOAD_GL(type, variable, symbol) \
    type variable = (type)load_symbol(context->mali, symbol)

    LOAD_GL(PFN_glGetString, glGetString, "glGetString");
    LOAD_GL(PFN_glGenTextures, glGenTextures, "glGenTextures");
    context->glDeleteTextures =
        (PFN_glDeleteTextures)load_symbol(
            context->mali,
            "glDeleteTextures");
    context->glBindTexture =
        (PFN_glBindTexture)load_symbol(
            context->mali,
            "glBindTexture");
    LOAD_GL(PFN_glTexParameteri, glTexParameteri, "glTexParameteri");
    context->glPixelStorei =
        (PFN_glPixelStorei)load_symbol(
            context->mali,
            "glPixelStorei");
    context->glTexImage2D =
        (PFN_glTexImage2D)load_symbol(
            context->mali,
            "glTexImage2D");
    context->glTexSubImage2D =
        (PFN_glTexSubImage2D)load_symbol(
            context->mali,
            "glTexSubImage2D");

    LOAD_GL(PFN_glCreateShader, glCreateShader, "glCreateShader");
    LOAD_GL(PFN_glShaderSource, glShaderSource, "glShaderSource");
    LOAD_GL(PFN_glCompileShader, glCompileShader, "glCompileShader");
    LOAD_GL(PFN_glGetShaderiv, glGetShaderiv, "glGetShaderiv");
    LOAD_GL(PFN_glGetShaderInfoLog, glGetShaderInfoLog, "glGetShaderInfoLog");
    LOAD_GL(PFN_glDeleteShader, glDeleteShader, "glDeleteShader");
    LOAD_GL(PFN_glCreateProgram, glCreateProgram, "glCreateProgram");
    LOAD_GL(PFN_glAttachShader, glAttachShader, "glAttachShader");
    LOAD_GL(PFN_glBindAttribLocation, glBindAttribLocation, "glBindAttribLocation");
    LOAD_GL(PFN_glLinkProgram, glLinkProgram, "glLinkProgram");
    LOAD_GL(PFN_glGetProgramiv, glGetProgramiv, "glGetProgramiv");
    LOAD_GL(PFN_glGetProgramInfoLog, glGetProgramInfoLog, "glGetProgramInfoLog");

    context->glDeleteProgram =
        (PFN_glDeleteProgram)load_symbol(
            context->mali,
            "glDeleteProgram");
    context->glUseProgram =
        (PFN_glUseProgram)load_symbol(
            context->mali,
            "glUseProgram");

    LOAD_GL(
        PFN_glGetUniformLocation,
        glGetUniformLocation,
        "glGetUniformLocation");

    context->glUniform1i =
        (PFN_glUniform1i)load_symbol(
            context->mali,
            "glUniform1i");
    context->glActiveTexture =
        (PFN_glActiveTexture)load_symbol(
            context->mali,
            "glActiveTexture");
    context->glViewport =
        (PFN_glViewport)load_symbol(
            context->mali,
            "glViewport");
    context->glClearColor =
        (PFN_glClearColor)load_symbol(
            context->mali,
            "glClearColor");
    context->glClear =
        (PFN_glClear)load_symbol(
            context->mali,
            "glClear");
    context->glVertexAttribPointer =
        (PFN_glVertexAttribPointer)load_symbol(
            context->mali,
            "glVertexAttribPointer");
    context->glEnableVertexAttribArray =
        (PFN_glEnableVertexAttribArray)load_symbol(
            context->mali,
            "glEnableVertexAttribArray");
    context->glDrawArrays =
        (PFN_glDrawArrays)load_symbol(
            context->mali,
            "glDrawArrays");
    context->glFinish =
        (PFN_glFinish)load_symbol(
            context->mali,
            "glFinish");

#undef LOAD_GL

    if (!glGetString ||
        !glGenTextures ||
        !context->glDeleteTextures ||
        !context->glBindTexture ||
        !glTexParameteri ||
        !context->glPixelStorei ||
        !context->glTexImage2D ||
        !context->glTexSubImage2D ||
        !glCreateShader ||
        !glShaderSource ||
        !glCompileShader ||
        !glGetShaderiv ||
        !glGetShaderInfoLog ||
        !glDeleteShader ||
        !glCreateProgram ||
        !glAttachShader ||
        !glBindAttribLocation ||
        !glLinkProgram ||
        !glGetProgramiv ||
        !glGetProgramInfoLog ||
        !context->glDeleteProgram ||
        !context->glUseProgram ||
        !glGetUniformLocation ||
        !context->glUniform1i ||
        !context->glActiveTexture ||
        !context->glViewport ||
        !context->glClearColor ||
        !context->glClear ||
        !context->glVertexAttribPointer ||
        !context->glEnableVertexAttribArray ||
        !context->glDrawArrays ||
        !context->glFinish) {

        goto fail;
    }

    context->program = create_program(
        context,
        glCreateShader,
        glShaderSource,
        glCompileShader,
        glGetShaderiv,
        glGetShaderInfoLog,
        glDeleteShader,
        glCreateProgram,
        glAttachShader,
        glBindAttribLocation,
        glLinkProgram,
        glGetProgramiv,
        glGetProgramInfoLog,
        context->glDeleteProgram);
    if (!context->program)
        goto fail;

    context->sampler_location =
        glGetUniformLocation(
            context->program,
            "u_texture");
    context->swap_rb_location =
        glGetUniformLocation(
            context->program,
            "u_swap_rb");

    context->info.egl_major = major;
    context->info.egl_minor = minor;
    context->info.egl_vendor =
        eglQueryString(
            context->display,
            EGL_VENDOR);
    context->info.egl_version =
        eglQueryString(
            context->display,
            EGL_VERSION);
    context->info.gl_vendor =
        (const char *)glGetString(GL_VENDOR);
    context->info.gl_renderer =
        (const char *)glGetString(GL_RENDERER);
    context->info.gl_version =
        (const char *)glGetString(GL_VERSION);

    glGenTextures(1, &context->texture);
    context->glBindTexture(
        GL_TEXTURE_2D,
        context->texture);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE);
    context->glPixelStorei(
        GL_UNPACK_ALIGNMENT,
        1);

    fprintf(
        stderr,
        "FLIP_GPU: CONTEXT_OK egl=%d.%d "
        "egl_vendor=%s gl_vendor=%s renderer=%s gl=%s\n",
        major,
        minor,
        context->info.egl_vendor
            ? context->info.egl_vendor
            : "?",
        context->info.gl_vendor
            ? context->info.gl_vendor
            : "?",
        context->info.gl_renderer
            ? context->info.gl_renderer
            : "?",
        context->info.gl_version
            ? context->info.gl_version
            : "?");

    return context;

fail:
    flip_gpu_context_destroy(context);
    return NULL;
}

void flip_gpu_context_destroy(
    flip_gpu_context_t *context)
{
    if (!context)
        return;

    if (context->texture &&
        context->glDeleteTextures) {

        context->glDeleteTextures(
            1,
            &context->texture);
    }

    if (context->program &&
        context->glDeleteProgram) {

        context->glDeleteProgram(
            context->program);
    }

    if (context->display &&
        context->surface &&
        context->eglDestroySurface) {

        context->eglDestroySurface(
            context->display,
            context->surface);
    }

    if (context->display &&
        context->context &&
        context->eglDestroyContext) {

        context->eglDestroyContext(
            context->display,
            context->context);
    }

    if (context->display &&
        context->eglTerminate) {

        context->eglTerminate(
            context->display);
    }

    if (context->gbm &&
        context->gbm_device_destroy) {

        context->gbm_device_destroy(
            context->gbm);
    }

    if (context->drm_fd >= 0)
        close(context->drm_fd);

    if (context->mali)
        dlclose(context->mali);

    free(context->packed);
    free(context);
}

const flip_gpu_context_info_t *flip_gpu_context_info(
    const flip_gpu_context_t *context)
{
    return context
        ? &context->info
        : NULL;
}

int flip_gpu_context_render(
    flip_gpu_context_t *context,
    const void *data,
    unsigned width,
    unsigned height,
    size_t pitch,
    flip_gpu_format_t format)
{
    if (!context ||
        !data ||
        width == 0 ||
        height == 0) {

        return -1;
    }

    unsigned bpp = 0;
    GLenum gl_format = 0;
    GLenum gl_type = 0;
    int swap_rb = 0;

    switch (format) {
        case FLIP_GPU_FORMAT_RGB565:
            bpp = 2;
            gl_format = GL_RGB;
            gl_type =
                GL_UNSIGNED_SHORT_5_6_5;
            break;

        case FLIP_GPU_FORMAT_XRGB8888:
            bpp = 4;
            gl_format = GL_RGBA;
            gl_type =
                GL_UNSIGNED_BYTE;
            swap_rb = 1;
            break;

        default:
            return -1;
    }

    size_t packed_pitch =
        (size_t)width * bpp;

    const void *pixels = data;

    if (pitch != packed_pitch) {
        size_t bytes =
            packed_pitch * height;

        if (ensure_packed(
                context,
                bytes) < 0) {

            return -1;
        }

        const unsigned char *source =
            data;

        for (unsigned y = 0;
             y < height;
             ++y) {

            memcpy(
                context->packed +
                    y * packed_pitch,
                source +
                    y * pitch,
                packed_pitch);
        }

        pixels =
            context->packed;
    }

    flip_video_layout_t layout;

    if (flip_video_layout_compute(
            width,
            height,
            context->display_width,
            context->display_height,
            &layout) < 0) {

        return -1;
    }

    context->glActiveTexture(GL_TEXTURE0);
    context->glBindTexture(
        GL_TEXTURE_2D,
        context->texture);
    context->glPixelStorei(
        GL_UNPACK_ALIGNMENT,
        1);

    if (context->texture_width != width ||
        context->texture_height != height ||
        context->texture_format != format) {

        context->glTexImage2D(
            GL_TEXTURE_2D,
            0,
            (GLint)gl_format,
            (GLsizei)width,
            (GLsizei)height,
            0,
            gl_format,
            gl_type,
            pixels);

        context->texture_width =
            width;
        context->texture_height =
            height;
        context->texture_format =
            format;

    } else {

        context->glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            (GLsizei)width,
            (GLsizei)height,
            gl_format,
            gl_type,
            pixels);
    }

    context->glViewport(
        0,
        0,
        (GLsizei)context->display_width,
        (GLsizei)context->display_height);

    context->glClearColor(
        0.0f,
        0.0f,
        0.0f,
        1.0f);

    context->glClear(
        GL_COLOR_BUFFER_BIT);

    int viewport_y =
        (int)context->display_height -
        layout.y -
        (int)layout.output_height;

    context->glViewport(
        layout.x,
        viewport_y,
        (GLsizei)layout.output_width,
        (GLsizei)layout.output_height);

    static const GLfloat vertices[] = {
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f
    };

    context->glUseProgram(
        context->program);

    context->glUniform1i(
        context->sampler_location,
        0);

    context->glUniform1i(
        context->swap_rb_location,
        swap_rb);

    context->glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * (GLsizei)sizeof(GLfloat),
        vertices);

    context->glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        4 * (GLsizei)sizeof(GLfloat),
        vertices + 2);

    context->glEnableVertexAttribArray(0);
    context->glEnableVertexAttribArray(1);

    context->glDrawArrays(
        GL_TRIANGLE_STRIP,
        0,
        4);

    context->glFinish();

    return 0;
}
