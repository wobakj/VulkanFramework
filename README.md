# VulkanFramework
Focused on multithreaded and distributed vulkan applications and LOD rendering. Development started as part of a [semester project](https://www.uni-weimar.de/de/medien/professuren/medieninformatik/vr/teaching/ws-201617/project-inside-vulkan/)
* ### Framework
    3 level framework
    * **wrap:** RAI wrappers for all important Vulkan objects
    * **util:** convenience classes to allocate memory, load textures, wrap geometry etc.
    * **app:** hierarchical application and launcher classes to run the same high-level application in different modes
* ### Scenegraph
    simple scenegraph implementation supporting:
    * Transform Node: scale, translation, rotation, children
    * Shape Node: geometry
    * Light Node: color, intensity, location, radius
    * Camera Node: automatically created, not loaded from json
    
    the scene is described in a json file based on the JSON Scenegraph specification
    each model requires a name ($DEF)
### Applications
* **Minimal:** drawing a triangle
* **Simple:** drawing sphere.obj and sponza.obj [(external)](graphics.cs.williams.edu/data/) with 1st person navigation 
* **Compute:** calculating an animated texture usinga  computer shader
* **Clustered:** drawing sponza.obj [(external, PBR)](http://www.alexandre-pestana.com/pbr-textures-sponza/) using clustered shading
* **Scenegraph** drawing a Scenegraph based on the [JSON Scenegraph](https://github.com/wildpeaks/json-scenegraph) format
* **Scenegraph Clustered** drawing a Scenegraph based on the [JSON Scenegraph](https://github.com/wildpeaks/json-scenegraph) format with clustered PBR shading
* **LOD MPI** draws an xyz_rgb file with bvh with 3 processes. 1 presenter and 2 workers
* **LOD** draws an xyz_rgb file with bvh

## Modes
each application can be run in different modes
* **level 1** specified in the main function, worker mode currently only used in LOD MPI app
    * Window: a window is created and the application renders to the window
    * Worker: the application renders tp an image and tranmits it via MPI to a presenter
* **level 2** can be chosen at runtime using the "-t 1 to 3" argument 
    * Single: application runs in 1 thread
    * Threaded: application has a separate drawing thread in parallel to logic, recording and presenting
    * Threaded Transfer: application additionally has a thread for data transfer 

## Libraries
* [**SPIR-V Cross**](https://github.com/KhronosGroup/SPIRV-Cross) for shader compilation 
* [**cmdline**](https://github.com/tanakh/cmdline) for argument parsing
* [**GLFW**](http://www.glfw.org/) for window and context creation
* [**GLM**](glm.g-truc.net/) for mathematics
* [**GLI**](http://gli.g-truc.net/0.8.2/index.html) for dds image loading
* [**stb_image**](https://github.com/nothings/stb) for tga image loading
* [**tinyobjloader**](http://syoyo.github.io/tinyobjloader/) for obj model loading
* [**lamure (bvh)**](https://github.com/vrsys/lamure) for bvh and xzy_rgb model loading
* [**jsoncpp**](https://github.com/open-source-parsers/jsoncpp) for json scenegraph loading

## License
this framework is licensed under the GPL v3
* **SPIR-V Cross** is licensed under Apache license 2.0
* **cmdline** is licensed under 3 clause BSD revised
* **GLFW** is licensed under the zlib/libpng license
* **GLM** is licensed under the Happy Bunny License (Modified MIT)
* **GLI** is licensed under the Happy Bunny License (Modified MIT)
* **stb_image** is public domain
* **tinyobjloader** is licensed under 2 clause BSD
* **lamure** is licensed under 3 clause BSD
* **jsoncpp** is public domain
