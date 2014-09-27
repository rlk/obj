/* Copyright (c) 2005,2013,2014 Robert Kooima                                 */
/*                                                                            */
/* Permission is hereby granted, free of charge, to any person obtaining a    */
/* copy of this software and associated documentation files (the "Software"), */
/* to deal in the Software without restriction, including without limitation  */
/* the rights to use, copy, modify, merge, publish, distribute, sublicense,   */
/* and/or sell copies of the Software, and to permit persons to whom the      */
/* Software is furnished to do so, subject to the following conditions:       */
/*                                                                            */
/* The above copyright notice and this permission notice shall be included in */
/* all copies or substantial portions of the Software.                       */
/*                                                                            */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR */
/* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   */
/* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    */
/* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER */
/* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    */
/* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        */
/* DEALINGS IN THE SOFTWARE.                                                 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#ifndef CONF_NO_GL
#ifdef __APPLE__
#  include <OpenGL/gl3.h>
#else
#  include <GL/glew.h>
#endif
#endif

#define MAXSTR 1024

/*============================================================================*/

#define index_t       unsigned int
#define GL_INDEX_T GL_UNSIGNED_INT
/*
#define index_t       unsigned short
#define GL_INDEX_T GL_UNSIGNED_SHORT
*/
/*============================================================================*/

#include "obj.h"

struct obj_prop
{
    char        *str;
    int          opt;
    unsigned int map;

    float c[4];
    float o[3];
    float s[3];
};

struct obj_mtrl
{
    char *name;

    struct obj_prop kv[OBJ_PROP_COUNT];
};

struct obj_vert
{
    float u[3];
    float n[3];
    float t[2];
    float v[3];
};

struct obj_poly
{
    index_t vi[3];
};

struct obj_line
{
    index_t vi[2];
};

struct obj_surf
{
    int mi;

    int pc;
    int pm;
    int lc;
    int lm;

    unsigned int pibo;
    unsigned int libo;

    struct obj_poly *pv;
    struct obj_line *lv;
};

struct obj
{
    unsigned int vao;
    unsigned int vbo;

    int mc;
    int mm;
    int vc;
    int vm;
    int sc;
    int sm;

    int uloc;
    int nloc;
    int tloc;
    int vloc;

    int cloc[OBJ_PROP_COUNT];
    int oloc[OBJ_PROP_COUNT];
    int Mloc[OBJ_PROP_COUNT];

    struct obj_mtrl *mv;
    struct obj_vert *vv;
    struct obj_surf *sv;
};

static void invalidate(obj *);

/*----------------------------------------------------------------------------*/

#define assert_surf(O, i) \
      { assert(O); assert(0 <= i && i < O->sc); }
#define assert_vert(O, i) \
      { assert(O); assert(0 <= i && i < O->vc); }
#define assert_mtrl(O, i) \
      { assert(O); assert(0 <= i && i < O->mc); }

#define assert_line(O, i, j) \
      { assert_surf(O, i); assert(0 <= j && j < O->sv[i].lc); }
#define assert_poly(O, i, j) \
      { assert_surf(O, i); assert(0 <= j && j < O->sv[i].pc); }
#define assert_prop(O, i, j) \
      { assert_mtrl(O, i); assert(0 <= j && j < OBJ_PROP_COUNT); }

/*============================================================================*/
/* Vector cache                                                               */

struct vec2
{
    float v[2];
    int _ii;
};

struct vec3
{
    float v[3];
    int _ii;
};

struct iset
{
    int vi;
    int gi;

    int _vi;
    int _ti;
    int _ni;
    int _ii;
};

static int _vc, _vm;
static int _tc, _tm;
static int _nc, _nm;
static int _ic, _im;

static struct vec3 *_vv;
static struct vec2 *_tv;
static struct vec3 *_nv;
static struct iset *_iv;

/*----------------------------------------------------------------------------*/

static int add__(void **_v, int *_c, int *_m, size_t _s)
{
    int   m = (*_m > 0) ? *_m * 2 : 2;
    void *v;

    /* If space remains in the current block, return it. */

    if (*_m > *_c)
        return (*_c)++;

    /* Else, try to increase the size of the block. */

    else if ((v = realloc(*_v, _s * m)))
    {
        *_v = v;
        *_m = m;
        return (*_c)++;
    }

    /* Else, indicate failure. */

    else return -1;
}

static int add_v(void)
{
    return add__((void **) &_vv, &_vc, &_vm, sizeof (struct vec3));
}

static int add_t(void)
{
    return add__((void **) &_tv, &_tc, &_tm, sizeof (struct vec2));
}

static int add_n(void)
{
    return add__((void **) &_nv, &_nc, &_nm, sizeof (struct vec3));
}

static int add_i(void)
{
    return add__((void **) &_iv, &_ic, &_im, sizeof (struct iset));
}

/*============================================================================*/
/* Handy functions                                                            */

static void cross(float *z, const float *x, const float *y)
{
    float t[3];

    t[0] = x[1] * y[2] - x[2] * y[1];
    t[1] = x[2] * y[0] - x[0] * y[2];
    t[2] = x[0] * y[1] - x[1] * y[0];

    z[0] = t[0];
    z[1] = t[1];
    z[2] = t[2];
}

static void normalize(float *v)
{
    float k = 1.0f / (float) sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    v[0] *= k;
    v[1] *= k;
    v[2] *= k;
}

static void normal(float *n, const float *a,
                             const float *b,
                             const float *c)
{
    float u[3];
    float v[3];

    u[0] = b[0] - a[0];
    u[1] = b[1] - a[1];
    u[2] = b[2] - a[2];

    v[0] = c[0] - a[0];
    v[1] = c[1] - a[1];
    v[2] = c[2] - a[2];

    cross(n, u, v);
    normalize(n);
}

/*============================================================================*/

#pragma pack(push, 1)
struct tga_head
{
    unsigned char  id_length;
    unsigned char  color_map_type;
    unsigned char  image_type;
    unsigned short color_map_offset;
    unsigned short color_map_length;
    unsigned char  color_map_size;
    unsigned short image_x_origin;
    unsigned short image_y_origin;
    unsigned short image_width;
    unsigned short image_height;
    unsigned char  image_depth;
    unsigned char  image_descriptor;
};
#pragma pack(pop)

void *read_tga(const char *filename, int *w, int *h, int *d)
{
    struct tga_head head;
    FILE *stream;

    if ((stream = fopen(filename, "rb")))
    {
        if (fread(&head, sizeof (struct tga_head), 1, stream) == 1)
        {
            if (head.image_type == 2)
            {
                *w = (int) head.image_width;
                *h = (int) head.image_height;
                *d = (int) head.image_depth;

                if (fseek(stream, head.id_length, SEEK_CUR) == 0)
                {
                    size_t s = (*d) / 8;
                    size_t n = (*w) * (*h);
                    void *p;

                    if ((p = calloc(n, s)))
                    {
                        if (fread(p, s, n, stream) == n)
                        {
                            fclose(stream);
                            return p;
                        }
                    }
                }
            }
        }
        fclose(stream);
    }
    return 0;
}

unsigned int obj_load_image(const char *filename)
{
    unsigned int o = 0;

#ifndef CONF_NO_GL
    if (filename)
    {
        int   w;
        int   h;
        int   d;
        void *p;

        /* Read the image data from the named file to a new pixel buffer. */

        if ((p = read_tga(filename, &w, &h, &d)))
        {
            /* Create an OpenGL texture object using these pixels. */

            glGenTextures(1, &o);
            glBindTexture(GL_TEXTURE_2D, o);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

            if (d == 32)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                             GL_BGRA, GL_UNSIGNED_BYTE, p);
            if (d == 24)
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,  w, h, 0,
                             GL_BGR,  GL_UNSIGNED_BYTE, p);

            glGenerateMipmap(GL_TEXTURE_2D);

            /* Discard the unnecessary pixel buffer. */

            free(p);
        }
    }
#endif

    return o;
}

static void dirpath(char *pathname)
{
    int i;

    /* Find the path by cutting a file name at the last directory delimiter. */

    for (i = (int) strlen(pathname) - 1; i >= 0; --i)
        if (pathname[i] == '/' || pathname[i] == '\\')
        {
            pathname[i] = '\0';
            return;
        }

    /* If no delimiter was found, return the current directory. */

    strcpy(pathname, ".");
}

/*----------------------------------------------------------------------------*/

