#include <mbgl/platform/default/headless_view.hpp>
#include <mbgl/platform/default/headless_display.hpp>
#include <mbgl/platform/log.hpp>

#include <cassert>

#include <EGL/egl.h>

namespace mbgl {
	//Android seems to be very specific here...,
	static const EGLint configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_STENCIL_SIZE, 0,
		EGL_NONE
	};

	gl::glProc HeadlessView::initializeExtension(const char* name) {
		Log::Info(Event::Android, "Initialize extension: %s", name);
		return eglGetProcAddress(name);
	}

	void HeadlessView::createContext() {
		Log::Info(Event::Android, "Create context");

		//Connect to display system
		glDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if(glDisplay == EGL_NO_DISPLAY){
			throw std::runtime_error("Could not connect to display");
		}

		//Initialize
		EGLint major, minor;
		if(!eglInitialize(glDisplay, &major, &minor)) {
			Log::Error(Event::Android, "Could not initialize egl");
		}

		EGLint numConfigs;
		if(!eglChooseConfig(glDisplay, configAttribs, &glConfig, 1, &numConfigs)) {
			throw std::runtime_error( "Could not choose config");
		}

		EGLint surfAttribs[] = {
			EGL_WIDTH, 512,
			EGL_HEIGHT, 512,
			EGL_LARGEST_PBUFFER, EGL_TRUE,
			EGL_NONE
		};

		glSurface = eglCreatePbufferSurface(glDisplay, glConfig, surfAttribs);
		if(glSurface == EGL_NO_SURFACE) {
			throw std::runtime_error("Could not create surface: " + std::to_string(eglGetError()));
		}

		eglBindAPI(EGL_OPENGL_API);

		GLint contextAttrs[] =
		{
			EGL_CONTEXT_CLIENT_VERSION, 2,
			EGL_NONE
		};
		glContext = eglCreateContext(glDisplay, glConfig, EGL_NO_CONTEXT, contextAttrs);

		if (glContext == EGL_NO_CONTEXT) {
			throw std::runtime_error("Could not create context");
		}
	}

	void HeadlessView::destroyContext() {
		Log::Info(Event::Android, "Destroy context");
		if (glContext != EGL_NO_CONTEXT) {
			eglDestroyContext(glDisplay, glContext);
			glContext = EGL_NO_CONTEXT;
		}

		//Destroy surface?
		/*
		   if (glSurface != EGL_NO_SURFACE) {
		   eglDestroySurface(glDisplay, glSurface);
		   glSurface = EGL_NO_SURFACE;
		   }
		   */
		//Terminate display?
		/*
		   if(glDisplay != EGL_NO_DISPLAY) {
		   eglTerminate(glDisplay);
		   glDisplay = EGL_NO_DISPLAY;
		   }
		   */

		//Release thread?
		//eglReleaseThread();
	}

	void HeadlessView::resizeFramebuffer() {
		const unsigned int w = dimensions[0] * pixelRatio;
		const unsigned int h = dimensions[1] * pixelRatio;

		// Create depth/stencil buffer
		MBGL_CHECK_ERROR(glGenRenderbuffers(1, &fboDepthStencil));
		MBGL_CHECK_ERROR(glBindRenderbuffer(GL_RENDERBUFFER, fboDepthStencil));
		MBGL_CHECK_ERROR(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, w, h));
		MBGL_CHECK_ERROR(glBindRenderbuffer(GL_RENDERBUFFER, 0));

		MBGL_CHECK_ERROR(glGenRenderbuffers(1, &fboColor));
		MBGL_CHECK_ERROR(glBindRenderbuffer(GL_RENDERBUFFER, fboColor));
		MBGL_CHECK_ERROR(glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_OES, w, h));
		MBGL_CHECK_ERROR(glBindRenderbuffer(GL_RENDERBUFFER, 0));

		MBGL_CHECK_ERROR(glGenFramebuffers(1, &fbo));
		MBGL_CHECK_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

		MBGL_CHECK_ERROR(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fboColor));
		MBGL_CHECK_ERROR(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboDepthStencil));
		MBGL_CHECK_ERROR(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fboDepthStencil));

		GLenum status = MBGL_CHECK_ERROR(glCheckFramebufferStatus(GL_FRAMEBUFFER));

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			std::string error("Couldn't create framebuffer: ");
			switch (status) {
				case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: (error += "incomplete attachment"); break;
				case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: error += "incomplete missing attachment"; break;
				case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: error += "incomplete dimensions"; break;
				case GL_FRAMEBUFFER_UNSUPPORTED: error += "unsupported"; break;
				default: error += "other"; break;
			}
			throw std::runtime_error(error);
		}

		MBGL_CHECK_ERROR(glViewport(0, 0, w, h));
	}

	/*
	   void HeadlessView::resizeFramebuffer() {
	   Log::Info(Event::Android, "Resize FB");

//Make sure we're clean
if(glSurface != EGL_NO_SURFACE) {
eglDestroySurface(glDisplay, glSurface);
}

const int32_t w = dimensions[0] * pixelRatio;
const int32_t h = dimensions[1] * pixelRatio;

const EGLint pbufferAttribs[] = {
EGL_WIDTH, w,
EGL_HEIGHT, h,
EGL_NONE,
};

Log::Info(Event::Android, "Resizing pbuffer to %dx%d", w, h);

glSurface = eglCreatePbufferSurface(glDisplay, glConfig, pbufferAttribs);

this->activateContext();
}
*/

void HeadlessView::clearBuffers() {
	Log::Info(Event::Android, "Clear buffers");
	MBGL_CHECK_ERROR(glBindFramebuffer(GL_FRAMEBUFFER, 0));

	if (fbo) {
		MBGL_CHECK_ERROR(glDeleteFramebuffers(1, &fbo));
		fbo = 0;
	}

	if (fboColor) {
		MBGL_CHECK_ERROR(glDeleteRenderbuffers(1, &fboColor));
		fboColor = 0;
	}

	if (fboDepthStencil) {
		MBGL_CHECK_ERROR(glDeleteRenderbuffers(1, &fboDepthStencil));
		fboDepthStencil = 0;
	}
}

void HeadlessView::activateContext() {
	Log::Info(Event::Android, "Activate context");
	if(eglMakeCurrent(glDisplay, glSurface, glSurface, glContext) != EGL_TRUE) {
		Log::Info(Event::Android, "Error activating context");
		throw std::runtime_error("Error activating context");
	}
}

void HeadlessView::deactivateContext() {
	Log::Info(Event::Android, "Deactivate context");
	if(eglMakeCurrent(glDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) != EGL_TRUE) {
		Log::Info(Event::Android, "Error deactivating context");
		throw std::runtime_error("Error deactivating context");
	}
}

} // namespace mbgl
