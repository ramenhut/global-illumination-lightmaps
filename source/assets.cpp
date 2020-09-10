
#include "assets.h"

#include <iostream>
#include <thread>

#include "bitmap/bitmap.h"
#include "jmath/intersect.h"
#include "jmath/normal.h"
#include "jmath/plane.h"
#include "jmath/scalar.h"
#include "jmath/trace.h"
#include "window/base_graphics.h"

#define ENABLE_MULTITHREADING (1)
#define MIN_LIGHTMAP_SIZE (256)
#define MAX_LIGHTMAP_SIZE (256)
#define SAMPLE_COUNT (250)
#define RANDOM_NORMAL_COUNT (1000)

using ::base::int32;
using ::base::uint32;
using ::std::cout;
using ::std::endl;

const float32 lightmap_scale_factor = 1.0f;

inline void FindLightmapPlane(const vector4& plane, uint32* u_coeff,
                              uint32* v_coeff) {
  if (fabs(plane.x) >= fabs(plane.y) && fabs(plane.x) >= fabs(plane.z)) {
    // The plane is most closely aligned with the YZ plane.
    *u_coeff = 1;
    *v_coeff = 2;
  } else if (fabs(plane.y) >= fabs(plane.x) && fabs(plane.y) >= fabs(plane.z)) {
    // The plane is most closely aligned with the XZ plane.
    *u_coeff = 0;
    *v_coeff = 2;
  } else if (fabs(plane.z) >= fabs(plane.x) && fabs(plane.z) >= fabs(plane.y)) {
    // The plane is most closely aligned with the XY plane.
    *u_coeff = 0;
    *v_coeff = 1;
  }
}

/* Simple method to read one line from a file. */
void ReadOneLine(FILE* f, char* string) {
  do {
    fgets(string, 75, f);
  } while ((string[0] == '/') || (string[0] == '\n') || (string[0] == '#'));
}

Light::Light(const vector3& position, const vector4& color, float32 intensity) {
  position_ = position;
  color_ = color;
  intensity_ = intensity;
  enabled_ = false;
}

Texture::Texture(const ::std::string& filename) {
  texture_width_ = 0;
  texture_height_ = 0;
  gl_texture_index_ = 0;

  if (!::base::LoadBitmapImage(filename, &texture_map_, &texture_width_,
                               &texture_height_)) {
    cout << "Failed to load bitmap image " << filename << endl;
    return;
  }

  UploadTexture();

  cout << "Successfully loaded texture " << filename << "." << endl;
}

Texture::Texture(uint32 width, uint32 height) {
  texture_map_.resize(width * height * 3);
  texture_width_ = width;
  texture_height_ = height;
  gl_texture_index_ = 0;
}

void Texture::BlurTexture(int32 radius, int32 step) {
  // Perform horizontal blur.
  for (int32 j = 0; j < texture_height_; j++) {
    for (int32 i = 0; i < texture_width_; i++) {
      vector3 temp_val;
      uint32 accum_count = 0;
      uint32 texel_offset = (j * texture_width_ + i) * 3;
      for (int32 k = -(radius - 1); k < radius; k += step) {
        if (i + k < 0 || i + k >= texture_width_) {
          continue;
        }
        temp_val += vector3(texture_map_[texel_offset + k * 3],
                            texture_map_[texel_offset + k * 3 + 1],
                            texture_map_[texel_offset + k * 3 + 2]);
        accum_count++;
      }
      if (accum_count) {
        temp_val /= accum_count;
        texture_map_[texel_offset + 0] = temp_val.x;
        texture_map_[texel_offset + 1] = temp_val.y;
        texture_map_[texel_offset + 2] = temp_val.z;
      }
    }
  }

  // Perform vertical blur.
  for (int32 j = 0; j < texture_height_; j++) {
    for (int32 i = 0; i < texture_width_; i++) {
      vector3 temp_val;
      uint32 accum_count = 0;
      uint32 texel_offset = (j * texture_width_ + i) * 3;
      for (int32 k = -(radius - 1); k < radius; k += step) {
        if (j + k < 0 || j + k >= texture_height_) {
          continue;
        }
        temp_val +=
            vector3(texture_map_[texel_offset + k * texture_width_ * 3],
                    texture_map_[texel_offset + k * texture_width_ * 3 + 1],
                    texture_map_[texel_offset + k * texture_width_ * 3 + 2]);
        accum_count++;
      }
      if (accum_count) {
        temp_val /= accum_count;
        texture_map_[texel_offset + 0] = temp_val.x;
        texture_map_[texel_offset + 1] = temp_val.y;
        texture_map_[texel_offset + 2] = temp_val.z;
      }
    }
  }
}