static void read_image(obj *O, int mi, int ki, const char *line,
                                               const char *path)
{
    unsigned int clamp  = 0;

    float o[3] = { 0.0f, 0.0f, 0.0f };
    float s[3] = { 1.0f, 1.0f, 1.0f };

    char pathname[MAXSTR];

    char map[MAXSTR];
    char val[MAXSTR];

    const char *end;

    memset(map, 0, MAXSTR);
    memset(val, 0, MAXSTR);

    while (line[0] != '\0' && line[0] != '\r' && line[0] != '\n')
    {
        int n = 0;

        while(isspace(line[0])) line++;

        /* Parse property map options. */

        if (sscanf(line, "-clamp %s%n", val, &n) >= 1)
        {
            clamp  = (strcmp(val, "on") == 0) ? OBJ_OPT_CLAMP : 0;
            line  += n;
        }

        /* Parse property map scale. */

        else if (sscanf(line, "-s %f %f %f%n", s + 0, s + 1, s + 2, &n) >= 3)
            line += n;
        else if (sscanf(line, "-s %f %f%n",    s + 0, s + 1,        &n) >= 2)
            line += n;
        else if (sscanf(line, "-s %f%n",       s + 0,               &n) >= 1)
            line += n;

        /* Parse property map offset. */

        else if (sscanf(line, "-o %f %f %f%n", o + 0, o + 1, o + 2, &n) >= 3)
            line += n;
        else if (sscanf(line, "-o %f %f%n",    o + 0, o + 1,        &n) >= 2)
            line += n;
        else if (sscanf(line, "-o %f%n",       o + 0,               &n) >= 1)
            line += n;

        /* Check for a file name */

        else if ((end = strstr(line, ".tga"))) { strncpy(map, line, end - line + 4); break; }
        else if ((end = strstr(line, ".TGA"))) { strncpy(map, line, end - line + 4); break; }

        /* If we see something we don't recognize, stop looking. */

        else break;
    }

    /* Apply all parsed property attributes to the material. */

    sprintf(pathname, "%s/%s", path, map);

    obj_set_mtrl_opt(O, mi, ki, clamp);
    obj_set_mtrl_map(O, mi, ki, pathname);
    obj_set_mtrl_o  (O, mi, ki, o);
    obj_set_mtrl_s  (O, mi, ki, s);
}

static void read_color(obj *O, int mi, int ki, const char *line)
{
    float c[4];

    /* Merge incoming color components with existing defaults. */

    obj_get_mtrl_c(O, mi, ki, c);
    sscanf(line, "%f %f %f", c, c + 1, c + 2);
    obj_set_mtrl_c(O, mi, ki, c);
}

static void read_alpha(obj *O, int mi, int ki, const char *line)
{
    float c[4];
    float t;

    /* Merge incoming color components with existing defaults. */

    obj_get_mtrl_c(O, mi, ki, c);
    sscanf(line, "%f", &t);
    c[3] = 1.0 - t;
    obj_set_mtrl_c(O, mi, ki, c);
}

static void read_mtl(const char *path,
                     const char *file,
                     const char *name, obj *O, int mi)
{
    char pathname[MAXSTR];

    char buf[MAXSTR];
    char key[MAXSTR];
    char arg[MAXSTR];

    FILE *fin;

    int scanning = 1;
    int n        = 0;

    sprintf(pathname, "%s/%s", path, file);

    if ((fin = fopen(pathname, "r")))
    {
        /* Process each line of the MTL file. */

        while  (fgets (buf, MAXSTR, fin))
            if (sscanf(buf, "%s%n", key, &n) >= 1)
            {
                const char *c = buf + n;

                if (scanning)
                {
                    /* Determine if we've found the MTL we're looking for. */

                    if (!strcmp(key, "newmtl"))
                    {
                        sscanf(c, "%s", arg);

                        if ((scanning = strcmp(arg, name)) == 0)
                            obj_set_mtrl_name(O, mi, name);
                    }
                }
                else
                {
                    /* Stop scanning when the next MTL begins. */

                    if (!strcmp(key, "newmtl"))
                        break;

                    /* Parse this material's properties. */

                    else if (!strcmp(key, "map_Kd"))
                        read_image(O, mi, OBJ_KD, c, path);
                    else if (!strcmp(key, "map_Ka"))
                        read_image(O, mi, OBJ_KA, c, path);
                    else if (!strcmp(key, "map_Ke"))
                        read_image(O, mi, OBJ_KE, c, path);
                    else if (!strcmp(key, "map_Ks"))
                        read_image(O, mi, OBJ_KS, c, path);
                    else if (!strcmp(key, "map_Ns"))
                        read_image(O, mi, OBJ_NS, c, path);
                    else if (!strcmp(key, "map_Kn"))
                        read_image(O, mi, OBJ_KN, c, path);

                    else if (!strcmp(key, "Kd"))
                        read_color(O, mi, OBJ_KD, c);
                    else if (!strcmp(key, "Ka"))
                        read_color(O, mi, OBJ_KA, c);
                    else if (!strcmp(key, "Ke"))
                        read_color(O, mi, OBJ_KE, c);
                    else if (!strcmp(key, "Ks"))
                        read_color(O, mi, OBJ_KS, c);
                    else if (!strcmp(key, "Ns"))
                        read_color(O, mi, OBJ_NS, c);

                    else if (!strcmp(key, "d"))
                        read_alpha(O, mi, OBJ_KD, c);
                    else if (!strcmp(key, "Tr"))
                        read_alpha(O, mi, OBJ_KD, c);
                }
            }
        fclose(fin);
    }
}

static void read_mtllib(char *file, const char *line)
{
    /* Parse the first file name from the given line. */

    sscanf(line, "%s", file);
}

static int read_usemtl(const char *path,
                       const char *file,
                       const char *line, obj *O)
{
    char name[MAXSTR];

    int si;
    int mi;

    sscanf(line, "%s", name);

    /* Create a new material for the incoming definition. */

    if ((mi = obj_add_mtrl(O)) >= 0)
    {
        /* Create a new surface to contain geometry with the new material. */

        if ((si = obj_add_surf(O)) >= 0)
        {
            /* Read the material definition and apply it to the new surface. */

            read_mtl(path, file, name, O, mi);
            obj_set_surf(O, si, mi);

            /* Return the surface so that new geometry may be added to it. */

            return si;
        }
    }

    /* On failure, return the default surface. */

    return 0;
}

/*----------------------------------------------------------------------------*/

static int read_poly_indices(const char *line, int *_vi, int *_ti, int *_ni)
{
    int n;

    *_vi = 0;
    *_ti = 0;
    *_ni = 0;

    /* Parse a face vertex specification from the given line. */

    if (sscanf(line, "%d/%d/%d%n", _vi, _ti, _ni, &n) >= 3) return n;
    if (sscanf(line, "%d/%d%n",    _vi, _ti,      &n) >= 2) return n;
    if (sscanf(line, "%d//%d%n",   _vi,      _ni, &n) >= 2) return n;
    if (sscanf(line, "%d%n",       _vi,           &n) >= 1) return n;

    return 0;
}

static int read_poly_vertices(const char *line, obj *O, int gi)
{
    const char *c = line;

    int _vi;
    int _ti;
    int _ni;
    int _ii;
    int _ij;

    int  dc;
    int  vi;
    int  ic = 0;

    /* Scan the face string, converting index sets to vertices. */

    while ((dc = read_poly_indices(c, &_vi, &_ti, &_ni)))
    {
        /* Convert face indices to vector cache indices. */

        _vi += (_vi < 0) ? _vc : -1;
        _ti += (_ti < 0) ? _tc : -1;
        _ni += (_ni < 0) ? _nc : -1;

        /* Initialize a new index set. */

        if ((_ii = add_i()) >= 0)
        {
            _iv[_ii]._vi = _vi;
            _iv[_ii]._ni = _ni;
            _iv[_ii]._ti = _ti;

            /* Search the vector reference list for a repeated index set. */

            for (_ij = _vv[_vi]._ii; _ij >= 0; _ij = _iv[_ij]._ii)
                if (_iv[_ij]._vi == _vi &&
                    _iv[_ij]._ti == _ti &&
                    _iv[_ij]._ni == _ni &&
                    _iv[_ij]. gi ==  gi)
                {
                    /* A repeat has been found. Link new to old. */

                    _vv[_vi]._ii = _ii;
                    _iv[_ii]._ii = _ij;
                    _iv[_ii]. vi = _iv[_ij].vi;
                    _iv[_ii]. gi = _iv[_ij].gi;

                    break;
                }

            /* If no repeat was found, add a new vertex. */

            if ((_ij < 0) && (vi = obj_add_vert(O)) >= 0)
            {
                _vv[_vi]._ii = _ii;
                _iv[_ii]._ii =  -1;
                _iv[_ii]. vi =  vi;
                _iv[_ii]. gi =  gi;

                /* Initialize the new vertex using valid cache references. */

                if (0 <= _vi && _vi < _vc) obj_set_vert_v(O, vi, _vv[_vi].v);
                if (0 <= _ni && _ni < _nc) obj_set_vert_n(O, vi, _nv[_ni].v);
                if (0 <= _ti && _ti < _tc) obj_set_vert_t(O, vi, _tv[_ti].v);
            }
            ic++;
        }
        c  += dc;
    }
    return ic;
}

