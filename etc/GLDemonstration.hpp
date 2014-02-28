// Copyright (c) 2014 Robert Kooima
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#ifndef GLDEMONSTRATION_HPP
#define GLDEMONSTRATION_HPP

#include <SDL.h>

//------------------------------------------------------------------------------

namespace gl
{
    class demonstration
    {
    public:

        /// Initialize an SDL OpenGL window with the given title and size.

        demonstration(const char *title, int w, int h) :
            width(w),
            height(h),
            running(false),
            sun_rotation(-90, 0),
            drag_sun_rotation(false),
            drag_cam_rotation(false)
        {
            if (SDL_Init(SDL_INIT_VIDEO) == 0)
            {
                int x = SDL_WINDOWPOS_CENTERED;
                int y = SDL_WINDOWPOS_CENTERED;
                int f = SDL_WINDOW_OPENGL;

                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
                SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                    SDL_GL_CONTEXT_PROFILE_CORE);

                SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
                SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
                SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
                SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,  24);
                SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

                if ((window = SDL_CreateWindow(title, x, y, w, h, f)))
                {
                    context = SDL_GL_CreateContext(window);
                    running = true;
#ifdef GLEW_VERSION
                    glewExperimental = GL_TRUE;
                    glewInit();
#endif
                }
            }
        }

        /// Dispatch SDL events.

        void run()
        {
            SDL_Event e;

            while (running)
            {
                while (SDL_PollEvent(&e))
                    switch (e.type)
                    {
                        case SDL_MOUSEBUTTONDOWN: button(e.button.button, true);     break;
                        case SDL_MOUSEBUTTONUP:   button(e.button.button, false);    break;
                        case SDL_MOUSEMOTION:     motion(e.motion.x, e.motion.y);    break;
                        case SDL_KEYDOWN: if (e.key.repeat == 0) key(e.key.keysym.scancode, true);  break;
                        case SDL_KEYUP:   if (e.key.repeat == 0) key(e.key.keysym.scancode, false); break;
                        case SDL_QUIT:            running = false;                   break;
                    }

                step();
                draw();
                SDL_GL_SwapWindow(window);
            }
        }

        /// Finalize the SDL OpenGL window.

        virtual ~demonstration()
        {
            if (context) SDL_GL_DeleteContext(context);
            if (window)  SDL_DestroyWindow(window);
        }

    protected:

        /// Draw the scene.

        virtual void draw()
        {
        }

        /// Animate.

        virtual void step()
        {
            mat3 N = normal(inverse(view()));
            cam_position = cam_position + N * cam_velocity / 30.0;
        }

        /// Handle a mouse button press or release.

        virtual void button(int button, bool down)
        {
            switch (button)
            {
            case SDL_BUTTON_LEFT:
                drag_cam_rotation = down;
                prev_cam_rotation = cam_rotation;
                break;

            case SDL_BUTTON_RIGHT:
                drag_sun_rotation = down;
                prev_sun_rotation = sun_rotation;
                break;
            }
            prev_x = curr_x;
            prev_y = curr_y;
        }

        /// Handle mouse pointer motion.

        virtual void motion(int x, int y)
        {
            GLfloat dy = GLfloat(y - prev_y) / height;
            GLfloat dx = GLfloat(x - prev_x) / height;

            if (drag_cam_rotation)
            {
                cam_rotation[0] = prev_cam_rotation[0] +  90.0 * dy;
                cam_rotation[1] = prev_cam_rotation[1] + 180.0 * dx;

                if (cam_rotation[0] >   90) cam_rotation[0]  =  90;
                if (cam_rotation[0] <  -90) cam_rotation[0]  = -90;
                if (cam_rotation[1] >  180) cam_rotation[1] -= 360;
                if (cam_rotation[1] < -180) cam_rotation[1] += 360;
            }
            if (drag_sun_rotation)
            {
                sun_rotation[0] = prev_sun_rotation[0] +  90.0 * dy;
                sun_rotation[1] = prev_sun_rotation[1] + 180.0 * dx;

                if (sun_rotation[0] >   90) sun_rotation[0]  =  90;
                if (sun_rotation[0] <  -90) sun_rotation[0]  = -90;
                if (sun_rotation[1] >  180) sun_rotation[1] -= 360;
                if (sun_rotation[1] < -180) sun_rotation[1] += 360;
            }
            curr_x = x;
            curr_y = y;
        }

        /// Handle a key press or release.

        virtual void key(int key, bool down)
        {
            if (down)
                switch (key)
                {
                case SDL_SCANCODE_A:     cam_velocity[0] -= 1.0; break;
                case SDL_SCANCODE_D:     cam_velocity[0] += 1.0; break;
                case SDL_SCANCODE_C:     cam_velocity[1] -= 1.0; break;
                case SDL_SCANCODE_SPACE: cam_velocity[1] += 1.0; break;
                case SDL_SCANCODE_W:     cam_velocity[2] -= 1.0; break;
                case SDL_SCANCODE_S:     cam_velocity[2] += 1.0; break;
                }
            else
                switch (key)
                {
                case SDL_SCANCODE_A:     cam_velocity[0] += 1.0; break;
                case SDL_SCANCODE_D:     cam_velocity[0] -= 1.0; break;
                case SDL_SCANCODE_C:     cam_velocity[1] += 1.0; break;
                case SDL_SCANCODE_SPACE: cam_velocity[1] -= 1.0; break;
                case SDL_SCANCODE_W:     cam_velocity[2] += 1.0; break;
                case SDL_SCANCODE_S:     cam_velocity[2] -= 1.0; break;
                }
        }

        /// Return the current projection matrix.

        mat4 projection(GLfloat n=0.10f, GLfloat f=100.f) const
        {
            const GLfloat a = GLfloat(width) / GLfloat(height);
            return perspective(to_radians(60.f), a, n, f);
        }

        /// Return the current view matrix.

        mat4 view() const
        {
            return xrotation(to_radians(cam_rotation[0]))
                 * yrotation(to_radians(cam_rotation[1]))
                 * translation(-cam_position);
        }

        /// Return the current light vector.

        vec4 light() const
        {
            return xrotation(to_radians(sun_rotation[0]))
                 * yrotation(to_radians(sun_rotation[1])) * vec4(0, 0, 1, 0);
        }

    protected:

        int  width;
        int  height;
        bool running;

        vec3 cam_position;
        vec3 cam_velocity;
        vec2 cam_rotation;
        vec2 sun_rotation;

    private:

        bool drag_sun_rotation;
        bool drag_cam_rotation;
        vec2 prev_sun_rotation;
        vec2 prev_cam_rotation;
        int  prev_x;
        int  prev_y;
        int  curr_x;
        int  curr_y;

        SDL_Window   *window;
        SDL_GLContext context;
    };
}

//------------------------------------------------------------------------------

#endif