void Texture::UploadTexture() {
  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &gl_texture_index_);
  glBindTexture(GL_TEXTURE_2D, gl_texture_index_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture_width_, texture_height_, 0,
               GL_RGB, GL_UNSIGNED_BYTE, texture_map_.data());
}

vector3 Texture::ReadTexel(const vector2& coord) const {
  uint32 x =
      ::base::clip_range(coord.x * texture_width_, 0.0, texture_width_ - 1);
  uint32 y =
      ::base::clip_range(coord.y * texture_height_, 0.0, texture_height_ - 1);
  uint32 offset = texture_width_ * 3 * y + x * 3;
  vector3 texel = vector3(texture_map_[offset + 0], texture_map_[offset + 1],
                          texture_map_[offset + 2]);
  return texel / 255.0;
}

void Texture::WriteTexel(const vector2& coord, const vector3& texel) {
  uint32 x =
      ::base::clip_range(coord.x * texture_width_, 0.0, texture_width_ - 1);
  uint32 y =
      ::base::clip_range(coord.y * texture_height_, 0.0, texture_height_ - 1);
  uint32 offset = texture_width_ * 3 * y + x * 3;
  texture_map_[offset + 0] = texel.x * 255.0;
  texture_map_[offset + 1] = texel.y * 255.0;
  texture_map_[offset + 2] = texel.z * 255.0;
}

Texture::~Texture() { glDeleteTextures(1, &gl_texture_index_); }

void Texture::Bind(int unit_index) {
  ::base::glActiveTextureARB(unit_index);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, gl_texture_index_);
}

Triangle::Triangle(const vector3& v0, const vector2& t0, const vector4& c0,
                   const vector3& v1, const vector2& t1, const vector4& c1,
                   const vector3& v2, const vector2& t2, const vector4& c2,
                   const ::std::shared_ptr<Texture>& diffuse) {
  vertices_[0].vert = v0;
  vertices_[0].tc = t0;
  vertices_[0].color = c0;
  vertices_[1].vert = v1;
  vertices_[1].tc = t1;
  vertices_[1].color = c1;
  vertices_[2].vert = v2;
  vertices_[2].tc = t2;
  vertices_[2].color = c2;

  diffuse_ = diffuse;
  requires_alpha_ = false;
  normal_ = ::base::calculate_normal(v0, v1, v2);
  plane_ = ::base::calculate_plane(normal_, v0);

  if (c0.a < 1.0 || c1.a < 1.0 || c2.a < 1.0) {
    requires_alpha_ = true;
  }
}

void Triangle::AttachLightmap(const ::std::shared_ptr<Texture>& lightmap) {
  lightmap_ = lightmap;
}

void Triangle::AttachGlobalLightmap(
    const ::std::shared_ptr<Texture>& lightmap) {
  gi_lightmap_ = lightmap;
}

vector3 Triangle::ReadLight(const vector3& point) const {
  // Map to barycentric coordinate within the triangle.
  // Using these barycentric coordinates, compute the texel and lumel
  // coordinates Sample from both textures and then multiply and return the
  // value.
  return vector3(0, 0, 1);
}