static void read_f(const char *line, obj *O, int si, int gi)
{
    float n[3];
    float t[3];
    int i, pi;

    /* Create new vertex references for this face. */

    int i0 = _ic;
    int ic = read_poly_vertices(line, O, gi);

    /* If smoothing, apply this face's normal to vertices that need it. */

    if (gi)
    {
        normal(n, _vv[_iv[i0 + 0]._vi].v,
                  _vv[_iv[i0 + 1]._vi].v,
                  _vv[_iv[i0 + 2]._vi].v);

        for (i = 0; i < ic; ++i)
            if (_iv[i0 + 0]._ni < 0)
            {
                obj_get_vert_n(O, _iv[i0 + i]._vi, t);
                t[0] += n[0];
                t[1] += n[1];
                t[2] += n[2];
                obj_set_vert_n(O, _iv[i0 + i]._vi, t);
            }
    }

    /* Convert our N new vertex references into N-2 new triangles. */

    for (i = 0; i < ic - 2; ++i)

        if ((pi = obj_add_poly(O, si)) >= 0)
        {
            int vi[3];

            vi[0] = _iv[i0        ].vi;
            vi[1] = _iv[i0 + i + 1].vi;
            vi[2] = _iv[i0 + i + 2].vi;

            obj_set_poly(O, si, pi, vi);
        }
}

/*----------------------------------------------------------------------------*/

static int read_line_indices(const char *line, int *_vi, int *_ti)
{
    int n;

    *_vi = 0;
    *_ti = 0;

    /* Parse a line vertex specification from the given line. */

    if (sscanf(line, "%d/%d%n", _vi, _ti, &n) >= 2) return n;
    if (sscanf(line, "%d%n",    _vi,      &n) >= 1) return n;

    return 0;
}

static int read_line_vertices(const char *line, obj *O)
{
    const char *c = line;

    int _vi;
    int _ti;
    int _ii;
    int _ij;

    int  dc;
    int  vi;
    int  ic = 0;

    /* Scan the line string, converting index sets to vertices. */

    while ((dc = read_line_indices(c, &_vi, &_ti)))
    {
        /* Convert line indices to vector cache indices. */

        _vi += (_vi < 0) ? _vc : -1;
        _ti += (_ti < 0) ? _tc : -1;

        /* Initialize a new index set. */

        if ((_ii = add_i()) >= 0)
        {
            _iv[_ii]._vi = _vi;
            _iv[_ii]._ti = _ti;

            /* Search the vector reference list for a repeated index set. */

            for (_ij = _vv[_vi]._ii; _ij >= 0; _ij = _iv[_ij]._ii)
                if (_iv[_ij]._vi == _vi &&
                    _iv[_ij]._ti == _ti)
                {
                    /* A repeat has been found. Link new to old. */

                    _vv[_vi]._ii = _ii;
                    _iv[_ii]._ii = _ij;
                    _iv[_ii]. vi = _iv[_ij].vi;

                    break;
                }

            /* If no repeat was found, add a new vertex. */

            if ((_ij < 0) && (vi = obj_add_vert(O)) >= 0)
            {
                _vv[_vi]._ii = _ii;
                _iv[_ii]._ii =  -1;
                _iv[_ii]. vi =  vi;

                /* Initialize the new vertex using valid cache references. */

                if (0 <= _vi && _vi < _vc) obj_set_vert_v(O, vi, _vv[_vi].v);
                if (0 <= _ti && _ti < _tc) obj_set_vert_t(O, vi, _tv[_ti].v);
            }
            ic++;
        }
        c  += dc;
    }
    return ic;
}

static void read_l(const char *line, obj *O, int si)
{
    int i, li;

    /* Create new vertices for this line. */

    int i0 = _ic;
    int ic = read_line_vertices(line, O);

    /* Convert our N new vertices into N-1 new lines. */

    for (i = 0; i < ic - 1; ++i)

        if ((li = obj_add_line(O, si)) >= 0)
        {
            int vi[2];

            vi[0] = _iv[i0 + i    ].vi;
            vi[1] = _iv[i0 + i + 1].vi;

            obj_set_line(O, si, li, vi);
        }
}

/*----------------------------------------------------------------------------*/

static void read_v(const char *line)
{
    int _vi;

    /* Parse a vertex position. */

    if ((_vi = add_v()) >= 0)
    {
        sscanf(line, "%f %f %f", _vv[_vi].v + 0,
                                 _vv[_vi].v + 1,
                                 _vv[_vi].v + 2);
        _vv[_vi]._ii = -1;
    }
}

static void read_vt(const char *line)
{
    int _ti;

    /* Parse a texture coordinate. */

    if ((_ti = add_t()) >= 0)
    {
        sscanf(line, "%f %f", _tv[_ti].v + 0,
                              _tv[_ti].v + 1);
        _tv[_ti]._ii = -1;
    }
}

static void read_vn(const char *line)
{
    int _ni;

    /* Parse a normal. */

    if ((_ni = add_n()) >= 0)
    {
        sscanf(line, "%f %f %f", _nv[_ni].v + 0,
                                 _nv[_ni].v + 1,
                                 _nv[_ni].v + 2);
        _nv[_ni]._ii = -1;
    }
}

/*----------------------------------------------------------------------------*/

static void read_obj(obj *O, const char *filename)
{
    char buf[MAXSTR];
    char key[MAXSTR];

    char L[MAXSTR];
    char D[MAXSTR];

    FILE *fin;

    /* Flush the vector caches. */

    _vc = 0;
    _tc = 0;
    _nc = 0;
    _ic = 0;

    /* Add the named file to the given object. */

    if ((fin = fopen(filename, "r")))
    {
        /* Ensure there exists a default surface 0 and default material 0. */

        int si = obj_add_surf(O);
        int mi = obj_add_mtrl(O);
        int gi = 0;
        int n;

        obj_set_surf(O, si, mi);

        /* Extract the directory from the filename for use in MTL loading. */

        strncpy(D, filename, MAXSTR);
        dirpath(D);

        /* Process each line of the OBJ file, invoking the handler for each. */

        while  (fgets (buf, MAXSTR, fin))
            if (sscanf(buf, "%s%n", key, &n) >= 1)
            {
                const char *c = buf + n;

                if      (!strcmp(key, "f" )) read_f (c, O, si, gi);
                else if (!strcmp(key, "l" )) read_l (c, O, si);
                else if (!strcmp(key, "vt")) read_vt(c);
                else if (!strcmp(key, "vn")) read_vn(c);
                else if (!strcmp(key, "v" )) read_v (c);

                else if (!strcmp(key, "mtllib"))      read_mtllib(   L, c   );
                else if (!strcmp(key, "usemtl")) si = read_usemtl(D, L, c, O);
                else if (!strcmp(key, "s"     )) gi = atoi(c);
            }

        fclose(fin);
    }
}

/*----------------------------------------------------------------------------*/

static void obj_rel_mtrl(struct obj_mtrl *mp)
{
    /* Release any resources held by this material. */

    int ki;

    for (ki = 0; ki < OBJ_PROP_COUNT; ki++)
    {
        if (mp->kv[ki].str) free(mp->kv[ki].str);
#ifndef CONF_NO_GL
        if (mp->kv[ki].map) glDeleteTextures(1, &mp->kv[ki].map);
#endif
    }
}

