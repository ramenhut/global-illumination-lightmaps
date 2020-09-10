## Global Illumination with Static Lightmaps
Lightmapping is a simple technique for baking high quality lighting information into textures for static lighting environments. This process gained popularity in the mid 1990s for its visual quality and efficiency, and continues to be used by modern games when performance is the top priority. There are several common methods of computing lightmaps, but approaches generally fall into one of two categories: direct and indirect (aka global) illumination. For this project I wrote a simple and flexible lightmap generator that computes high quality lightmaps using both direct and indirect illumination.

This project was completed in 2002 for an undergraduate course in computer graphics, and the code was later updated in 2014 to add multi-threading support. 

### Features
* Direct and indirect illumination solvers

* Linear math and tracing library

* Lightmap disk caching

* Simple map file support

* Multi-threaded lightmap generation

### Screenshots
![Screenshot](https://github.com/ramenhut/global-illumination-lightmaps/raw/master/thumbnails/gil_1-s.jpg?raw=true)
![Screenshot](https://github.com/ramenhut/global-illumination-lightmaps/raw/master/thumbnails/gil_2-s.jpg?raw=true)
![Screenshot](https://github.com/ramenhut/global-illumination-lightmaps/raw/master/thumbnails/gil_3-s.jpg?raw=true)
![Screenshot](https://github.com/ramenhut/global-illumination-lightmaps/raw/master/thumbnails/gil_4-s.jpg?raw=true)

### License

This software is released under the terms of the BSD 2-Clause Simplified License.

### More Information
For more information, including pre-built binaries and media, visit [http://www.bertolami.com](http://bertolami.com/index.php?engine=portfolio&content=computer-graphics&detail=global-illumination-lightmaps).