void Triangle::Draw(bool textures_enabled, bool lights_enabled,
                    bool global_illum_enabled) const {
  if (textures_enabled) {
    diffuse_->Bind(GL_TEXTURE0_ARB);

    if (global_illum_enabled) {
      gi_lightmap_->Bind(GL_TEXTURE1_ARB);
    } else if (lights_enabled) {
      lightmap_->Bind(GL_TEXTURE1_ARB);
    }
  } else {
    if (global_illum_enabled) {
      gi_lightmap_->Bind(GL_TEXTURE0_ARB);
    } else if (lights_enabled) {
      lightmap_->Bind(GL_TEXTURE0_ARB);
    }
  }

  if (requires_alpha_) {
    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_DST_ALPHA);
  }

  glBegin(GL_TRIANGLES);

  for (int i = 0; i < 3; i++) {
    if (textures_enabled) {
      ::base::glMultiTexCoord2fARB(GL_TEXTURE0_ARB, vertices_[i].tc.x,
                                   vertices_[i].tc.y);
      if (lights_enabled || global_illum_enabled) {
        ::base::glMultiTexCoord2fARB(GL_TEXTURE1_ARB, vertices_[i].lc.x,
                                     vertices_[i].lc.y);
      }
    } else {
      if (lights_enabled || global_illum_enabled) {
        ::base::glMultiTexCoord2fARB(GL_TEXTURE0_ARB, vertices_[i].lc.x,
                                     vertices_[i].lc.y);
      }
    }

    glNormal3fv(normal_.v);
    glColor4fv(vertices_[i].color.v);
    if (textures_enabled) {
      glTexCoord2fv(vertices_[i].tc.v);
    }
    glVertex3fv(vertices_[i].vert.v);
  }

  glEnd();

  if (requires_alpha_) {
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_ONE, GL_ONE);
  }
}

World::World(const string& filename) {
  if (!LoadWorldFromFile(filename)) {
    return;
  }

  PrepareTrianglesForLightmapping();

  // If we can load a lightmap bitmap from filename.lmp then we use it.
  // Otherwise we'll generate lightmaps.
  if (!LoadLightmapsFromFile(filename + ".lmp.bmp")) {
    normal_generator.initialize(RANDOM_NORMAL_COUNT);
    GenerateLightmaps();
    SaveLightmapsToFile(filename + ".lmp.bmp");
  }
}

bool World::IsValid() const { return true; }

void World::Draw(bool textures_enabled, bool lights_enabled,
                 bool global_illum_enabled) const {
  for (int i = 0; i < triangles_.size(); i++) {
    triangles_[i].Draw(textures_enabled, lights_enabled, global_illum_enabled);
  }
}

bool World::LoadWorldFromFile(const ::std::string& filename) {
  FILE* file_ptr = NULL;
  fopen_s(&file_ptr, filename.c_str(), "r");
  if (!file_ptr) {
    cout << "Failed to open world file " << filename << "." << endl;
    return false;
  }

  uint32 magic_value = 0;
  uint32 poly_count = 0;
  uint32 texture_count = 0;
  uint32 light_count = 0;

  char one_line[MAX_PATH] = {0};
  ReadOneLine(file_ptr, one_line);
  sscanf_s(one_line, "%i", &magic_value);

  if (magic_value != 99) {
    cout << "Invalid world file format detected. " << endl;
    fclose(file_ptr);
    return false;
  }

  /* Load our polygon (triangle) count. */
  ReadOneLine(file_ptr, one_line);
  sscanf_s(one_line, "poly %i", &poly_count);
  cout << "Polygon count: " << poly_count << "." << endl;

  /* Load our texture count. */
  ReadOneLine(file_ptr, one_line);
  sscanf_s(one_line, "texs %i", &texture_count);
  cout << "Texture count: " << texture_count << "." << endl;

  /* Load our light count. */
  ReadOneLine(file_ptr, one_line);
  sscanf_s(one_line, "lights %i", &light_count);
  cout << "Light count: " << light_count << "." << endl;

  /* Read and initialize our textures. */
  for (uint32 j = 0; j < texture_count; j++) {
    ReadOneLine(file_ptr, one_line);

    if (one_line[0] == 't') {
      char temp_string[80] = {0};
      sscanf_s(one_line, "t %s", temp_string, 80);
      ::std::shared_ptr<Texture> temp_texture_object =
          ::std::make_shared<Texture>(temp_string);
      textures_.emplace_back(temp_texture_object);
    }
  }

  /* Read and initialize our lights. */
  for (uint32 j = 0; j < light_count; j++) {
    ReadOneLine(file_ptr, one_line);
    vector3 light_position, light_color;
    float32 light_intensity = 0;

    if (one_line[0] == 'l') {
      sscanf_s(one_line, "l %f, %f, %f, %f, %f, %f, %f", &light_color.x,
               &light_color.y, &light_color.z, &light_position.x,
               &light_position.y, &light_position.z, &light_intensity);
      lights_.emplace_back(light_position, light_color, light_intensity);
    }
  }

  /* Read and initialize our polygons (triangles). */
  for (uint32 i = 0; i < poly_count; i++) {
    ReadOneLine(file_ptr, one_line);

    if (one_line[0] == 'f') {
      vector3 vertices[3];
      vector4 colors[3];
      vector2 texcoords[3];
      uint32 texture_index;
      uint32 vertex_index = 0;
      uint32 line_count = 0;

      sscanf_s(one_line, "f %i", &line_count);

      for (uint32 j = 0; j < line_count; j++) {
        ReadOneLine(file_ptr, one_line);

        if (one_line[0] == 'v') {
          sscanf_s(one_line, "v %f, %f, %f, %f, %f, %f, %f, %f, %f",
                   &vertices[vertex_index].x, &vertices[vertex_index].y,
                   &vertices[vertex_index].z, &texcoords[vertex_index].x,
                   &texcoords[vertex_index].y, &colors[vertex_index].r,
                   &colors[vertex_index].g, &colors[vertex_index].b,
                   &colors[vertex_index].a);
          vertex_index++;
        } else if (one_line[0] == 't') {
          sscanf_s(one_line, "t %i", &texture_index);
        }
      }

      triangles_.emplace_back(vertices[0], texcoords[0], colors[0], vertices[1],
                              texcoords[1], colors[1], vertices[2],
                              texcoords[2], colors[2],
                              textures_[texture_index]);
    }
  }

  return true;
}

