
#include "base_graphics.h"
#include <iostream>

using ::std::cout;
using ::std::endl;

namespace base {

#if defined(BASE_PLATFORM_WINDOWS)
#pragma comment(lib, "OpenGL32.lib")
#endif

PFNGLMULTITEXCOORD1FARBPROC	glMultiTexCoord1fARB = NULL;
PFNGLMULTITEXCOORD2FARBPROC	glMultiTexCoord2fARB = NULL;
PFNGLMULTITEXCOORD3FARBPROC	glMultiTexCoord3fARB = NULL;
PFNGLMULTITEXCOORD4FARBPROC	glMultiTexCoord4fARB = NULL;
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;
PFNGLCLIENTACTIVETEXTUREARBPROC	glClientActiveTextureARB = NULL;
int32 GL_AVAILABLE_TEXTURE_UNITS_ARB = 0;
bool extensions_initialized = false;

void InitializeGraphicsExtensions() {
#if defined(BASE_PLATFORM_WINDOWS)
    if (extensions_initialized) {
        return;
    }

    char* exttext = nullptr;
    extensions_initialized = true;
    bool multi_texture_supported = false;
    exttext = (char*)glGetString(GL_EXTENSIONS);

    if (strstr(exttext, "GL_ARB_multitexture")) {
        multi_texture_supported = true;
    }

    glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &GL_AVAILABLE_TEXTURE_UNITS_ARB);
    
    if (!multi_texture_supported || GL_AVAILABLE_TEXTURE_UNITS_ARB < 2) {
        cout << "Warning: not enough texture units available. Required=" << 2 << ", available=" << GL_AVAILABLE_TEXTURE_UNITS_ARB << "." << endl;
        // fixme: handle this case more gracefully to avoid a crash when calling unbound extensions.
        return;
    }

    glMultiTexCoord1fARB = (PFNGLMULTITEXCOORD1FARBPROC)wglGetProcAddress("glMultiTexCoord1fARB");
    glMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)wglGetProcAddress("glMultiTexCoord2fARB");
    glMultiTexCoord3fARB = (PFNGLMULTITEXCOORD3FARBPROC)wglGetProcAddress("glMultiTexCoord3fARB");
    glMultiTexCoord4fARB = (PFNGLMULTITEXCOORD4FARBPROC)wglGetProcAddress("glMultiTexCoord4fARB");
    glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress("glActiveTextureARB");
    glClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)wglGetProcAddress("glClientActiveTextureARB");
#endif
}

GraphicsWindow::GraphicsWindow(const ::std::string& title, uint32 x, uint32 y, uint32 width,
                               uint32 height, uint32 render_bpp, uint32 depth_stencil_bpp,
                               uint32 style_flags) {
  Create(title, x, y, width, height, style_flags);
  CreateGraphics(render_bpp, depth_stencil_bpp);
}

GraphicsWindow::~GraphicsWindow() { DestroyGraphics(); }

void GraphicsWindow::CreateGraphics(uint32 render_bpp, uint32 depth_stencil_bpp) {
#if defined(BASE_PLATFORM_WINDOWS)
  PIXELFORMATDESCRIPTOR pfd;
  device_context_handle_ = (HDC)GetDC(window_handle_);

  memset(&pfd, 0, sizeof(pfd));

  pfd.nSize = sizeof(pfd);
  pfd.nVersion = 1;
  pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SWAP_EXCHANGE;
  pfd.iPixelType = PFD_TYPE_RGBA;
  pfd.cColorBits = render_bpp;
  pfd.cDepthBits = depth_stencil_bpp;
  pfd.iLayerType = PFD_MAIN_PLANE;

  SetPixelFormat(device_context_handle_, ChoosePixelFormat(device_context_handle_, &pfd), &pfd);

  graphics_handle_ = wglCreateContext(device_context_handle_);
  wglMakeCurrent(device_context_handle_, graphics_handle_);
#endif

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glViewport(0, 0, width_, height_);
  glPointSize(45.0);

  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
  glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

  InitializeGraphicsExtensions();
}

void GraphicsWindow::DestroyGraphics() {
#if defined(BASE_PLATFORM_WINDOWS)
  wglMakeCurrent(NULL, NULL);
  wglDeleteContext(graphics_handle_);
  ReleaseDC(window_handle_, device_context_handle_);
#endif
}

void GraphicsWindow::BeginScene() {
#if defined(BASE_PLATFORM_WINDOWS)
  wglMakeCurrent(device_context_handle_, graphics_handle_);
#endif

  glEnable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glDepthFunc(GL_LEQUAL);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(TRUE);

#if defined(BASE_PLATFORM_WINDOWS) || defined(BASE_PLATFORM_MACOS)
  glEnable(GL_TEXTURE_2D);
  glDisable(GL_LIGHTING);
  glClearDepth(1.0f);
#endif
}

void GraphicsWindow::EndScene() {
#if defined(BASE_PLATFORM_WINDOWS)
  SwapBuffers(device_context_handle_);
#elif defined(BASE_PLATFORM_MACOS)
  [[m_hView openGLContext] flushBuffer];
#elif defined(BASE_PLATFORM_IOS)
  // On iOS, a render buffer must be bound, which will be presented by
  // flushBuffer. This behavior differs from OSX and Win, which automatically
  // render the attached color buffer of the presently bound framebuffer.
  [m_hView flushBuffer];
#endif
}

void GraphicsWindow::Resolve() {
#if defined(BASE_PLATFORM_WINDOWS)
  wglMakeCurrent(device_context_handle_, graphics_handle_);
#endif

  glFlush();
}

}  // namespace base
