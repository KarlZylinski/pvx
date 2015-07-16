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
static Shape g_shapes[MAX_SHAPES];
static unsigned g_free_shapes[MAX_SHAPES];
static float g_projection_matrix[16];
static GLuint g_model_view_projection_matrix_location;
static const unsigned floats_per_vertex = 5;

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
    static char buffer[512];
    glGetShaderInfoLog(result, 512, NULL, buffer);
    printf("%s", buffer);
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

static void set_projection_matrix(unsigned window_width, unsigned window_height)
{
    memset(g_projection_matrix, 0, sizeof(g_projection_matrix));
    float near_plane = -1;
    float far_plane = 1;
    g_projection_matrix[0] = 2.0f / (window_width - 1.0f);
    g_projection_matrix[5] = -2.0f / (window_height - 1.0f);
    g_projection_matrix[10] = 2.0f / (far_plane / near_plane);
    g_projection_matrix[12] = -1;
    g_projection_matrix[13] = 1;
    g_projection_matrix[14] = (near_plane + far_plane) / (near_plane - far_plane);
    g_projection_matrix[15] = 1;
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_SIZE:
    {
        WORD width = LOWORD(lparam);
        WORD height = HIWORD(lparam);
        set_projection_matrix(width, height);
        glViewport(0, 0, width, height);
        return 0;
    }
    default:
        return DefWindowProc(hwnd, message, wparam, lparam);
    }
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

    set_projection_matrix(window_width, window_height);
    memset(g_free_shapes, 1, sizeof(g_free_shapes));
    wc.hInstance = h;
    wc.lpfnWndProc = window_proc;
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
    g_model_view_projection_matrix_location = glGetUniformLocation(g_shader, "model_view_projection_matrix");
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
    glBufferData(GL_ARRAY_BUFFER, n * floats_per_vertex * sizeof(float), verts, GL_STATIC_DRAW);
    Shape shape = { geometry_handle, n };
    g_shapes[handle] = shape;
    return handle;
}

static void mat_ident(float* out)
{
    memset(out, 0, 16 * sizeof(float));
    out[0] = 1;
    out[5] = 1;
    out[10] = 1;
    out[15] = 1;
}

static void mat_mul(const float* m1, const float* m2, float* out)
{
    out[0] = m1[0] * m2[0] + m1[1] * m2[4] + m1[2] * m2[8] + m1[3] * m2[12];
    out[1] = m1[0] * m2[1] + m1[1] * m2[5] + m1[2] * m2[9] + m1[3] * m2[13];
    out[2] = m1[0] * m2[2] + m1[1] * m2[6] + m1[2] * m2[10] + m1[3] * m2[14];
    out[3] = m1[0] * m2[3] + m1[1] * m2[7] + m1[2] * m2[11] + m1[3] * m2[15];

    out[4] = m1[4] * m2[0] + m1[5] * m2[4] + m1[6] * m2[8] + m1[7] * m2[12];
    out[5] = m1[4] * m2[1] + m1[5] * m2[5] + m1[6] * m2[9] + m1[7] * m2[13];
    out[6] = m1[4] * m2[2] + m1[5] * m2[6] + m1[6] * m2[10] + m1[7] * m2[14];
    out[7] = m1[4] * m2[3] + m1[5] * m2[7] + m1[6] * m2[11] + m1[7] * m2[15];

    out[8] = m1[8] * m2[0] + m1[9] * m2[4] + m1[10] * m2[8] + m1[11] * m2[12];
    out[9] = m1[8] * m2[1] + m1[9] * m2[5] + m1[10] * m2[9] + m1[11] * m2[13];
    out[10] = m1[8] * m2[2] + m1[9] * m2[6] + m1[10] * m2[10] + m1[11] * m2[14];
    out[11] = m1[8] * m2[3] + m1[9] * m2[7] + m1[10] * m2[11] + m1[11] * m2[15];

    out[12] = m1[12] * m2[0] + m1[13] * m2[4] + m1[14] * m2[8] + m1[15] * m2[12];
    out[13] = m1[12] * m2[1] + m1[13] * m2[5] + m1[14] * m2[9] + m1[15] * m2[13];
    out[14] = m1[12] * m2[2] + m1[13] * m2[6] + m1[14] * m2[10] + m1[15] * m2[14];
    out[15] = m1[12] * m2[3] + m1[13] * m2[7] + m1[14] * m2[11] + m1[15] * m2[15];
}

static void draw_shape(Handle handle, float x, float y)
{
    glUseProgram(g_shader);
    Shape* shape = g_shapes + handle;
    glBindBuffer(GL_ARRAY_BUFFER, shape->geometry_handle);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, floats_per_vertex * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, floats_per_vertex * sizeof(float), (void*)(2 * sizeof(float)));
    static float model_projection_matrix[16];
    static float model_matrix[16];
    mat_ident(model_matrix);
    model_matrix[12] = x;
    model_matrix[13] = y;
    mat_mul(model_matrix, g_projection_matrix, model_projection_matrix);
    glUniformMatrix4fv(g_model_view_projection_matrix_location, 1, GL_FALSE, model_projection_matrix);
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
    float r = (float)luaL_checknumber(L, 1);
    float g = (float)luaL_checknumber(L, 2);
    float b = (float)luaL_checknumber(L, 3);
    assert(lua_istable(L, 4));
    int n = luaL_getn(L, 4);

    int vertex_counter = 0;

    for (int i = 1; i <= n; ++i)
    {
        lua_rawgeti(L, 4, i);
        verts[vertex_counter++] = (float)lua_tonumber(L, -1);

        if (i % 2 == 0)
        {
            verts[vertex_counter++] = r;
            verts[vertex_counter++] = g;
            verts[vertex_counter++] = b;
        }

        lua_pop(L, 1);
    }

    lua_pushnumber(L, add_shape(verts, n / 2));
    return 1;
}

static int pvx_draw_shape(lua_State* L)
{
    unsigned handle = luaL_checkint(L, 1);
    float x = (float)luaL_checknumber(L, 2);
    float y = (float)luaL_checknumber(L, 3);
    draw_shape(handle, x, y);
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