void World::SaveLightmapsToFile(const ::std::string& filename) {
  if (!triangles_.size()) {
    return;
  }

  cout << "Saving lightmaps to file " << filename << "." << endl;

  uint32 first_lightmap_dim = triangles_[0].lightmap_->texture_width_;
  uint32 lightmap_size = first_lightmap_dim * first_lightmap_dim * 3;
  uint32 write_offset = 0;

  ::std::vector<uint8> lightmap_buffer(lightmap_size * triangles_.size() * 2);

  for (uint32 i = 0; i < triangles_.size(); i++) {
    memcpy(&lightmap_buffer.at(0) + write_offset,
           &triangles_[i].lightmap_->texture_map_.at(0), lightmap_size);
    write_offset += lightmap_size;
    memcpy(&lightmap_buffer.at(0) + write_offset,
           &triangles_[i].gi_lightmap_->texture_map_.at(0), lightmap_size);
    write_offset += lightmap_size;
  }

  ::base::SaveBitmapImage(filename, &lightmap_buffer, first_lightmap_dim,
                          first_lightmap_dim * triangles_.size() * 2);
}

bool World::LoadLightmapsFromFile(const ::std::string& filename) {
  if (!triangles_.size()) {
    return false;
  }

  uint32 width, height;
  ::std::vector<uint8> lightmap_buffer;
  if (!::base::LoadBitmapImage(filename, &lightmap_buffer, &width, &height)) {
    return false;
  }

  cout << "Loading lightmaps from file " << filename << "." << endl;

  uint32 first_lightmap_dim = width;
  uint32 lightmap_size = first_lightmap_dim * first_lightmap_dim * 3;
  uint32 read_offset = 0;

  cout << "Lightmap size: " << lightmap_buffer.size() / lightmap_size << endl;
  cout << "Triangle count: " << triangles_.size() << endl;

  if ((lightmap_buffer.size() / lightmap_size) < triangles_.size()) {
    // Lightmap count is less than triangle count, so this lightmap
    // was not generated for the current geometry. Regenerate it.
    cout << "Lightmap was not generated for the current map. Regenerating..."
         << endl;
    return false;
  }

  for (uint32 i = 0; i < triangles_.size(); i++) {
    memcpy(&triangles_[i].lightmap_->texture_map_.at(0),
           &lightmap_buffer.at(0) + read_offset, lightmap_size);
    read_offset += lightmap_size;
    memcpy(&triangles_[i].gi_lightmap_->texture_map_.at(0),
           &lightmap_buffer.at(0) + read_offset, lightmap_size);
    read_offset += lightmap_size;

    triangles_[i].lightmap_->UploadTexture();
    triangles_[i].gi_lightmap_->UploadTexture();
  }

  return true;
}