static void obj_rel_surf(struct obj_surf *sp)
{
#ifndef CONF_NO_GL
    if (sp->pibo) glDeleteBuffers(1, &sp->pibo);
    if (sp->libo) glDeleteBuffers(1, &sp->libo);
#endif

    sp->pibo = 0;
    sp->libo = 0;

    /* Release this surface's polygon and line vectors. */

    if (sp->pv) free(sp->pv);
    if (sp->lv) free(sp->lv);
}

static void obj_rel(obj *O)
{
    int si;
    int mi;

    /* Release resources held by this file and it's materials and surfaces. */

#ifndef CONF_NO_GL
    if (O->vbo) glDeleteBuffers     (1, &O->vbo);
    if (O->vao) glDeleteVertexArrays(1, &O->vao);
#endif

    O->vbo = 0;

    for (mi = 0; mi < O->mc; ++mi) obj_rel_mtrl(O->mv + mi);
    for (si = 0; si < O->sc; ++si) obj_rel_surf(O->sv + si);
}

/*============================================================================*/

obj *obj_create(const char *filename)
{
    obj *O;
    int  i;

    /* Allocate and initialize a new file. */

    if ((O = (obj *) calloc(1, sizeof (obj))))
    {
        if (filename)
        {
            /* Read the named file. */

            read_obj(O, filename);

            /* Post-process the loaded object. */

            obj_mini(O);
            obj_proc(O);
        }

        /* Set default shader locations. */

        for (i = 0; i < OBJ_PROP_COUNT; i++)
        {
            O->cloc[i] = -1;
            O->oloc[i] = -1;
            O->Mloc[i] = -1;
        }
        O->uloc = -1;
        O->nloc = -1;
        O->tloc = -1;
        O->vloc = -1;
    }
    return O;
}

void obj_delete(obj *O)
{
    assert(O);

    obj_rel(O);

    free(O);
}

/*----------------------------------------------------------------------------*/

