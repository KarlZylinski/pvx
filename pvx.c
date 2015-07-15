#include "gl3w.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lauxlib.h"

#define MAX_SHAPES 256

typedef unsigned Handle;

typedef struct Shape {
    GLuint geometry_handle;
    unsigned count;
} Shape;

typedef struct LoadedFile
{
    int loaded;
    char* data;
    size_t size;
} LoadedFile;

static HWND g_window_handle;
static HDC g_device_context;
static HGLRC g_rendering_context;
static GLuint g_shader;
static unsigned g_window_width;
static unsigned g_window_height;
static Shape g_shapes[MAX_SHAPES];
static unsigned g_free_shapes[MAX_SHAPES];

static LoadedFile load_file(const char* filename)
{
    LoadedFile lf = {0};
    FILE* fp = fopen(filename, "rb");

    if (!fp)
        return lf;

    fseek(fp, 0, SEEK_END);
    lf.size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    lf.data = (char*)malloc(lf.size + 1);

    if (!lf.data)
        return lf;

    fread(lf.data, 1, lf.size, fp);
    lf.data[lf.size] = 0;
    fclose(fp);
    lf.loaded = 1;
    return lf;
}

static GLuint compile_glsl(const char* shader_source, GLenum shader_type)
{
    GLuint result = glCreateShader(shader_type);
    glShaderSource(result, 1, &shader_source, NULL);
    glCompileShader(result);
    return result;
}

static GLuint load_shader(const char* vertex_source, const char* fragment_source)
{
    GLuint vertex_shader = compile_glsl(vertex_source, GL_VERTEX_SHADER);
    GLuint fragment_shader = compile_glsl(fragment_source, GL_FRAGMENT_SHADER);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}

static void init(const char* window_title, unsigned window_width, unsigned window_height)
{
    HINSTANCE h = GetModuleHandle(NULL);
    WNDCLASS wc = {0};    
    GLuint vao;
    int border_width = GetSystemMetrics(SM_CXFIXEDFRAME);
    int h_border_thickness = GetSystemMetrics(SM_CXSIZEFRAME) + border_width;
    int v_border_thickness = GetSystemMetrics(SM_CYSIZEFRAME) + border_width;
    int caption_thickness = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXPADDEDBORDER);
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
        32,                       //Colordepth of the framebuffer.
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        32,                       //Number of bits for the depthbuffer
        8,                        //Number of bits for the stencilbuffer
        0,                        //Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };

    memset(g_free_shapes, 1, sizeof(g_free_shapes));
    g_window_width = window_width;
    g_window_height = window_height;
    wc.hInstance = h;
    wc.lpfnWndProc = DefWindowProc;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    wc.lpszClassName = window_title;
    wc.style = CS_OWNDC;
    RegisterClass(&wc);
    g_window_handle = CreateWindow(window_title, window_title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, window_width + 2 * h_border_thickness,window_height + 2 * v_border_thickness + caption_thickness, 0, 0, h, 0);
    g_device_context = GetDC(g_window_handle);
    SetPixelFormat(g_device_context, ChoosePixelFormat(g_device_context, &pfd), &pfd);
    g_rendering_context = wglCreateContext(g_device_context);
    wglMakeCurrent(g_device_context,g_rendering_context);
    gl3wInit();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDisable(GL_DEPTH_TEST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    LoadedFile vertex_shader = load_file("vertex_shader.glsl");
    assert(vertex_shader.loaded);
    LoadedFile fragment_shader = load_file("fragment_shader.glsl");
    assert(fragment_shader.loaded);
    g_shader = load_shader(vertex_shader.data, fragment_shader.data);
}

static void deinit()
{
    DestroyWindow(g_window_handle);
}

static void process_events()
{
    MSG msg = {0};

    while(PeekMessage(&msg,0,0,0,PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void clear()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

static void flip()
{
    glUseProgram(g_shader);
    SwapBuffers(g_device_context);
}

static Handle get_free_shape_handle()
{
    for (unsigned i = 0; i < MAX_SHAPES; ++i)
    {
        if (g_free_shapes[i])
            return i;
    }

    return (Handle)-1;
}

static Handle add_shape(float* verts, int n)
{
    Handle handle = get_free_shape_handle();
    assert(handle != -1);
    GLuint geometry_handle;
    glGenBuffers(1, &geometry_handle);
    glBindBuffer(GL_ARRAY_BUFFER, geometry_handle);
    glBufferData(GL_ARRAY_BUFFER, n * sizeof(float), verts, GL_STATIC_DRAW);
    Shape shape = { geometry_handle, n / 2 };
    g_shapes[handle] = shape;
    return handle;
}

static void draw_shape(Handle handle)
{
    Shape* shape = g_shapes + handle;
    glBindBuffer(GL_ARRAY_BUFFER, shape->geometry_handle);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, shape->count);
}


//////
// Expose lua API.

static int is_window_open()
{
    return IsWindow(g_window_handle);
}

static int pvx_init(lua_State* L)
{
    const char* window_title = luaL_checkstring(L, 1);
    unsigned window_width = (unsigned)luaL_checknumber(L, 2);
    unsigned window_height = (unsigned)luaL_checknumber(L, 3);
    init(window_title, window_width, window_height);
    return 0;
}

static int pvx_deinit(lua_State* L)
{
    deinit();
    return 0;
}

static int pvx_process_events(lua_State* L)
{
    process_events();
    return 0;
}

static int pvx_is_window_open(lua_State* L)
{   
    lua_pushboolean(L, is_window_open());
    return 1;
}

static int pvx_add_shape(lua_State* L)
{
    static float verts[256];
    assert(lua_istable(L, 1));
    int n = luaL_getn(L, 1);

    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, -1, i);
        verts[i - 1] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }

    lua_pushnumber(L, add_shape(verts, n));
    return 1;
}

static int pvx_draw_shape(lua_State* L)
{
    unsigned handle = luaL_checkint(L, 1);
    draw_shape(handle);
    return 0;
}

static int pvx_clear(lua_State* L)
{
    clear();
    return 0;
}

static int pvx_flip(lua_State* L)
{
    flip();
    return 0;
}

int __declspec(dllexport) pvx_load(lua_State* L)
{
    lua_register(L, "pvx_init", pvx_init);
    lua_register(L, "pvx_deinit", pvx_deinit);
    lua_register(L, "pvx_process_events", pvx_process_events);
    lua_register(L, "pvx_is_window_open", pvx_is_window_open);
    lua_register(L, "pvx_add_shape", pvx_add_shape);
    lua_register(L, "pvx_draw_shape", pvx_draw_shape);
    lua_register(L, "pvx_clear", pvx_clear);
    lua_register(L, "pvx_flip", pvx_flip);
    return 0;
}