void World::PrepareTrianglesForLightmapping() {
  for (uint32 i = 0; i < triangles_.size(); ++i) {
    Triangle* tri = &triangles_[i];
    vector3 v0 = tri->vertices_[0].vert;
    vector3 v1 = tri->vertices_[1].vert;
    vector3 v2 = tri->vertices_[2].vert;

    // Compute the lightmap size based on the triangle dimensions.
    float32 max_length =
        max(max((v1 - v0).length(), (v2 - v0).length()), (v2 - v1).length());
    uint32 lightmap_width = lightmap_scale_factor * max_length;
    lightmap_width = ::base::clip_range(lightmap_width, MIN_LIGHTMAP_SIZE,
                                        MAX_LIGHTMAP_SIZE);

    // Generate a lightmap texture for this triangle and attach it.
    ::std::shared_ptr<Texture> lightmap =
        ::std::make_shared<Texture>(lightmap_width, lightmap_width);
    tri->AttachLightmap(lightmap);

    // Generate a global illumination lightmap texture for this triangle and
    // attach it.
    ::std::shared_ptr<Texture> gi_lightmap =
        ::std::make_shared<Texture>(lightmap_width, lightmap_width);
    tri->AttachGlobalLightmap(gi_lightmap);

    // Generate the UVs for the triangle and attach them.
    float32 max_u = -100000;
    float32 min_u = 100000;
    float32 max_v = -100000;
    float32 min_v = 100000;
    float32 delta_u = 0.0f;
    float32 delta_v = 0.0f;

    uint32 u_coeff = 0, v_coeff = 0;

    FindLightmapPlane(tri->plane_, &u_coeff, &v_coeff);

    // Compute the minimum and maximum values on our planar map.
    for (int j = 0; j < 3; j++) {
      if (min_u > tri->vertices_[j].vert.v[u_coeff])
        min_u = tri->vertices_[j].vert.v[u_coeff];

      if (min_v > tri->vertices_[j].vert.v[v_coeff])
        min_v = tri->vertices_[j].vert.v[v_coeff];

      if (max_u < tri->vertices_[j].vert.v[u_coeff])
        max_u = tri->vertices_[j].vert.v[u_coeff];

      if (max_v < tri->vertices_[j].vert.v[v_coeff])
        max_v = tri->vertices_[j].vert.v[v_coeff];
    }

    delta_u = (max_u - min_u);
    delta_v = (max_v - min_v);

    if (delta_u == 0.0) delta_u = 0.0001f;
    if (delta_v == 0.0) delta_v = 0.0001f;

    for (int e = 0; e < 3; e++) {
      tri->vertices_[e].lc[0] =
          (tri->vertices_[e].vert.v[u_coeff] - min_u) / delta_u;
      tri->vertices_[e].lc[1] =
          (tri->vertices_[e].vert.v[v_coeff] - min_v) / delta_v;
    }
  }
}