int obj_add_mtrl(obj *O)
{
    unsigned int opt = 0;

    const float Kd[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
    const float Ka[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
    const float Ke[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float Ks[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    const float Ns[4] = { 8.0f, 0.0f, 0.0f, 0.0f };
    const float  s[3] = { 1.0f, 1.0f, 1.0f       };

    int mi;

    assert(O);

    /* Allocate and initialize a new material. */

    if ((mi = add__((void **) &O->mv,
                              &O->mc,
                              &O->mm, sizeof (struct obj_mtrl))) >= 0)
    {
        memset(O->mv + mi, 0, sizeof (struct obj_mtrl));

        obj_set_mtrl_opt(O, mi, OBJ_KD, opt);
        obj_set_mtrl_opt(O, mi, OBJ_KA, opt);
        obj_set_mtrl_opt(O, mi, OBJ_KE, opt);
        obj_set_mtrl_opt(O, mi, OBJ_KS, opt);
        obj_set_mtrl_opt(O, mi, OBJ_NS, opt);
        obj_set_mtrl_opt(O, mi, OBJ_KN, opt);

        obj_set_mtrl_c  (O, mi, OBJ_KD, Kd);
        obj_set_mtrl_c  (O, mi, OBJ_KA, Ka);
        obj_set_mtrl_c  (O, mi, OBJ_KE, Ke);
        obj_set_mtrl_c  (O, mi, OBJ_KS, Ks);
        obj_set_mtrl_c  (O, mi, OBJ_NS, Ns);

        obj_set_mtrl_s  (O, mi, OBJ_KD, s);
        obj_set_mtrl_s  (O, mi, OBJ_KA, s);
        obj_set_mtrl_s  (O, mi, OBJ_KE, s);
        obj_set_mtrl_s  (O, mi, OBJ_KS, s);
        obj_set_mtrl_s  (O, mi, OBJ_NS, s);
        obj_set_mtrl_s  (O, mi, OBJ_KN, s);
    }
    return mi;
}

int obj_add_vert(obj *O)
{
    int vi;

    assert(O);

    /* Allocate and initialize a new vertex. */

    if ((vi = add__((void **) &O->vv,
                              &O->vc,
                              &O->vm, sizeof (struct obj_vert))) >= 0)

        memset(O->vv + vi, 0, sizeof (struct obj_vert));

    return vi;
}

int obj_add_poly(obj *O, int si)
{
    int pi;

    assert_surf(O, si);

    /* Allocate and initialize a new polygon. */

    if ((pi = add__((void **) &O->sv[si].pv,
                              &O->sv[si].pc,
                              &O->sv[si].pm, sizeof (struct obj_poly)))>=0)

        memset(O->sv[si].pv + pi, 0, sizeof (struct obj_poly));

    return pi;
}

int obj_add_line(obj *O, int si)
{
    int li;

    assert_surf(O, si);

    /* Allocate and initialize a new line. */

    if ((li = add__((void **) &O->sv[si].lv,
                              &O->sv[si].lc,
                              &O->sv[si].lm, sizeof (struct obj_line)))>=0)

        memset(O->sv[si].lv + li, 0, sizeof (struct obj_line));

    return li;
}

int obj_add_surf(obj *O)
{
    int si;

    assert(O);

    /* Allocate and initialize a new surface. */

    if ((si = add__((void **) &O->sv,
                              &O->sc,
                              &O->sm, sizeof (struct obj_surf))) >= 0)

        memset(O->sv + si, 0, sizeof (struct obj_surf));

    return si;
}

/*----------------------------------------------------------------------------*/

int obj_num_mtrl(const obj *O)
{
    assert(O);
    return O->mc;
}

int obj_num_vert(const obj *O)
{
    assert(O);
    return O->vc;
}

int obj_num_poly(const obj *O, int si)
{
    assert_surf(O, si);
    return O->sv[si].pc;
}

int obj_num_line(const obj *O, int si)
{
    assert_surf(O, si);
    return O->sv[si].lc;
}

int obj_num_surf(const obj *O)
{
    assert(O);
    return O->sc;
}


/*----------------------------------------------------------------------------*/

void obj_del_mtrl(obj *O, int mi)
{
    int si;

    assert_mtrl(O, mi);

    /* Remove this material from the material vector. */

    obj_rel_mtrl(O->mv + mi);

    memmove(O->mv + mi,
            O->mv + mi + 1,
           (O->mc - mi - 1) * sizeof (struct obj_mtrl));

    O->mc--;

    /* Remove all references to this material. */

    for (si = O->sc - 1; si >= 0; --si)
    {
        struct obj_surf *sp = O->sv + si;

        if (sp->mi == mi)
            obj_del_surf(O, si);
        else
            if (sp->mi > mi)
                sp->mi--;
    }
}

void obj_del_vert(obj *O, int vi)
{
    int si;
    int pi;
    int li;

    assert_vert(O, vi);

    /* Remove this vertex from the file's vertex vector. */

    memmove(O->vv + vi,
            O->vv + vi + 1,
           (O->vc - vi - 1) * sizeof (struct obj_vert));

    O->vc--;

    /* Remove all references to this vertex from all surfaces. */

    for (si = 0; si < O->sc; ++si)
    {
        /* Delete all referencing polygons. Decrement later references. */

        for (pi = O->sv[si].pc - 1; pi >= 0; --pi)
        {
            struct obj_poly *pp = O->sv[si].pv + pi;

            if (pp->vi[0] == vi || pp->vi[1] == vi || pp->vi[2] == vi)
                obj_del_poly(O, si, pi);
            else
            {
                if (pp->vi[0] > vi) pp->vi[0]--;
                if (pp->vi[1] > vi) pp->vi[1]--;
                if (pp->vi[2] > vi) pp->vi[2]--;
            }
        }

        /* Delete all referencing lines. Decrement later references. */

        for (li = O->sv[si].lc - 1; li >= 0; --li)
        {
            struct obj_line *lp = O->sv[si].lv + li;

            if (lp->vi[0] == vi || lp->vi[1] == vi)
                obj_del_line(O, si, li);
            else
            {
                if (lp->vi[0] > vi) lp->vi[0]--;
                if (lp->vi[1] > vi) lp->vi[1]--;
            }
        }
    }

    /* Schedule the VBO for refresh. */

    invalidate(O);
}

void obj_del_poly(obj *O, int si, int pi)
{
    assert_poly(O, si, pi);

    /* Remove this polygon from the surface's polygon vector. */

    memmove(O->sv[si].pv + pi,
            O->sv[si].pv + pi + 1,
           (O->sv[si].pc - pi - 1) * sizeof (struct obj_poly));

    O->sv[si].pc--;
}

void obj_del_line(obj *O, int si, int li)
{
    assert_line(O, si, li);

    /* Remove this line from the surface's line vector. */

    memmove(O->sv[si].lv + li,
            O->sv[si].lv + li + 1,
           (O->sv[si].lc - li - 1) * sizeof (struct obj_line));

    O->sv[si].lc--;
}

void obj_del_surf(obj *O, int si)
{
    assert_surf(O, si);

    /* Remove this surface from the file's surface vector. */

    obj_rel_surf(O->sv + si);

    memmove(O->sv + si,
            O->sv + si + 1,
           (O->sc - si - 1) * sizeof (struct obj_surf));

    O->sc--;
}

/*----------------------------------------------------------------------------*/

static char *set_name(char *old, const char *src)
{
    char *dst = NULL;

    if (old)
        free(old);

    if (src && (dst = (char *) malloc(strlen(src) + 1)))
        strcpy(dst, src);

    return dst;
}

void obj_set_mtrl_name(obj *O, int mi, const char *name)
{
    assert_mtrl(O, mi);
    O->mv[mi].name = set_name(O->mv[mi].name, name);
}

void obj_set_mtrl_map(obj *O, int mi, int ki, const char *str)
{
    assert_prop(O, mi, ki);

#ifndef CONF_NO_GL
    if (O->mv[mi].kv[ki].map)
        glDeleteTextures(1, &O->mv[mi].kv[ki].map);
#endif

    O->mv[mi].kv[ki].map = obj_load_image(str);
    O->mv[mi].kv[ki].str = set_name(O->mv[mi].kv[ki].str, str);
}

void obj_set_mtrl_opt(obj *O, int mi, int ki, unsigned int opt)
{
    assert_prop(O, mi, ki);

    O->mv[mi].kv[ki].opt = opt;
}

void obj_set_mtrl_c(obj *O, int mi, int ki, const float c[4])
{
    assert_prop(O, mi, ki);

    O->mv[mi].kv[ki].c[0] = c[0];
    O->mv[mi].kv[ki].c[1] = c[1];
    O->mv[mi].kv[ki].c[2] = c[2];
    O->mv[mi].kv[ki].c[3] = c[3];
}

void obj_set_mtrl_s(obj *O, int mi, int ki, const float s[3])
{
    assert_prop(O, mi, ki);

    O->mv[mi].kv[ki].s[0] = s[0];
    O->mv[mi].kv[ki].s[1] = s[1];
    O->mv[mi].kv[ki].s[2] = s[2];
}

void obj_set_mtrl_o(obj *O, int mi, int ki, const float o[3])
{
    assert_prop(O, mi, ki);

    O->mv[mi].kv[ki].o[0] = o[0];
    O->mv[mi].kv[ki].o[1] = o[1];
    O->mv[mi].kv[ki].o[2] = o[2];
}

/*----------------------------------------------------------------------------*/

static void invalidate(obj *O)
{
#ifndef CONF_NO_GL
    if (O->vbo) glDeleteBuffers     (1, &O->vbo);
    if (O->vao) glDeleteVertexArrays(1, &O->vao);
#endif
    O->vbo = 0;
    O->vao = 0;
}

void obj_set_vert_v(obj *O, int vi, const float v[3])
{
    assert_vert(O, vi);

    O->vv[vi].v[0] = v[0];
    O->vv[vi].v[1] = v[1];
    O->vv[vi].v[2] = v[2];

    invalidate(O);
}

void obj_set_vert_t(obj *O, int vi, const float t[2])
{
    assert_vert(O, vi);

    O->vv[vi].t[0] = t[0];
    O->vv[vi].t[1] = t[1];

    invalidate(O);
}

void obj_set_vert_n(obj *O, int vi, const float n[3])
{
    assert_vert(O, vi);

    O->vv[vi].n[0] = n[0];
    O->vv[vi].n[1] = n[1];
    O->vv[vi].n[2] = n[2];

    invalidate(O);
}

void obj_set_vert_u(obj *O, int vi, const float u[3])
{
    assert_vert(O, vi);

    O->vv[vi].u[0] = u[0];
    O->vv[vi].u[1] = u[1];
    O->vv[vi].u[2] = u[2];

    invalidate(O);
}

/*----------------------------------------------------------------------------*/

void obj_set_poly(obj *O, int si, int pi, const int vi[3])
{
    assert_poly(O, si, pi);

    O->sv[si].pv[pi].vi[0] = (index_t) vi[0];
    O->sv[si].pv[pi].vi[1] = (index_t) vi[1];
    O->sv[si].pv[pi].vi[2] = (index_t) vi[2];
}

void obj_set_line(obj *O, int si, int li, const int vi[2])
{
    assert_line(O, si, li);

    O->sv[si].lv[li].vi[0] = (index_t) vi[0];
    O->sv[si].lv[li].vi[1] = (index_t) vi[1];
}

void obj_set_surf(obj *O, int si, int mi)
{
    assert_surf(O, si);

    O->sv[si].mi = mi;
}

/*----------------------------------------------------------------------------*/

void obj_set_vert_loc(obj *O, int u, int n, int t, int v)
{
    assert(O);

    O->uloc = u;
    O->nloc = n;
    O->tloc = t;
    O->vloc = v;

    invalidate(O);
}

void obj_set_prop_loc(obj *O, int ki, int c, int o, int M)
{
    assert(O);
    assert(0 <= ki && ki < OBJ_PROP_COUNT);

    O->cloc[ki] = c;
    O->oloc[ki] = o;
    O->Mloc[ki] = M;
}

/*============================================================================*/

const char *obj_get_mtrl_name(const obj *O, int mi)
{
    assert_mtrl(O, mi);
    return O->mv[mi].name;
}

unsigned int obj_get_mtrl_map(const obj *O, int mi, int ki)
{
    assert_prop(O, mi, ki);
    return O->mv[mi].kv[ki].map;
}

unsigned int obj_get_mtrl_opt(const obj *O, int mi, int ki)
{
    assert_prop(O, mi, ki);
    return O->mv[mi].kv[ki].opt;
}

void obj_get_mtrl_c(const obj *O, int mi, int ki, float *c)
{
    assert_prop(O, mi, ki);

    c[0] = O->mv[mi].kv[ki].c[0];
    c[1] = O->mv[mi].kv[ki].c[1];
    c[2] = O->mv[mi].kv[ki].c[2];
    c[3] = O->mv[mi].kv[ki].c[3];
}

void obj_get_mtrl_s(const obj *O, int mi, int ki, float *s)
{
    assert_prop(O, mi, ki);

    s[0] = O->mv[mi].kv[ki].s[0];
    s[1] = O->mv[mi].kv[ki].s[1];
    s[2] = O->mv[mi].kv[ki].s[2];
}

void obj_get_mtrl_o(const obj *O, int mi, int ki, float *o)
{
    assert_prop(O, mi, ki);

    o[0] = O->mv[mi].kv[ki].o[0];
    o[1] = O->mv[mi].kv[ki].o[1];
    o[2] = O->mv[mi].kv[ki].o[2];
}

/*----------------------------------------------------------------------------*/

void obj_get_vert_v(const obj *O, int vi, float *v)
{
    assert_vert(O, vi);

    v[0] = O->vv[vi].v[0];
    v[1] = O->vv[vi].v[1];
    v[2] = O->vv[vi].v[2];
}

void obj_get_vert_t(const obj *O, int vi, float *t)
{
    assert_vert(O, vi);

    t[0] = O->vv[vi].t[0];
    t[1] = O->vv[vi].t[1];
}

void obj_get_vert_n(const obj *O, int vi, float *n)
{
    assert_vert(O, vi);

    n[0] = O->vv[vi].n[0];
    n[1] = O->vv[vi].n[1];
    n[2] = O->vv[vi].n[2];
}

/*----------------------------------------------------------------------------*/

void obj_get_poly(const obj *O, int si, int pi, int *vi)
{
    assert_poly(O, si, pi);

    vi[0] = (int) O->sv[si].pv[pi].vi[0];
    vi[1] = (int) O->sv[si].pv[pi].vi[1];
    vi[2] = (int) O->sv[si].pv[pi].vi[2];
}

void obj_get_line(const obj *O, int si, int li, int *vi)
{
    assert_line(O, si, li);

    vi[0] = (int) O->sv[si].lv[li].vi[0];
    vi[1] = (int) O->sv[si].lv[li].vi[1];
}

int obj_get_surf(const obj *O, int si)
{
    assert_surf(O, si);
    return O->sv[si].mi;
}

/*============================================================================*/

void obj_mini(obj *O)
{
    int si;
    int mi;

    /* Remove empty surfaces. */

    for (si = O->sc - 1; si >= 0; --si)
        if (O->sv[si].pc == 0 &&
            O->sv[si].lc == 0)
            obj_del_surf(O, si);

    /* Remove unreferenced materials. */

    for (mi = O->mc - 1; mi >= 0; --mi)
    {
        int cc = 0;

        for (si = 0; si < O->sc; ++si)
            if (O->sv[si].mi == mi)
                cc++;

        if (cc == 0)
            obj_del_mtrl(O, mi);
    }
}

void obj_norm(obj *O)
{
    int vi;
    int si;
    int pi;

    assert(O);

    /* Zero the normals for all vertices. */

    for (vi = 0; vi < O->vc; ++vi)
    {
        O->vv[vi].n[0] = 0;
        O->vv[vi].n[1] = 0;
        O->vv[vi].n[2] = 0;
    }

    /* Compute normals for all faces. */

    for (si = 0; si < O->sc; ++si)
        for (pi = 0; pi < O->sv[si].pc; ++pi)
        {
            struct obj_vert *v0 = O->vv + O->sv[si].pv[pi].vi[0];
            struct obj_vert *v1 = O->vv + O->sv[si].pv[pi].vi[1];
            struct obj_vert *v2 = O->vv + O->sv[si].pv[pi].vi[2];

            float n[3];

            /* Compute the normal formed by these 3 vertices. */

            normal(n, v0->v, v1->v, v2->v);

            /* Sum this normal to all vertices. */

            v0->n[0] += n[0];
            v0->n[1] += n[1];
            v0->n[2] += n[2];

            v1->n[0] += n[0];
            v1->n[1] += n[1];
            v1->n[2] += n[2];

            v2->n[0] += n[0];
            v2->n[1] += n[1];
            v2->n[2] += n[2];
        }
}

void obj_proc(obj *O)
{
    int si;
    int sj;
    int pi;
    int vi;

    assert(O);

    /* Normalize all normals. Zero all tangent vectors. */

    for (vi = 0; vi < O->vc; ++vi)
    {
        normalize(O->vv[vi].n);

        O->vv[vi].u[0] = 0.0f;
        O->vv[vi].u[1] = 0.0f;
        O->vv[vi].u[2] = 0.0f;
    }

    /* Compute tangent vectors for all vertices. */

    for (si = 0; si < O->sc; ++si)
        for (pi = 0; pi < O->sv[si].pc; ++pi)
        {
            struct obj_vert *v0 = O->vv + O->sv[si].pv[pi].vi[0];
            struct obj_vert *v1 = O->vv + O->sv[si].pv[pi].vi[1];
            struct obj_vert *v2 = O->vv + O->sv[si].pv[pi].vi[2];

            float dt1, dv1[3];
            float dt2, dv2[3];

            float u[3];

            /* Compute the tangent vector for this polygon. */

            dv1[0] = v1->v[0] - v0->v[0];
            dv1[1] = v1->v[1] - v0->v[1];
            dv1[2] = v1->v[2] - v0->v[2];

            dv2[0] = v2->v[0] - v0->v[0];
            dv2[1] = v2->v[1] - v0->v[1];
            dv2[2] = v2->v[2] - v0->v[2];

            dt1    = v1->t[1] - v0->t[1];
            dt2    = v2->t[1] - v0->t[1];

            u[0]   = dt2 * dv1[0] - dt1 * dv2[0];
            u[1]   = dt2 * dv1[1] - dt1 * dv2[1];
            u[2]   = dt2 * dv1[2] - dt1 * dv2[2];

            normalize(u);

            /* Accumulate the tangent vectors for this polygon's vertices. */

            v0->u[0] += u[0];  v0->u[1] += u[1];  v0->u[2] += u[2];
            v1->u[0] += u[0];  v1->u[1] += u[1];  v1->u[2] += u[2];
            v2->u[0] += u[0];  v2->u[1] += u[1];  v2->u[2] += u[2];
        }

    /* Orthonormalize each tangent basis. */

    for (vi = 0; vi < O->vc; ++vi)
    {
        float *n = O->vv[vi].n;
        float *u = O->vv[vi].u;

        float v[3];

        cross(v, n, u);
        cross(u, v, n);
        normalize(u);
    }

    /* Sort surfaces such that transparent ones appear later. */

    for (si = 0; si < O->sc; ++si)
        for (sj = si + 1; sj < O->sc; ++sj)
            if (O->mv[O->sv[si].mi].kv[OBJ_KD].c[3] <
                O->mv[O->sv[sj].mi].kv[OBJ_KD].c[3])
            {
                struct obj_surf temp;

                temp      = O->sv[si];
                O->sv[si] = O->sv[sj];
                O->sv[sj] = temp;
            }
}

void obj_init(obj *O)
{
#ifndef CONF_NO_GL
    if (O->vao == 0)
    {
        const size_t vs = sizeof (struct obj_vert);
        const size_t ps = sizeof (struct obj_poly);
        const size_t ls = sizeof (struct obj_line);

        int si;

        /* Store the following bindings in a vertex array object. */

        glGenVertexArrays(1, &O->vao);
        glBindVertexArray(    O->vao);

        /* Store all vertex data in a vertex buffer object. */

        glGenBuffers(1, &O->vbo);
        glBindBuffer(GL_ARRAY_BUFFER, O->vbo);
        glBufferData(GL_ARRAY_BUFFER, O->vc * vs, O->vv, GL_STATIC_DRAW);

        /* Store all index data in index buffer objects. */

        for (si = 0; si < O->sc; ++si)
        {
            if (O->sv[si].pc > 0)
            {
                glGenBuffers(1, &O->sv[si].pibo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, O->sv[si].pibo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, O->sv[si].pc * ps,
                                        O->sv[si].pv, GL_STATIC_DRAW);
            }

            if (O->sv[si].lc > 0)
            {
                glGenBuffers(1, &O->sv[si].libo);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, O->sv[si].libo);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, O->sv[si].lc * ls,
                                        O->sv[si].lv, GL_STATIC_DRAW);
            }
        }

        /* Enable and bind the attributes. */

        if (O->uloc >= 0)
        {
            glEnableVertexAttribArray(O->uloc);
            glVertexAttribPointer(O->uloc, 3, GL_FLOAT, GL_FALSE, vs, (const GLvoid *)  0);
        }
        if (O->nloc >= 0)
        {
            glEnableVertexAttribArray(O->nloc);
            glVertexAttribPointer(O->nloc, 3, GL_FLOAT, GL_FALSE, vs, (const GLvoid *) 12);
        }
        if (O->tloc >= 0)
        {
            glEnableVertexAttribArray(O->tloc);
            glVertexAttribPointer(O->tloc, 2, GL_FLOAT, GL_FALSE, vs, (const GLvoid *) 24);
        }
        if (O->vloc >= 0)
        {
            glEnableVertexAttribArray(O->vloc);
            glVertexAttribPointer(O->vloc, 3, GL_FLOAT, GL_FALSE, vs, (const GLvoid *) 32);
        }
    }
#endif
}

/*----------------------------------------------------------------------------*/

int obj_cmp_vert(obj *O, int vi, int vj, float eps, float dot)
{
    if (fabs(O->vv[vi].v[0] - O->vv[vj].v[0]) >= eps) return 0;
    if (fabs(O->vv[vi].v[1] - O->vv[vj].v[1]) >= eps) return 0;
    if (fabs(O->vv[vi].v[2] - O->vv[vj].v[2]) >= eps) return 0;

    if (fabs(O->vv[vi].t[0] - O->vv[vj].t[0]) >= eps) return 0;
    if (fabs(O->vv[vi].t[1] - O->vv[vj].t[1]) >= eps) return 0;

    if (O->vv[vi].n[0] * O->vv[vj].n[0] +
        O->vv[vi].n[1] * O->vv[vj].n[1] +
        O->vv[vi].n[2] * O->vv[vj].n[2] < dot) return 0;

    return 1;
}

void obj_swp_vert(obj *O, int vi, int vj)
{
    int si;
    int pi;
    int li;

    /* Replace all occurrences of vi with vj. */

    for (si = 0; si < O->sc; ++si)
    {
        for (pi = 0; pi < O->sv[si].pc; ++pi)
        {
            if (O->sv[si].pv[pi].vi[0] == vi)
                O->sv[si].pv[pi].vi[0] =  vj;
            if (O->sv[si].pv[pi].vi[1] == vi)
                O->sv[si].pv[pi].vi[1] =  vj;
            if (O->sv[si].pv[pi].vi[2] == vi)
                O->sv[si].pv[pi].vi[2] =  vj;
        }
        for (li = 0; li < O->sv[si].lc; ++li)
        {
            if (O->sv[si].lv[li].vi[0] == vi)
                O->sv[si].lv[li].vi[0] =  vj;
            if (O->sv[si].lv[li].vi[1] == vi)
                O->sv[si].lv[li].vi[1] =  vj;
        }
    }
}

void obj_uniq(obj *O, float eps, float dot, int verbose)
{
    int vc = O->vc;
    int vi;
    int vj;
    int di;

    /* Merge all vertices within epsilon of one another. */

    for (vi = 0; vi < O->vc; vi += di)
    {
        di = 1;

        for (vj = 0; vj < vi; ++vj)
        {
            if (obj_cmp_vert(O, vi, vj, eps, dot))
            {
                if (verbose) printf("%d %d\n", vi, vc--);

                obj_swp_vert(O, vi, vj);
                obj_del_vert(O, vi);

                di = 0;
                break;
            }
        }
    }
}

/*----------------------------------------------------------------------------*/

void obj_sort(obj *O, int qc)
{
    const int vc = O->vc;

    struct vert
    {
        int  qs;    /* Cache insertion serial number */
        int *iv;    /* Polygon reference list buffer */
        int  ic;    /* Polygon reference list length */
    };

    /* Vertex optimization data; vertex FIFO cache */

    struct vert *vv = (struct vert *) malloc(vc * sizeof (struct vert));
    int         *qv = (int         *) malloc(qc * sizeof (int        ));

    int qs = 1;   /* Current cache insertion serial number */
    int qi = 0;   /* Current cache insertion point [0, qc) */

    int si;
    int pi;
    int vi;
    int ii;
    int qj;

    /* Initialize the vertex cache to empty. */

    for (qj = 0; qj < qc; ++qj)
        qv[qj] = -1;

    /* Process each surface of this file in turn. */

    for (si = 0; si < O->sc; ++si)
    {
        const int pc = O->sv[si].pc;

        /* Allocate the polygon reference list buffers. */

        int *ip, *iv = (int *) malloc(3 * pc * sizeof (int));

        /* Count the number of polygon references per vertex. */

        memset(vv, 0, vc * sizeof (struct vert));

        for (pi = 0; pi < pc; ++pi)
        {
            const index_t *i = O->sv[si].pv[pi].vi;

            vv[i[0]].ic++;
            vv[i[1]].ic++;
            vv[i[2]].ic++;
        }

        /* Initialize all vertex optimization data. */

        for (vi = 0, ip = iv; vi < vc; ++vi)
        {
            vv[vi].qs = -qc;
            vv[vi].iv =  ip;
            ip += vv[vi].ic;
            vv[vi].ic =   0;
        }

        /* Fill the polygon reference list buffers. */

        for (pi = 0; pi < pc; ++pi)
        {
            const index_t *i = O->sv[si].pv[pi].vi;

            vv[i[0]].iv[vv[i[0]].ic++] = pi;
            vv[i[1]].iv[vv[i[1]].ic++] = pi;
            vv[i[2]].iv[vv[i[2]].ic++] = pi;
        }

        /* Iterate over the polygon array of this surface. */

        for (pi = 0; pi < pc; ++pi)
        {
            const index_t *i = O->sv[si].pv[pi].vi;

            int qd = qs - qc;

            int dk = -1;    /* The best polygon score */
            int pk = pi;    /* The best polygon index */

            /* Find the best polygon among those referred-to by the cache. */

            for (qj = 0; qj < qc; ++qj)
                if (qv[qj] >= 0)

                    for (ii = 0;  ii < vv[qv[qj]].ic; ++ii)
                    {
                        int pj = vv[qv[qj]].iv[ii];
                        int dj = 0;

                        const index_t *j = O->sv[si].pv[pj].vi;

                        /* Recently-used vertex bonus. */

                        if (vv[j[0]].qs > qd) dj += vv[j[0]].qs - qd;
                        if (vv[j[1]].qs > qd) dj += vv[j[1]].qs - qd;
                        if (vv[j[2]].qs > qd) dj += vv[j[2]].qs - qd;

                        /* Low-valence vertex bonus. */

                        dj -= vv[j[0]].ic;
                        dj -= vv[j[1]].ic;
                        dj -= vv[j[2]].ic;

                        if (dk < dj)
                        {
                            dk = dj;
                            pk = pj;
                        }
                    }

            if (pk != pi)
            {
                struct obj_poly temp;

                /* Update the polygon reference list. */

                for (vi = 0; vi < 3; ++vi)
                    for (ii = 0; ii < vv[i[vi]].ic; ++ii)
                        if (vv[i[vi]].iv[ii] == pi)
                        {
                            vv[i[vi]].iv[ii] =  pk;
                            break;
                        }

                /* Swap the best polygon into the current position. */

                temp             = O->sv[si].pv[pi];
                O->sv[si].pv[pi] = O->sv[si].pv[pk];
                O->sv[si].pv[pk] =  temp;
            }

            /* Iterate over the current polygon's vertices. */

            for (vi = 0; vi < 3; ++vi)
            {
                struct vert *vp = vv + i[vi];

                /* If this vertex was a cache miss then queue it. */

                if (qs - vp->qs >= qc)
                {
                    vp->qs = qs++;
                    qv[qi] = i[vi];
                    qi = (qi + 1) % qc;
                }

                /* Remove the current polygon from the reference list. */

                vp->ic--;

                for (ii = 0; ii < vp->ic; ++ii)
                    if (vp->iv[ii] == pk)
                    {
                        vp->iv[ii] = vp->iv[vp->ic];
                        break;
                    }
            }
        }
        free(iv);
    }
    free(qv);
    free(vv);
}

float obj_acmr(obj *O, int qc)
{
    int *vs = (int *) malloc(O->vc * sizeof (int));
    int  qs = 1;

    int si;
    int vi;
    int pi;

    int nn = 0;
    int dd = 0;

    for (si = 0; si < O->sc; ++si)
    {
        for (vi = 0; vi < O->vc; ++vi)
            vs[vi] = -qc;

        for (pi = 0; pi < O->sv[si].pc; ++pi)
        {
            const index_t *i = O->sv[si].pv[pi].vi;

            if (qs - vs[i[0]] >= qc) { vs[i[0]] = qs++; nn++; }
            if (qs - vs[i[1]] >= qc) { vs[i[1]] = qs++; nn++; }
            if (qs - vs[i[2]] >= qc) { vs[i[2]] = qs++; nn++; }

            dd++;
        }
    }

    return (float) nn / (float) dd;
}

/*----------------------------------------------------------------------------*/

#ifndef CONF_NO_GL

static void obj_render_prop(const obj *O, int mi, int ki)
{
    const struct obj_prop *kp = O->mv[mi].kv + ki;

    if (kp->map)
    {
        GLenum wrap = GL_REPEAT;

        /* Bind the property map. */

        glBindTexture(GL_TEXTURE_2D, kp->map);

        /* Apply the property options. */

        if (kp->opt & OBJ_OPT_CLAMP)
            wrap = GL_CLAMP_TO_EDGE;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

        /* Apply the texture coordinate offset and scale. */

        if (O->Mloc[ki] >= 0)
        {
            GLfloat T[16];

            memset(T, 0, sizeof (T));

            T[ 0] = kp->s[0];
            T[ 5] = kp->s[1];
            T[10] = kp->s[2];
            T[12] = kp->o[0];
            T[13] = kp->o[1];
            T[14] = kp->o[2];
            T[15] = 1.0f;

            glUniformMatrix4fv(O->Mloc[ki], 1, GL_FALSE, T);
        }
    }
    else glBindTexture(GL_TEXTURE_2D, 0);
}

void obj_render_mtrl(const obj *O, int mi)
{
    int ki;

    /* Bind all material properties and texture maps. */

    for (ki = 0; ki < OBJ_PROP_COUNT; ki++)
    {
        if (O->oloc[ki] >= 0)
        {
            glActiveTexture(GL_TEXTURE0 + ki);
            obj_render_prop(O, mi, ki);
            glUniform1i(O->oloc[ki], ki);
        }
        if (O->cloc[ki] >= 0)
            glUniform4fv(O->cloc[ki], 1, O->mv[mi].kv[ki].c);
    }
    glActiveTexture(GL_TEXTURE0);
}

void obj_render_surf(const obj *O, int si)
{
    const struct obj_surf *sp = O->sv + si;

    if (0 < sp->pc || sp->lc > 0)
    {
        /* Apply this surface's material. */

        if (0 <= sp->mi && sp->mi < O->mc)
            obj_render_mtrl(O, sp->mi);

        /* Render all polygons. */

        if (sp->pibo)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sp->pibo);
            glDrawElements(GL_TRIANGLES, 3 * sp->pc, GL_INDEX_T, (const GLvoid *) 0);
        }

        /* Render all lines. */

        if (sp->libo)
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sp->libo);
            glDrawElements(GL_LINES, 2 * sp->lc, GL_INDEX_T, (const GLvoid *) 0);
        }
    }
}

