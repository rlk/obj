# obj

`obj` loads, manipulates, optimizes, renders, and stores 3D geometry using the Wavefront OBJ file format. The renderer uses the OpenGL 3.2 Core Profile, making it compatible with OpenGL versions 3 and 4, OpenGL ES 2.0, and also commonly-available extended implementations of OpenGL 2.1.

-   [obj.c](obj.c)
-   [obj.h](obj.h)

An OBJ file consists of sets of *materials*, *vertices*, and *surfaces*. A surface consists of sets of *polygons* and *lines* with a reference to a material to be used when rendering them. A polygon is a triplet of references to vertices in the file, and a line is a pair of references to vertices.

OBJ files are referenced using pointers to type `obj`. The elements within the OBJ are referenced by integer index. These indices work much like a file or socket descriptor. The internal geometry representation is not accessible to the application. All operations on geometry are performed using the API documented here.

## Compilation

To use this module, simply link it with your own code. The renderer requires OpenGL. Linux and Windows compilation also requires [GLEW](http://glew.sourceforge.net/)...

    cc -o program program.c obj.c -lGLEW -lGL -lm

Though OSX's native OpenGL support suffices...

    cc -o program program.c obj.c -framework OpenGL -lm

If used only to process OBJ files and not to render them, the OpenGL dependency may be eliminated by defining `CONF_NO_GL`.

    cc -o program program.c obj.c -DCONF_NO_GL -lm

## Quickstart

These code fragments implement the common case of loading and displaying a model stored in an OBJ file. First, an OBJ pointer is declared.

    obj *O;

During initialization, an `obj` structure is allocated, the OBJ file and all of its MTL files and texture images are read, and the `obj` pointer is returned.

    O = obj_create("teapot.obj");

During rendering:

    obj_render(O);

When no longer needed, all resources held by the `obj` data structure are released:

    obj_delete(O);

## The OBJ API

### Top level

As we've just seen, the top level API for manipulation OBJ files is as follows:

- `obj *obj_create(const char *filename)`

    Create a new file object, load the OBJ file named by `filename`, and return a file object. If `filename` is `NULL` then an empty file is returned.

- `void obj_delete(obj *O)`

    Delete OBJ `O` and release all resources held by it.

### Rendering

- `void obj_set_vert_loc(obj *O, int u, int n, int t, int v)`

    Set the vertex attribute locations for the tangent, normal, texture coordinate, and position. These are acquired by calling [`glGetAttribLocation`](http://www.opengl.org/sdk/docs/man/html/glGetAttribLocation.xhtml) with the intended shader program. Pass `-1` for the location of any attribue that the shader does not receive.

    For example, if program object `program` declares `in` varibles named `vTangent`, `vNormal`, `vTexCoord` and `vPosition` then the syntax is:

        obj_set_vert_loc(object, glGetAttribLocation(program, "vTangent"),
                                 glGetAttribLocation(program, "vNormal"),
                                 glGetAttribLocation(program, "vTexCoord"),
                                 glGetAttribLocation(program, "vPosition"));


- `void obj_set_prop_loc(obj *O, int ki, int c, int o, int M)`

    Set the uniform location for the color, texture sampler index, or texture transform for material property `ki`. Pass `-1` for the location of any uniform that the shader does not receive. The value of `ki` must be one of the following:

    <table style="margin: auto">
      <tr><td><code>OBJ_KD</code></td><td>Diffuse color</td></tr>
      <tr><td><code>OBJ_KA</code></td><td>Ambient color</td></tr>
      <tr><td><code>OBJ_KE</code></td><td>Emissive color</td></tr>
      <tr><td><code>OBJ_KS</code></td><td>Specular color</td></tr>
      <tr><td><code>OBJ_NS</code></td><td>Specular exponent</td></tr>
      <tr><td><code>OBJ_KN</code></td><td>Normal</td></tr>
    </table>

    For example, if program object `program` declares diffuse and normal samplers named `DiffuseTexture` and `NormalTexture`, but no colors, transforms, or other material properties, then the syntax is:

        obj_set_prop_loc(object, OBJ_KD, -1, glGetUniformLocation(program, "DiffuseTexture"), -1);
        obj_set_prop_loc(object, OBJ_KN, -1, glGetUniformLocation(program, "NormalTexture"), -1);

    The material color will be set using `glUniform4fv`, the sampler index with `glUniform1i`, and the texture transform with `glUniformMatrix4fv`.

- `void obj_render(obj *O)`

    Render OBJ `O`. The polygons and lines of all surfaces are rendered using their assigned materials. Aside from materials and textures, no OpenGL state is modified. In particular, any bound vertex and fragment shaders execute as expected.

### Element Creation

- `int obj_add_mtrl(obj *O)`

    Add a new material to OBJ `O`, returning a material index. The new material is initialized with a diffuse color of (0.8, 0.8, 0.8, 1.0), an ambient color of (0.2, 0.2, 0.2, 1.0), an emissive color of (0.0, 0.0, 0.0, 0.0), a specular color of (0.0, 0.0, 0.0, 0.0), and a specular exponent of 8.0.

- `int obj_add_vert(obj *O)`

    Add a new vertex to OBJ `O`, returning a vertex index. The new vertex position, normal, and texture coordinate are initialized to zero.

- `int obj_add_surf(obj *O)`

    Add a new surface to OBJ `O`, returning a surface index. The new surface is initialized to reference material index zero. If no materials are defined when this surface is rendered, the default material is applied.

- `int obj_add_poly(obj *O, int si)`

    Add a new 3-sided polygon to surface `si` of OBJ `O`, returning a polygon index. The new polygon's vertex indices are initialized to zero.

- `int obj_add_line(obj *O, int si)`

    Add a new line to surface `si` of OBJ `O`, returning a line index. The new line's vertex indices are initialized to zero.

### Element Counters

- `int obj_num_mtrl(const obj *O)`
- `int obj_num_vert(const obj *O)`
- `int obj_num_surf(const obj *O)`

    Return the number of materials, vertices, and surfaces contained by OBJ `O`. All indices less than this number are valid indices into that file.

- `int obj_num_poly(const obj *O, int si)`
- `int obj_num_line(const obj *O, int si)`

    Return the number of polygons and lines contained by surface `si` of OBJ `O`. All indices less than this number are valid indices into that surface.

### Element deletion

Note: The element deletion API goes to great lengths to ensure that all geometry blocks are free of gaps, and that all internal references are consistent. If an application removes an element from the middle of a block then all higher-index elements are shifted down, and any references to these elements are decremented. Be aware: if an application caches element indices elsewhere, then these indices may be invalidated by a deletion operation.

- `void obj_del_mtrl(obj *O, int mi)`

    Remove material `mi` from OBJ `O`. Any surfaces in OBJ `O` that reference this material will also be removed. All higher-indexed materials are shifted down. Surfaces that refer to higher-indexed materials have their material indices decremented.

- `void obj_del_vert(obj *O, int vi)`

    Remove vertex `vi` from OBJ `O`. Any polygons or lines referencing this vertex in any surface of OBJ `O` are also removed. Higher-indexed vertices are shifted down. Polygons and lines that refer to higher-index vertices have their vertex indices decremented.

- `void obj_del_poly(obj *O, int si, int pi)`

    Remove polygon `pi` from surface `si` of OBJ `O`. Higher-indexed polygons are shifted down. Vertices referenced by polygon `pi` are *not* removed, as they may be refered-to by other polygons or lines.

- `void obj_del_line(obj *O, int si, int li)`

    Remove line `li` from surface `si` of OBJ `O`. Higher-indexed lines are shifted down. Vertices referenced by line `li` are *not* removed, as they may be refered-to by other polygons or lines.

- `void obj_del_surf(obj *O, int si)`

    Remove surface `si` from OBJ `O`. All polygons and lines contained in this surface are also removed. Vertices in OBJ `O` referenced by these polygons and lines are *not* removed. Higher-indexed surfaces are shifted down.

### Entity Manipulators

- `void obj_set_mtrl_name(obj *O, int mi, const char *name)`

    Set the name of material `mi` in OBJ `O`. This name is the means by which materials defined in an MTL file are referenced from within an OBJ file. Materials without assigned names are referenced as "default." If materials are not assigned unique names, then then the surface-material mapping is not guaranteed to be preserved when a file is written.

- `void obj_set_mtrl_map(obj *O, int mi, int ki, const char *image)`

    Set the image map to be used for diffuse color, ambient color, emissive color, specular color, specular exponent, or normal map of material `mi` of OBJ `O`. As of this writing, the file must be given in Targa (`.TGA`) 24-bit or 32-bit format.

- `void obj_set_mtrl_opt(obj *O, int mi, int ki, unsigned int opt)`

    Set the options on property `ki` of material `mi` in OBJ `O`.

<a name="option" id="option"></a>The `opt` argument gives a bitmap of options to be enabled. The following options are defined. Yeah, there's only one right now.

<table style="margin: auto">
  <tr><td><code>OBJ_OPT_CLAMP</code></td><td>Clamp the property map. Default is to repeat.</td></tr>
</table>

- `void obj_set_mtrl_c(obj *O, int mi, int ki, const float c[4])`

    Set the diffuse color, ambient color, emissive color, specular color, or specular exponent of material `mi` of OBJ `O`. The `ki` argument selects the property to be set. Note that the MTL file format supports RGBA diffuse color, but only RGB ambient, and specular colors. So while ambient and specular alpha values will be rendered normally, they cannot be stored in an MTL file, and will always default to 1.0 when a material is loaded. The specular exponent is stored and applied as a scalar.

- `void obj_set_mtrl_o(obj *O, int mi, int ki, const float o[3])`

    Set the texture coordinate offset for property map `ki` of material `mi` in OBJ `O`.

- `void obj_set_mtrl_s(obj *O, int mi, int ki, const float s[3])`

    Set the texture coordinate scale for property map `ki` of material `mi` in OBJ `O`.

- `void obj_set_vert_v(obj *O, int vi, const float *v)`
- `void obj_set_vert_t(obj *O, int vi, const float *t)`
- `void obj_set_vert_n(obj *O, int vi, const float *n)`

    Set the 3D position, 2D texture coordinate, or 3D normal vector of vertex `vi` of file fi.

- `void obj_set_poly(obj *O, int si, int pi, const int *vi)`

    Set the triplet of vertex indices defining polygon `pi` in surface `si` of OBJ `O`.

- `void obj_set_line(obj *O, int si, int li, const int *vi)`

    Set the pair of vertex indices defining line `li` in surface `si` of OBJ `O`.

- `void obj_set_surf(obj *O, int si, int mi)`

    Set the material index of surface `si` of OBJ `O`

### Entity Query

- `const char *obj_get_mtrl_name(const obj *O, int mi)`

    Return the name of material `mi` of OBJ `O`.

- `unsigned int obj_get_mtrl_map(const obj *O, int mi, int ki)`

    Return the property map `ki` of material `mi` of OBJ `O`. The returned value is an OpenGL texture object that may be manipulated normally using the OpenGL API.

- `unsigned int obj_get_mtrl_opt(const obj *O, int mi, int ki)`

    Return the bitmap of options set on the property map `ki` of material `mi` of OBJ `O`.

- `void obj_get_mtrl_c(const obj *O, int mi, int ki, float *c)`

    Return the RGBA color of property `ki` of material `mi` in OBJ `O`.

- `void obj_get_mtrl_o(const obj *O, int mi, int ki, float *o)`

    Return the 3D texture coordinate offset of property map `ki` of material `mi` in OBJ `O`. (This value *is* 3D despite the texture coordinates being 2D.)

- `void obj_get_mtrl_s(const obj *O, int mi, int ki, float *s)`

    Return the 3D texture coordinate scale of property map `ki` of material `mi` in OBJ `O`. (This value *is* 3D despite the texture coordinates being 2D.)

- `void obj_get_vert_v(const obj *O, int vi, float *v)`
- `void obj_get_vert_t(const obj *O, int vi, float *t)`
- `void obj_get_vert_n(const obj *O, int vi, float *n)`

    Return the 3D position, 2D texture coordinate, or 3D normal vector of vertex `vi` of OBJ `O`.

- `void obj_get_poly(const obj *O, int si, int pi, int *vi)`

    Return the triplet of indices defining polygon `pi` in surface `si` of OBJ `O`.

- `void obj_get_line(const obj *O, int si, int li, int *vi)`

    Return the pair of indices defining line `li` in surface `si` of OBJ `O`.

- `int obj_get_surf(const obj *O, int si)`

    Return the material index of surface `si` of OBJ `O`.

### OBJ I/O

#### Processing

- `void obj_proc(obj *O)`

    Process OBJ `O` for rendering. All normal vectors are normalized and a tangent vector is computed for each vertex using its normal vector and texture coordinate. Surfaces are sorted in order of increasing transparency in order to correct blending order.

- `float obj_acmr(obj *O, int qc)`

    Compute the *average cache miss ratio* (ACMR) for OBJ `O` using a cache size of `qc`. The ACMR gives a measure of the effective geometry complexity of a model. It is the average number of vertices processed per triangle, taking into account post-transform vertex caching. In the worst case scenario, an unoptimized model will have an ACMR of 3. A well-optimized, well-behaved model can have an ACMR as low as 0.5, though in practice, any value less than one is excellent.

- `void obj_sort(obj *O, int qc)`

    Sort the triangles of OBJ `O` in an attempt to reduce the model's average cache miss ratio, as rendered using a vertex cache of size `qc`. A sorted model may be written to a file and will remain optimized when subsequently read.

Proper selection of `qc` is crucial. Overestimating the cache size will result in bad performance. It is safe to assume a cache size of 16. Recent video hardware provides cache sizes up to 32. Average-case analysis indicates that future video hardware is unlikely to increase cache size far beyond 32.

Optimal sorting is NP-complete. This implementation is fast (linear in the number of triangles) but not optimal. There is no guarantee that a sorted model will have a lower ACMR than the original unsorted model. Paranoid applications should confirm that sorting reduces the ACMR and reload the model if it does not.

#### Exporting

- `void obj_write(obj *O, const char *obj, const char *mtl, int prec)`

    Write all geometry of OBJ `O` to a file named by `obj`. Write all materials of OBJ `O` to a file named by `mtl`. If either file name argument is `NULL` then the corresponding file is not written. The `prec` argument gives the number of digits of precision to write for each floating point value. This can have a significant impact on the resulting file size.

Note: if geometry is read from one file and written to another then there is no guarantee that the source and destination files are identical. Shared normals and texture coordinates are duplicated per vertex. The number of position, normal, and texture coordinate specifications equal the number of vertices in the file.

All face specifications are of the form "`f i/i/i j/j/j k/k/k`" for some vertex indices `i`, `j`, `k`. All line specifications are of the form "`l i/i j/j`" for some vertex indices `i`, `j`. All vertex indices are positive, counting from the beginning of the file.

Any groups specified in the source OBJ are discarded, and the output OBJ is organized by material. Unused specifications in OBJ and MTL files (curves, merging groups, etc) are omitted. Smoothing groups are omitted, and the smoothed normals for each vertex are inserted instead. All comments are stripped.