void ComputeDirectIlluminationHelper(World* world, uint32 thread_index) {
  ::std::vector<Triangle>& triangles_ = world->triangles_;
  ::std::vector<Light>& lights_ = world->lights_;
  uint32 triangle_bin_count = triangles_.size();

#if ENABLE_MULTITHREADING
  uint32 thread_count =
      min(triangles_.size(), ::std::thread::hardware_concurrency());
  triangle_bin_count = triangle_bin_count / thread_count;
#endif

  uint32 triangle_start_index = triangle_bin_count * thread_index;
  uint32 triangle_stop_index = triangle_bin_count * (thread_index + 1);

#if ENABLE_MULTITHREADING
  if (thread_index == thread_count - 1) {
    triangle_stop_index = triangles_.size();
  }
#endif

  // For each triangle
  for (uint32 i = triangle_start_index; i < triangle_stop_index; ++i) {
    Triangle* tri = &triangles_[i];
    float32 width = tri->lightmap_->texture_width_;
    float32 height = tri->lightmap_->texture_height_;
    vector2 v0 = tri->vertices_[1].lc - tri->vertices_[0].lc;
    vector2 v1 = tri->vertices_[2].lc - tri->vertices_[0].lc;
    for (uint32 lx = 0; lx < tri->lightmap_->texture_width_; lx++) {
      for (uint32 ly = 0; ly < tri->lightmap_->texture_height_; ly++) {
        vector2 lumel(::base::clip_range(lx / width, 0.0, 1.0),
                      ::base::clip_range(ly / height, 0.0, 1.0));
        vector2 vp = lumel - tri->vertices_[0].lc;
        float32 u = 0.0, v = 0.0;
        ::base::triangle_find_barycentric_coeff(v0, v1, vp, &u, &v);

        /*
        float32 three_lumels_u = 3.0f / tri->lightmap_->texture_width_;
        float32 three_lumels_v = 3.0f / tri->lightmap_->texture_height_;
        // Only trace lumels that lie on our triangles, plus a 3px boundary
        // around the edge of the triangle, to account for bilinear sampling
        // (and avoid black seams at the edges of the triangles after blending).
        if ((u < -three_lumels_u) || (v < -three_lumels_v) ||
            (u + v > 1 + three_lumels_u + three_lumels_v)) {
          continue;
        }
        */

        // We have a lumel that's inside the triangle. Map it to a point.
        vector3 trace_origin;
        ::base::triangle_interpolate_barycentric_coeff(
            tri->vertices_[0].vert, tri->vertices_[1].vert,
            tri->vertices_[2].vert, u, v, &trace_origin);

        for (uint32 light = 0; light < lights_.size(); light++) {
          // Compute the trace vector and then check it against all other
          // geometry.
          ::base::ray trace_ray(trace_origin, lights_[light].position_);
          ::base::collision hit_info;

          for (uint32 j = 0; j < triangles_.size(); j++) {
            if (i == j) {
              continue;
            }

            Triangle* test_tri = &triangles_[j];
            if (test_tri->requires_alpha_) {
              // Ignore transparent or partially transparent triangles.
              continue;
            }

            // If we hit any triangle then we exit early.
            if (::base::ray_intersect_triangle(
                    test_tri->vertices_[0].vert, test_tri->vertices_[1].vert,
                    test_tri->vertices_[2].vert, test_tri->plane_, trace_ray,
                    &hit_info, NULL)) {
              // Perform a second level of scrutiny.
              if (hit_info.param > BASE_EPSILON &&
                      hit_info.param < 1.0 - BASE_EPSILON ||
                  fabs(hit_info.normal.dot(test_tri->normal_)) < BASE_EPSILON) {
                break;
              }
            }
          }

          if (hit_info.param > 1 - BASE_EPSILON ||
              hit_info.param < BASE_EPSILON) {
            // We did not hit anything. Compute the light value and store it.
            vector3 incident = (lights_[light].position_ - trace_origin);
            float32 length = incident.length();
            float32 dot = fabs(incident.normalize().dot(tri->normal_));
            float32 attenuation =
                (500.0f * lights_[light].intensity_) / (1.0 + pow(length, 2));
            vector3 illum = lights_[light].color_ * dot * attenuation;
            illum.x = pow(::base::saturate(illum.x), 1.0 / 2.6);
            illum.y = pow(::base::saturate(illum.y), 1.0 / 2.6);
            illum.z = pow(::base::saturate(illum.z), 1.0 / 2.6);
            illum += tri->lightmap_->ReadTexel(lumel);
            illum = illum.clamp(0.0, 1.0);
            tri->lightmap_->WriteTexel(lumel, illum);
          }
        }
      }
    }

    // tri->lightmap_->BlurTexture(3, 1);
    // tri->lightmap_->BlurTexture(3, 1);
  }
}

void World::ComputeDirectIllumination() {
#if ENABLE_MULTITHREADING
  ::std::vector<::std::thread> thread_list;
  uint32 thread_count =
      min(triangles_.size(), ::std::thread::hardware_concurrency());

  for (uint32 thread_idx = 0; thread_idx < thread_count; thread_idx++) {
    thread_list.emplace_back(&ComputeDirectIlluminationHelper, this,
                             thread_idx);
  }

  for (auto& thread_ : thread_list) {
    thread_.join();
  }
#else
  ComputeDirectIlluminationHelper(this, 0);
#endif

  // Upload all of our textures to the GPU.
  for (uint32 i = 0; i < triangles_.size(); i++) {
    Triangle* tri = &triangles_[i];
    tri->lightmap_->UploadTexture();
  }

  cout << "Completed direct illumination pass." << endl;
}