void obj_render(obj *O)
{
    int si;

    assert(O);

    /* Initialize the vertex arrays. */

    obj_init(O);

    /* Render each surface. */

    glBindVertexArray(O->vao);

    for (si = 0; si < O->sc; ++si)
        obj_render_surf(O, si);
}

#else

void obj_render(obj *O)
{
}

#endif

/*============================================================================*/

void obj_bound(const obj *O, float *b)
{
    int vi;

    assert(O);

    /* Compute the bounding box of this object. */

    if (O->vc > 0)
    {
        const float *v = O->vv[0].v;

        b[0] = b[3] = v[0];
        b[1] = b[4] = v[1];
        b[2] = b[5] = v[2];
    }

    for (vi = 0; vi < O->vc; ++vi)
    {
        const float *v = O->vv[vi].v;

        if (b[0] > v[0]) b[0] = v[0];
        if (b[1] > v[1]) b[1] = v[1];
        if (b[2] > v[2]) b[2] = v[2];

        if (b[3] < v[0]) b[3] = v[0];
        if (b[4] < v[1]) b[4] = v[1];
        if (b[5] < v[2]) b[5] = v[2];
    }
}

/*============================================================================*/

static void obj_write_map(FILE *fout, const obj *O, int mi, int ki, const char *s)
{
    struct obj_prop *kp = O->mv[mi].kv + ki;

    /* If this property has a map... */

    if (kp->str)
    {
        fprintf(fout, "map_%s ", s);

        /* Store all map options. */

        if ((kp->opt & OBJ_OPT_CLAMP) != 0) fprintf(fout, "-clamp on ");

        /* Store the map offset, if any. */

        if (fabs(kp->o[0]) > 0 ||
            fabs(kp->o[1]) > 0 ||
            fabs(kp->o[2]) > 0) fprintf(fout, "-o %f %f %f ",
                                        kp->o[0], kp->o[1], kp->o[2]);

        /* Store the map scale, if any. */

        if (fabs(kp->s[0] - 1) > 0 ||
            fabs(kp->s[1] - 1) > 0 ||
            fabs(kp->s[2] - 1) > 0) fprintf(fout, "-s %f %f %f ",
                                            kp->s[0], kp->s[1], kp->s[2]);

        /* Store the map image file name. */

        fprintf(fout, "%s\n", kp->str);
    }
}

