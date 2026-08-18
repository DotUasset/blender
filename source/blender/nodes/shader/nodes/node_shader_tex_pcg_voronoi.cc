/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BLI_math_vector.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender {

namespace nodes::node_shader_tex_pcg_voronoi_cc {

NODE_STORAGE_FUNCS(NodeTexPCGVoronoi)

static void sh_node_tex_pcg_voronoi_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector"_ustr)
      .min(-10000.0f)
      .max(10000.0f)
      .default_input_type(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_output<decl::Color>("Position"_ustr);
}

static void node_shader_buts_tex_pcg_voronoi(ui::Layout &layout,
                                             bContext * /*C*/,
                                             PointerRNA *ptr)
{
  layout.prop(ptr, "quality", ui::ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);
}

static void node_shader_init_tex_pcg_voronoi(bNodeTree * /*node_tree*/, bNode *node)
{
  NodeTexPCGVoronoi *tex = MEM_new<NodeTexPCGVoronoi>(__func__);
  tex->quality = 1;
  node->storage = tex;
}

static int gpu_shader_tex_pcg_voronoi(GPUMaterial *mat,
                                      bNode *node,
                                      bNodeExecData * /*execdata*/,
                                      GPUNodeStack *in,
                                      GPUNodeStack *out)
{
  NodeTexPCGVoronoi *tex = static_cast<NodeTexPCGVoronoi *>(node->storage);
  float quality = float(tex->quality);
  return GPU_stack_link(mat, node, "node_tex_pcg_voronoi", in, out, GPU_constant(&quality));
}

/* ---- Rand3DPCG16, mismo puerto que en node_shader_tex_pcg_noise.cc ---- */
static int3 pcg16_hash_i(const int3 &p)
{
  uint32_t x = uint32_t(p.x);
  uint32_t y = uint32_t(p.y);
  uint32_t z = uint32_t(p.z);

  x = x * 1664525u + 1013904223u;
  y = y * 1664525u + 1013904223u;
  z = z * 1664525u + 1013904223u;

  x += y * z;
  y += z * x;
  z += x * y;
  x += y * z;
  y += z * x;
  z += x * y;

  x >>= 16u;
  y >>= 16u;
  z >>= 16u;

  return int3(int(x), int(y), int(z));
}

/* ---- Puerto de VoronoiCornerSample (Random.ush) ---- */
static float3 pcg_voronoi_corner_sample(const float3 &pos, int quality)
{
  int3 ip = int3(int(pos.x), int(pos.y), int(pos.z));
  int3 h = pcg16_hash_i(ip);
  float3 noise = float3(float(h.x), float(h.y), float(h.z)) * (1.0f / 65535.0f) - 0.5f;

  if (quality <= 2) {
    return math::normalize(noise) * 0.2588f;
  }
  if (quality == 3) {
    return math::normalize(noise) * 0.3090f;
  }
  return noise;
}

/* ---- Puerto de VoronoiCompare (Random.ush), variante posición+distancia ---- */
static float4 pcg_voronoi_compare(const float4 &minval, const float3 &candidate, const float3 &offset)
{
  float newdist = math::dot(offset, offset);
  if (newdist > minval.w) {
    return minval;
  }
  return float4(candidate.x, candidate.y, candidate.z, newdist);
}

/* ---- Puerto de VoronoiNoise3D_ALU (Random.ush), bTiling siempre false ---- */
static float4 pcg_voronoi_noise_3d(const float3 &v, int quality)
{
  float3 fv = v - math::floor(v);
  float3 fv2 = (v + 0.5f) - math::floor(v + 0.5f);
  float3 iv = math::floor(v);
  float3 iv2 = math::floor(v + 0.5f);

  float4 mindist = float4(0.0f, 0.0f, 0.0f, 100.0f);
  float3 offset, p;

  if (quality == 3) {
    /* búsqueda 3x3x3 */
    for (offset.x = -1.0f; offset.x <= 1.0f; offset.x += 1.0f) {
      for (offset.y = -1.0f; offset.y <= 1.0f; offset.y += 1.0f) {
        for (offset.z = -1.0f; offset.z <= 1.0f; offset.z += 1.0f) {
          p = offset + pcg_voronoi_corner_sample(iv2 + offset, quality);
          mindist = pcg_voronoi_compare(mindist, iv2 + p, fv2 - p);
        }
      }
    }
  }
  else {
    /* búsqueda base 2x2x2 */
    for (offset.x = 0.0f; offset.x <= 1.0f; offset.x += 1.0f) {
      for (offset.y = 0.0f; offset.y <= 1.0f; offset.y += 1.0f) {
        for (offset.z = 0.0f; offset.z <= 1.0f; offset.z += 1.0f) {
          p = offset + pcg_voronoi_corner_sample(iv + offset, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);

          if (quality == 2) {
            p = offset + pcg_voronoi_corner_sample(iv2 + offset + 467.0f, quality);
            mindist = pcg_voronoi_compare(mindist, iv2 + p, fv2 - p);
          }
        }
      }
    }
  }

  if (quality >= 4) {
    /* shells extra a lo largo de cada eje */
    for (offset.x = -1.0f; offset.x <= 2.0f; offset.x += 3.0f) {
      for (offset.y = 0.0f; offset.y <= 1.0f; offset.y += 1.0f) {
        for (offset.z = 0.0f; offset.z <= 1.0f; offset.z += 1.0f) {
          p = offset + pcg_voronoi_corner_sample(iv + offset, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);

          float3 offset_yzx = float3(offset.y, offset.z, offset.x);
          p = offset_yzx + pcg_voronoi_corner_sample(iv + offset_yzx, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);

          float3 offset_zxy = float3(offset.z, offset.x, offset.y);
          p = offset_zxy + pcg_voronoi_corner_sample(iv + offset_zxy, quality);
          mindist = pcg_voronoi_compare(mindist, iv + p, fv - p);
        }
      }
    }
  }

  return float4(mindist.x, mindist.y, mindist.z, std::sqrt(mindist.w));
}

class PCGVoronoiFunction : public mf::MultiFunction {
 private:
  int quality_;

 public:
  PCGVoronoiFunction(int quality) : quality_(quality)
  {
    static const mf::Signature signature = []() {
      mf::Signature sig;
      mf::SignatureBuilder builder{"PCGVoronoi", sig};
      builder.single_input<float3>("Vector");
      builder.single_output<ColorGeometry4f>("Position");
      return sig;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    MutableSpan<ColorGeometry4f> r_position =
        params.uninitialized_single_output<ColorGeometry4f>(1, "Position");

    mask.foreach_index([&](const int64_t i) {
      const float4 result = pcg_voronoi_noise_3d(vector[i], quality_);
      r_position[i] = ColorGeometry4f(result.x, result.y, result.z, result.w);
    });
  }

  void hash_unique(UniqueHashBytes &hash) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(quality_);
  }
};

static void sh_node_pcg_voronoi_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  const NodeTexPCGVoronoi &storage = node_storage(builder.node());
  builder.construct_and_set_matching_fn<PCGVoronoiFunction>(storage.quality);
}

}  // namespace nodes::node_shader_tex_pcg_voronoi_cc

void register_node_type_sh_tex_pcg_voronoi()
{
  namespace file_ns = nodes::node_shader_tex_pcg_voronoi_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexPCGVoronoi"_ustr, SH_NODE_TEX_PCG_VORONOI);
  ntype.ui_name = "PCG Voronoi Texture";
  ntype.ui_description =
      "Unreal Engine-compatible Voronoi noise (VoronoiNoise3D_ALU) using the PCG hash";
  ntype.enum_name_legacy = "TEX_PCG_VORONOI";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_pcg_voronoi_declare;
  ntype.draw_buttons = file_ns::node_shader_buts_tex_pcg_voronoi;
  ntype.initfunc = file_ns::node_shader_init_tex_pcg_voronoi;
  bke::node_type_storage(
      ntype, "NodeTexPCGVoronoi", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::gpu_shader_tex_pcg_voronoi;
  ntype.build_multi_function = file_ns::sh_node_pcg_voronoi_build_multi_function;

  bke::node_register_type(ntype);
}

}  // namespace blender