/*
//
// Copyright (c) 1998-2014 Joe Bertolami. All Right Reserved.
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

#ifndef __ASSETS_H__
#define __ASSETS_H__

#include <memory>
#include <string>
#include <vector>

#include "jmath/base.h"
#include "jmath/normal.h"
#include "jmath/vector3.h"
#include "jmath/vector4.h"

using ::base::float32;
using ::base::int32;
using ::base::uint32;
using ::base::uint8;
using ::base::vector2;
using ::base::vector3;
using ::base::vector4;

class World;

class Light {
  friend class World;
  friend void ComputeDirectIlluminationHelper(World* world, uint32 thread_index);
  friend void ComputeIndirectIlluminationHelper(World* world, uint32 thread_index);

 public:
  Light(const vector3& position, const vector4& color, float32 intensity);

 private:
  vector4 color_;
  vector3 position_;
  float32 intensity_;
  bool enabled_;
};

class Texture {
  friend World;
  friend void ComputeDirectIlluminationHelper(World* world, uint32 thread_index);
  friend void ComputeIndirectIlluminationHelper(World* world, uint32 thread_index);

 public:
  // Loads a texture (bitmap) into memory.
  Texture(const ::std::string& filename);
  // Creates a texture with the specified dimensions.
  Texture(uint32 width, uint32 height);
  // Destroys the texture and clears it from graphics memory.
  virtual ~Texture();
  // Binds the texture to the current graphics context, at the unit
  // indicated by unit_index.
  void Bind(int unit_index);
  // Returns the texel at the specified coordinate.
  vector3 ReadTexel(const vector2& coord) const;
  // Writes a texel at the specified coordinate.
  void WriteTexel(const vector2& coord, const vector3& texel);
  // Performs a 3x3 blur of the texture data.
  void BlurTexture(int32 radius, int32 step);
  // Uploads the current texture state to the GPU.
  void UploadTexture();

 private:
  // The internal graphics index for this texture.
  uint32 gl_texture_index_;
  // The graphical data for this texture.
  ::std::vector<uint8> texture_map_;
  // Texture image width
  uint32 texture_width_;
  // Texture image height
  uint32 texture_height_;
};

typedef struct Vertex {
  // The vertex coordinate.
  vector3 vert;
  // The color for this vertex.
  vector4 color;
  // The texture coordinate;
  vector2 tc;
  // The lightmap coordinate;
  vector2 lc;
} Vertex;

class Triangle {
 friend World;
 friend void ComputeDirectIlluminationHelper(World* world, uint32 thread_index);
 friend void ComputeIndirectIlluminationHelper(World* world, uint32 thread_index);

 public:
  // Also allocates a lightmap texture based on the size of the triangle and our
  // scale factor.
  Triangle(const vector3& v0, const vector2& t0, const vector4& c0, const vector3& v1,
           const vector2& t1, const vector4& c1, const vector3& v2, const vector2& t2, const vector4& c2,
           const ::std::shared_ptr<Texture>& diffuse);
  // Attaches a lightmap texture to the triangle.
  void AttachLightmap(const ::std::shared_ptr<Texture>& lightmap);
  // Attaches a global illumination lightmap texture to the triangle.
  void AttachGlobalLightmap(const ::std::shared_ptr<Texture>& lightmap);
  // Samples the modulated light at the given world coordinate.
  vector3 ReadLight(const vector3& point) const;
  // Renders the triangle with multitexturing, if enabled.
  void Draw(bool textures_enabled, bool lights_enabled,
            bool global_illum_enabled) const;

 private:
  // Three vertices for our triangle.
  Vertex vertices_[3];
  // The normal vector for the triangle.
  vector3 normal_;
  // The plane of the triangle (to speed up collision checks).
  vector4 plane_;
  // The diffuse texture.
  ::std::shared_ptr<Texture> diffuse_;
  // The lightmap texture.
  ::std::shared_ptr<Texture> lightmap_;
  // The global illumination lightmap texture.
  ::std::shared_ptr<Texture> gi_lightmap_;
  // True if the triangle requires alpha blending.
  bool requires_alpha_;
};

class World {
 friend void ComputeDirectIlluminationHelper(World* world, uint32 thread_index);
 friend void ComputeIndirectIlluminationHelper(World* world, uint32 thread_index);

 public:
  // Load a file and compute its lightmap.
  World(const ::std::string& filename);
  // Returns true if the world was initialized successfully.
  bool IsValid() const;
  // Render the world.
  void Draw(bool textures_enabled, bool lights_enabled,
            bool global_illum_enabled) const;

 private:
  ::std::vector<Triangle> triangles_;
  ::std::vector<Light> lights_;
  ::std::vector<::std::shared_ptr<Texture>> textures_;
  // A random normal generator.
  ::base::normal_sphere normal_generator;
  // Parses the world file and loads its contents.
  bool LoadWorldFromFile(const ::std::string& filename);
  // Parses a lightmap file and loads its contents.
  bool LoadLightmapsFromFile(const ::std::string& filename);
  // Saves our lightmap data to the specified file.
  void SaveLightmapsToFile(const ::std::string& filename);
  // Generates lightmaps for all surfaces in the world.
  void GenerateLightmaps();
  // Initializes lightmap memory and sets up lightmap UVs.
  void PrepareTrianglesForLightmapping();
  // Updates the level-1 lightmap with direct illumination.
  void ComputeDirectIllumination();
  // Updates the level-2 lightmap with indirect illumination.
  void ComputeIndirectIllumination();
};

#endif  // __ASSETS_H__