static void obj_write_mtl(const obj *O, const char *mtl)
{
    FILE *fout;
    int   mi;

    if ((fout = fopen(mtl, "w")))
    {
        for (mi = 0; mi < O->mc; ++mi)
        {
            struct obj_mtrl *mp = O->mv + mi;

            /* Start a new material. */

            if (mp->name)
                fprintf(fout, "newmtl %s\n", mp->name);
            else
                fprintf(fout, "newmtl default\n");

            /* Store all material property colors. */

            fprintf(fout, "Kd %12.8f %12.8f %12.8f\n", mp->kv[OBJ_KD].c[0],
                                                       mp->kv[OBJ_KD].c[1],
                                                       mp->kv[OBJ_KD].c[2]);
            fprintf(fout, "Ka %12.8f %12.8f %12.8f\n", mp->kv[OBJ_KA].c[0],
                                                       mp->kv[OBJ_KA].c[1],
                                                       mp->kv[OBJ_KA].c[2]);
            fprintf(fout, "Ke %12.8f %12.8f %12.8f\n", mp->kv[OBJ_KE].c[0],
                                                       mp->kv[OBJ_KE].c[1],
                                                       mp->kv[OBJ_KE].c[2]);
            fprintf(fout, "Ks %12.8f %12.8f %12.8f\n", mp->kv[OBJ_KS].c[0],
                                                       mp->kv[OBJ_KS].c[1],
                                                       mp->kv[OBJ_KS].c[2]);

            fprintf(fout, "Ns %12.8f\n", mp->kv[OBJ_NS].c[0]);
            fprintf(fout, "d  %12.8f\n", mp->kv[OBJ_KD].c[3]);

            /* Store all material property maps. */

            obj_write_map(fout, O, mi, OBJ_KD, "Kd");
            obj_write_map(fout, O, mi, OBJ_KA, "Ka");
            obj_write_map(fout, O, mi, OBJ_KA, "Ke");
            obj_write_map(fout, O, mi, OBJ_KS, "Ks");
            obj_write_map(fout, O, mi, OBJ_NS, "Ns");
            obj_write_map(fout, O, mi, OBJ_KN, "Kn");
        }
    }
    fclose(fout);
}