void ComputeIndirectIlluminationHelper(World* world, uint32 thread_index) {
  ::std::vector<Triangle>& triangles_ = world->triangles_;
  ::std::vector<Light>& lights_ = world->lights_;
  ::base::normal_sphere& normal_generator = world->normal_generator;
  uint32 triangle_bin_count = triangles_.size();

#if ENABLE_MULTITHREADING
  uint32 thread_count =
      min(triangles_.size(), ::std::thread::hardware_concurrency());
  triangle_bin_count = triangle_bin_count / thread_count;
#endif

  uint32 triangle_start_index = triangle_bin_count * thread_index;
  uint32 triangle_stop_index = triangle_bin_count * (thread_index + 1);

#if ENABLE_MULTITHREADING
  if (thread_index == thread_count - 1) {
    triangle_stop_index = triangles_.size();
  }
#endif

  // For each triangle
  for (uint32 i = triangle_start_index; i < triangle_stop_index; ++i) {
    Triangle* tri = &triangles_[i];
    float32 width = tri->gi_lightmap_->texture_width_;
    float32 height = tri->gi_lightmap_->texture_height_;
    vector2 v0 = tri->vertices_[1].lc - tri->vertices_[0].lc;
    vector2 v1 = tri->vertices_[2].lc - tri->vertices_[0].lc;

    for (uint32 lx = 0; lx < tri->gi_lightmap_->texture_width_; lx++) {
      for (uint32 ly = 0; ly < tri->gi_lightmap_->texture_height_; ly++) {
        vector2 lumel(::base::clip_range(lx / width, 0.0, 1.0),
                      ::base::clip_range(ly / height, 0.0, 1.0));
        vector2 vp = lumel - tri->vertices_[0].lc;
        float32 u = 0.0, v = 0.0;
        ::base::triangle_find_barycentric_coeff(v0, v1, vp, &u, &v);

        /*
        float32 three_lumels_u = 3.0f / tri->gi_lightmap_->texture_width_;
        float32 three_lumels_v = 3.0f / tri->gi_lightmap_->texture_height_;
        // Only trace lumels that lie on our triangles, plus a 3px boundary
        // around the edge of the triangle, to account for bilinear sampling
        // (and avoid black seams at the edges of the triangles after blending).
        if ((u < -three_lumels_u) || (v < -three_lumels_v) ||
            (u + v > 1 + three_lumels_u + three_lumels_v)) {
          continue;
        }
        */

        // We have a lumel that's inside the triangle. Map it to a point.
        vector3 trace_origin;
        ::base::triangle_interpolate_barycentric_coeff(
            tri->vertices_[0].vert, tri->vertices_[1].vert,
            tri->vertices_[2].vert, u, v, &trace_origin);

        // Copy over our direct illumination value.
        vector3 illumination;  // = tri->lightmap_->ReadTexel(lumel);
        float32 sample_count = 0.0f;

        // Now we run SAMPLE_COUNT samples, pointing in random directions and
        // pulling whatever light data we hit, and averaging the values in.

        for (uint32 sample = 0; sample < SAMPLE_COUNT; sample++) {
          vector3 ray_target =
              trace_origin + normal_generator.random_reflection(
                                 tri->normal_ * -1.0, tri->normal_, BASE_PI) *
                                 1000.0;

          ::base::ray trace_ray(trace_origin, ray_target);
          ::base::collision hit_info;
          vector2 best_bary_coords;
          Triangle* best_hit_tri = NULL;

          for (uint32 j = 0; j < triangles_.size(); j++) {
            if (i == j) {
              continue;
            }

            Triangle* test_tri = &triangles_[j];

            if (test_tri->requires_alpha_) {
              // Ignore transparent or partially transparent triangles.
              continue;
            }

            ::base::collision test_hit;
            vector2 test_bary_coords;
            // Test for collision, keeping track of the closest one.
            if (::base::ray_intersect_triangle(
                    test_tri->vertices_[0].vert, test_tri->vertices_[1].vert,
                    test_tri->vertices_[2].vert, test_tri->plane_, trace_ray,
                    &test_hit, &test_bary_coords)) {
              if (test_hit.param < hit_info.param &&
                  test_hit.param > BASE_EPSILON &&
                  test_hit.param < 1.0f - BASE_EPSILON) {
                hit_info = test_hit;
                best_hit_tri = test_tri;
                best_bary_coords = test_bary_coords;
              }
            }
          }

          if (hit_info.param < 1.0 - BASE_EPSILON &&
              hit_info.param > BASE_EPSILON) {
            // We hit something -- sample it's lighting and add it to our total.
            vector3 incident = (hit_info.point - trace_origin);
            const vector3& t0 = best_hit_tri->vertices_[0].tc;
            const vector3& t1 = best_hit_tri->vertices_[1].tc;
            const vector3& t2 = best_hit_tri->vertices_[2].tc;

            const vector3& l0 = best_hit_tri->vertices_[0].lc;
            const vector3& l1 = best_hit_tri->vertices_[1].lc;
            const vector3& l2 = best_hit_tri->vertices_[2].lc;

            const vector3& c0 = best_hit_tri->vertices_[0].color;
            const vector3& c1 = best_hit_tri->vertices_[1].color;
            const vector3& c2 = best_hit_tri->vertices_[2].color;

            vector3 output_texcoords, output_lightcoords, output_color;
            ;

            triangle_interpolate_barycentric_coeff(
                t0, t1, t2, best_bary_coords.x, best_bary_coords.y,
                &output_texcoords);

            triangle_interpolate_barycentric_coeff(
                l0, l1, l2, best_bary_coords.x, best_bary_coords.y,
                &output_lightcoords);

            triangle_interpolate_barycentric_coeff(
                c0, c1, c2, best_bary_coords.x, best_bary_coords.y,
                &output_color);

            vector2 target_lc =
                vector2(output_lightcoords.x, output_lightcoords.y);
            vector2 target_tc = vector2(fmod(output_texcoords.x, 1.0),
                                        fmod(output_texcoords.y, 1.0));

            vector3 color = best_hit_tri->lightmap_->ReadTexel(target_lc) *
                            best_hit_tri->diffuse_->ReadTexel(target_tc) *
                            output_color;

            illumination +=
                color * fabs(incident.normalize().dot(tri->normal_));
            sample_count += 1.0f;
          }
        }

        if (sample_count) {
          illumination = illumination / sample_count;
          illumination.x = pow(::base::saturate(illumination.x), 1.0 / 2.6);
          illumination.y = pow(::base::saturate(illumination.y), 1.0 / 2.6);
          illumination.z = pow(::base::saturate(illumination.z), 1.0 / 2.6);
        }

        illumination += tri->lightmap_->ReadTexel(lumel);
        illumination = illumination.clamp(0.0, 1.0);
        tri->gi_lightmap_->WriteTexel(lumel, illumination);
      }
    }

    tri->gi_lightmap_->BlurTexture(3, 1);
    tri->gi_lightmap_->BlurTexture(3, 1);
  }
}

void World::ComputeIndirectIllumination() {
#if ENABLE_MULTITHREADING
  ::std::vector<::std::thread> thread_list;
  uint32 thread_count =
      min(triangles_.size(), ::std::thread::hardware_concurrency());

  for (uint32 thread_idx = 0; thread_idx < thread_count; thread_idx++) {
    thread_list.emplace_back(&ComputeIndirectIlluminationHelper, this,
                             thread_idx);
  }

  for (auto& thread_ : thread_list) {
    thread_.join();
  }
#else
  ComputeIndirectIlluminationHelper(this, 0);
#endif

  // Upload all of our textures to the GPU.
  for (uint32 i = 0; i < triangles_.size(); i++) {
    Triangle* tri = &triangles_[i];
    tri->gi_lightmap_->UploadTexture();
  }

  cout << "Completed global illumination pass." << endl;
}

void World::GenerateLightmaps() {
  // Pass 1: direct illumination contribution.
  ComputeDirectIllumination();
  // Pass 2: indirect illumination contribution.
  ComputeIndirectIllumination();
}