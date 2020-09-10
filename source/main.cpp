/*
//
// Copyright (c) 1998-2012 Joe Bertolami. All Right Reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//   AND ANY EXPRESS OR IMPLIED WARRANTIES, CLUDG, BUT NOT LIMITED TO, THE
//   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//   ARE DISCLAIMED.  NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
//   LIABLE FOR ANY DIRECT, DIRECT, CIDENTAL, SPECIAL, EXEMPLARY, OR
//   CONSEQUENTIAL DAMAGES (CLUDG, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
//   GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSESS TERRUPTION)
//   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER  CONTRACT, STRICT
//   LIABILITY, OR TORT (CLUDG NEGLIGENCE OR OTHERWISE) ARISG  ANY WAY  OF THE
//   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Additional Information:
//
//   For more information, visit http://www.bertolami.com.
//
*/

#include <chrono>
#include <iostream>
#include <memory>
#include <vector>

#include "assets.h"
#include "jmath/matrix4.h"
#include "stdio.h"
#include "window/base_graphics.h"
#include "window/base_window.h"

#define WINDOW_WIDTH (1024.0)
#define WINDOW_HEIGHT (768.0)

using namespace base;
using ::std::cout;
using ::std::endl;
using ::std::make_unique;
using ::std::unique_ptr;
using ::std::vector;

float32 GetFrameTimeElapsed() {
  static ::std::chrono::steady_clock::time_point last_time =
      ::std::chrono::high_resolution_clock::now();
  ::std::chrono::steady_clock::time_point current_time =
      ::std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> time_delta = current_time - last_time;
  last_time = current_time;
  return time_delta.count();
}

void RenderFrame(const World& world, bool textures_enabled,
                 bool lighting_enabled, bool gi_enabled, bool animate) {
  static float32 angle_2 = 0.0;
  static const float32 omega_2 = 0.0001f;
  static ::base::matrix4 model_view;
  static ::base::matrix4 projection;

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (angle_2 >= 2 * BASE_PI) {
    angle_2 -= 2 * BASE_PI;
  }

  if (animate) {
    angle_2 += omega_2;
  }

  ::base::vector3 eye(70 * sin(angle_2), 20 * sin(angle_2) - 25.0f,
                      70 * cos(angle_2));
  model_view.look(eye, (eye - vector3(0, -25, 0)).normalize(),
                  vector3(0, 1, 0));
  projection.perspective(BASE_PI / 2.0f, WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f,
                         1000.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(model_view.m);
  glMatrixMode(GL_PROJECTION);
  glLoadMatrixf(projection.m);

  world.Draw(textures_enabled, lighting_enabled, gi_enabled);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    cout << "Usage: x.exe <world filename>" << endl;
    return 0;
  }

  /* Create a queue for input events. */
  vector<InputEvent> window_events;
  auto window = make_unique<GraphicsWindow>("Project X: Demo Zero", 100, 10,
                                            WINDOW_WIDTH, WINDOW_HEIGHT, 32, 0);

  /* Load our map and initialize our lightmaps. */
  World demo_world(argv[1]);

  if (!demo_world.IsValid()) {
    cout << "Failed to load world file " << argv[1] << "." << endl;
    return 0;
  }

  glEnable(GL_POLYGON_SMOOTH);
  glShadeModel(GL_SMOOTH);
  glClearDepth(1.0f);
  glDepthFunc(GL_LEQUAL);
  glClearColor(0, 0, 0, 1);

  bool textures_enabled = true;
  bool lighting_enabled = true;
  bool gi_enabled = true;
  bool animate = true;

  while (window && window->IsValid()) {
    window->Update(&window_events);

    /* Handle all of our recent events that collected in our queue. */
    for (auto& event : window_events) {
      if (event.is_on) {
        switch (event.switch_index) {
          case 27: /* If the user pressed escape, exit the app. */
            return 0;
          case '1':
            textures_enabled = !textures_enabled;
            break;
          case '2':
            lighting_enabled = !lighting_enabled;
            break;
          case '3':
            gi_enabled = !gi_enabled;
            break;
          case ' ':
            animate = !animate;
            break;
        }
      }
    }

    /* Render our scene. */
    window->BeginScene();
    RenderFrame(demo_world, textures_enabled, lighting_enabled, gi_enabled,
                animate);
    window->EndScene();
  }

  return 0;
}