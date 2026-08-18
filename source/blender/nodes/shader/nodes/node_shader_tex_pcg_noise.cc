/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_noise.hh"

#include "FN_multi_function.hh"

#include "NOD_multi_function.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender {

namespace nodes::node_shader_tex_pcg_noise_cc {

static void sh_node_tex_pcg_noise_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector"_ustr)
      .min(-10000.0f)
      .max(10000.0f)
      .default_input_type(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_output<decl::Color>("Color"_ustr);
}

static int gpu_shader_tex_pcg_noise(GPUMaterial *mat,
                                    bNode *node,
                                    bNodeExecData * /*execdata*/,
                                    GPUNodeStack *in,
                                    GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_tex_pcg_noise", in, out);
}

/* Port del hash PCG de Unreal Engine (Rand3DPCG16, RandomPCG.ush) para uso en CPU
 * (viewport Solid mode, field evaluation, etc). El GPU usa su propia copia en el .glsl. */
static float3 pcg16_hash_to_float3(const float3 &vector)
{
  int px = int(math::floor(vector.x));
  int py = int(math::floor(vector.y));
  int pz = int(math::floor(vector.z));

  uint32_t x = uint32_t(px);
  uint32_t y = uint32_t(py);
  uint32_t z = uint32_t(pz);

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

  return float3(float(x), float(y), float(z)) * (1.0f / 65535.0f);
}

class PCGNoiseFunction : public mf::MultiFunction {
 public:
  PCGNoiseFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature sig;
      mf::SignatureBuilder builder{"PCGNoise", sig};
      builder.single_input<float3>("Vector");
      builder.single_output<ColorGeometry4f>("Color");
      return sig;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    MutableSpan<ColorGeometry4f> r_color =
        params.uninitialized_single_output<ColorGeometry4f>(1, "Color");

    mask.foreach_index([&](const int64_t i) {
      const float3 c = pcg16_hash_to_float3(vector[i]);
      r_color[i] = ColorGeometry4f(c[0], c[1], c[2], 1.0f);
    });
  }

  void hash_unique(UniqueHashBytes &hash) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }
};

static void sh_node_pcg_noise_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static PCGNoiseFunction fn;
  builder.set_matching_fn(fn);
}

}  // namespace nodes::node_shader_tex_pcg_noise_cc

void register_node_type_sh_tex_pcg_noise()
{
  namespace file_ns = nodes::node_shader_tex_pcg_noise_cc;

  static bke::bNodeType ntype;

  common_node_type_base(&ntype, "ShaderNodeTexPCGNoise"_ustr, SH_NODE_TEX_PCG_NOISE);
  ntype.ui_name = "PCG Noise Texture";
  ntype.ui_description = "Calculate a random color using Unreal Engine's PCG hash";
  ntype.enum_name_legacy = "TEX_PCG_NOISE";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_pcg_noise_declare;
  ntype.gpu_fn = file_ns::gpu_shader_tex_pcg_noise;
  ntype.build_multi_function = file_ns::sh_node_pcg_noise_build_multi_function;
  bke::node_register_type(ntype);
}

}  // namespace blender
