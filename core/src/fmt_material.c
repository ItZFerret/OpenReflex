/* fmt_material.c -- see fmt_material.h. Reference: terrain_material_export.py parse_material, ParseMaterial in RfxData.cs. */
#include "fmt_material.h"

#include <stdlib.h>

void fmt_material_free(fmt_material* m)
{
    if (!m) return;
    free(m->textures); free(m->params);
    fmt_bxml_free(m->doc);
    free(m);
}

static int read_pairs(const fmt_bxml* d, int node, fmt_state_pair* out)
{
    fmt_bxml_value v;
    if (node < 0 || fmt_bxml_node_value(d, node, &v) < 0 || v.kind != FMT_BX_BYTES) return 0;
    int n = (int)(v.count / 8);
    if (n > FMT_MAT_MAX_STATES) n = FMT_MAT_MAX_STATES;
    for (int i = 0; i < n; i++) { out[i].key = fmt_u32(v.data + 8 * (size_t)i); out[i].value = fmt_u32(v.data + 8 * (size_t)i + 4); }
    return n;
}

fmt_material* fmt_material_parse(const uint8_t* data, size_t size)
{
    fmt_bxml* d = fmt_bxml_parse(data, size);
    if (!d) return NULL;
    if (strcmp(fmt_bxml_tag(d, 0), "MaterialEditorMaterial") != 0) { fmt_fail("material: root element is %s", fmt_bxml_tag(d, 0)); fmt_bxml_free(d); return NULL; }
    fmt_material* m = (fmt_material*)fmt_alloc(sizeof *m);
    if (!m) { fmt_bxml_free(d); return NULL; }
    m->doc = d;
    m->num_passes = fmt_bxml_attr_i32(d, 0, "numPasses", 0);
    int pass = fmt_bxml_find_child(d, 0, "Pass");
    if (pass < 0) { fmt_fail("material: no <Pass>"); fmt_material_free(m); return NULL; }
    int sh = fmt_bxml_find_child(d, pass, "Shaders");
    if (sh >= 0) {
        m->shader = fmt_bxml_attr_str(d, sh, "asset");
        int vs = fmt_bxml_find_child(d, sh, "VertexShader"), ps = fmt_bxml_find_child(d, sh, "PixelShader");
        if (vs >= 0) { m->vertex_shader = fmt_bxml_attr_str(d, vs, "asset"); m->vertex_entry = fmt_bxml_attr_str(d, vs, "entryPoint"); m->skinning_entry = fmt_bxml_attr_str(d, vs, "skinningEntryPoint"); }
        if (ps >= 0) { m->pixel_shader = fmt_bxml_attr_str(d, ps, "asset"); m->pixel_entry = fmt_bxml_attr_str(d, ps, "entryPoint"); if (!m->shader) m->shader = m->pixel_shader; }
    }
    int tx = fmt_bxml_find_child(d, pass, "Textures");
    int nt = tx >= 0 ? fmt_bxml_child_count(d, tx) : 0;
    m->textures = (fmt_material_texture*)fmt_alloc(((size_t)nt + 1) * sizeof *m->textures);
    int pr = fmt_bxml_find_child(d, pass, "Parameters");
    int np = pr >= 0 ? fmt_bxml_child_count(d, pr) : 0;
    m->params = (fmt_material_param*)fmt_alloc(((size_t)np + 1) * sizeof *m->params);
    if (!m->textures || !m->params) { fmt_material_free(m); return NULL; }
    for (int i = 0; i < nt; i++) {
        int t = fmt_bxml_child(d, tx, i);
        if (strcmp(fmt_bxml_tag(d, t), "Texture") != 0) continue;
        fmt_material_texture* mt = &m->textures[m->ntextures++];
        mt->name = fmt_bxml_attr_str(d, t, "textureName");
        if (!mt->name) mt->name = "";
        mt->nsampler = read_pairs(d, fmt_bxml_find_child(d, t, "XenonSamplerStates"), mt->sampler);
    }
    for (int i = 0; i < np; i++) {
        int p = fmt_bxml_child(d, pr, i);
        if (strcmp(fmt_bxml_tag(d, p), "Parameter") != 0) continue;
        fmt_material_param* mp = &m->params[m->nparams];
        mp->name = fmt_bxml_attr_str(d, p, "name");
        if (!mp->name) continue;
        m->nparams++;
        mp->type = fmt_bxml_attr_str(d, p, "type");
        fmt_bxml_value v;
        fmt_bxml_node_value(d, p, &v);
        if (v.kind == FMT_BX_VEC4 && v.count >= 1) { fmt_bx_floats(&v, 0, mp->value); mp->ncomp = 4; }
        else if (v.kind == FMT_BX_FLOAT && v.count >= 1) { mp->value[0] = fmt_bx_f32(&v, 0, 0); mp->ncomp = 1; }
    }
    m->nstates = read_pairs(d, fmt_bxml_find_child(d, pass, "XenonStates"), m->states);
    return m;
}

const fmt_material_param* fmt_material_param_find(const fmt_material* m, const char* name)
{
    if (m && name) for (int i = 0; i < m->nparams; i++) if (!strcmp(m->params[i].name, name)) return &m->params[i];
    return NULL;
}

int fmt_state_get(const fmt_state_pair* pairs, int n, uint32_t key, uint32_t* value)
{
    for (int i = 0; i < n; i++) if (pairs[i].key == key) { if (value) *value = pairs[i].value; return 1; }
    return 0;
}