static void obj_write_obj(const obj *O, const char *obj,
                                        const char *mtl, int prec)
{
    FILE *fout;

    if ((fout = fopen(obj, "w")))
    {
        char formv[256];
        char formt[256];
        char formn[256];

        int si;
        int vi;
        int pi;
        int li;

        if (mtl) fprintf(fout, "mtllib %s\n", mtl);

        /* Store all vertex data. */

        sprintf(formv, "v %%.%df %%.%df %%.%df\n",  prec, prec, prec);
        sprintf(formt, "vt %%.%df %%.%df\n",        prec, prec);
        sprintf(formn, "vn %%.%df %%.%df %%.%df\n", prec, prec, prec);

        for (vi = 0; vi < O->vc; ++vi)
            fprintf(fout, formv, O->vv[vi].v[0],
                                 O->vv[vi].v[1],
                                 O->vv[vi].v[2]);
        for (vi = 0; vi < O->vc; ++vi)
            fprintf(fout, formt, O->vv[vi].t[0],
                                 O->vv[vi].t[1]);
        for (vi = 0; vi < O->vc; ++vi)
            fprintf(fout, formn, O->vv[vi].n[0],
                                 O->vv[vi].n[1],
                                 O->vv[vi].n[2]);

        for (si = 0; si < O->sc; ++si)
        {
            int mi = O->sv[si].mi;

            /* Store the surface's material reference */

            if (0 <= mi && mi < O->mc && O->mv[mi].name)
                fprintf(fout, "usemtl %s\n", O->mv[O->sv[si].mi].name);
            else
                fprintf(fout, "usemtl default\n");

            /* Store all polygon definitions. */

            for (pi = 0; pi < O->sv[si].pc; pi++)
            {
                int vi0 = O->sv[si].pv[pi].vi[0] + 1;
                int vi1 = O->sv[si].pv[pi].vi[1] + 1;
                int vi2 = O->sv[si].pv[pi].vi[2] + 1;

                fprintf(fout, "f %d/%d/%d %d/%d/%d %d/%d/%d\n", vi0, vi0, vi0,
                                                                vi1, vi1, vi1,
                                                                vi2, vi2, vi2);
            }

            /* Store all line definitions. */

            for (li = 0; li < O->sv[si].lc; li++)
            {
                int vi0 = O->sv[si].lv[li].vi[0] + 1;
                int vi1 = O->sv[si].lv[li].vi[1] + 1;

                fprintf(fout, "l %d/%d/%d %d/%d/%d\n", vi0, vi0, vi0,
                                                       vi1, vi1, vi1);
            }
        }

        fclose(fout);
    }
}

void obj_write(const obj *O, const char *obj, const char *mtl, int prec)
{
    assert(O);

    if (obj) obj_write_obj(O, obj, mtl, prec);
    if (mtl) obj_write_mtl(O, mtl);
}

/*============================================================================*/
