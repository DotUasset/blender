/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_shader_util.hh"

#include "BLI_math_vector.hh"

#include "NOD_multi_function.hh"

namespace blender {

namespace nodes::node_shader_tex_pcg_jacobian_simplex_cc {

static void sh_node_tex_pcg_jacobian_simplex_declare(NodeDeclarationBuilder &b)
{
  b.is_function_node();
  b.add_input<decl::Vector>("Vector"_ustr)
      .min(-10000.0f)
      .max(10000.0f)
      .default_input_type(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_output<decl::Color>("Noise"_ustr).no_muted_links();
  b.add_output<decl::Color>("Gradient"_ustr).no_muted_links();
  b.add_output<decl::Color>("Curl"_ustr).no_muted_links();
  b.add_output<decl::Float>("Distance"_ustr).no_muted_links();
}

static int gpu_shader_tex_pcg_jacobian_simplex(GPUMaterial *mat,
                                               bNode *node,
                                               bNodeExecData * /*execdata*/,
                                               GPUNodeStack *in,
                                               GPUNodeStack *out)
{
  return GPU_stack_link(mat, node, "node_tex_pcg_jacobian_simplex", in, out);
}

/* ---- Rand3DPCG16, mismo puerto que en los otros dos nodos PCG ---- */
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

/* ---- Puerto del truco de bits de MGradient (Random.ush): extrae 3 signos
 * (±1) de 3 bits distintos del mismo hash de 16 bits. ---- */
static float3 pcg_mgradient_direction(uint32_t r)
{
  float x = float(r & 0x8000u) * (1.0f / 16384.0f) - 1.0f;
  float y = float(r & 0x4000u) * (1.0f / 8192.0f) - 1.0f;
  float z = float(r & 0x2000u) * (1.0f / 4096.0f) - 1.0f;
  return float3(x, y, z);
}

/* ---- Puerto de SimplexCorners (Random.ush) ---- */
static void pcg_simplex_corners(const float3 &v, float3 corners[4])
{
  float skew = (v.x + v.y + v.z) / 3.0f;
  float3 tet = math::floor(v + float3(skew, skew, skew));
  
  float unskew = (tet.x + tet.y + tet.z) / 6.0f;
  float3 base = tet - float3(unskew, unskew, unskew);
  float3 f = v - base;

  float3 f_yzx = float3(f.y, f.z, f.x);
  float3 g = float3(f.x >= f_yzx.x ? 1.0f : 0.0f,
                    f.y >= f_yzx.y ? 1.0f : 0.0f,
                    f.z >= f_yzx.z ? 1.0f : 0.0f);
  float3 h = float3(1.0f - g.z, 1.0f - g.x, 1.0f - g.y);

  float3 a1 = float3(std::min(g.x, h.x), std::min(g.y, h.y), std::min(g.z, h.z)) -
             (1.0f / 6.0f);
  float3 a2 = float3(std::max(g.x, h.x), std::max(g.y, h.y), std::max(g.z, h.z)) -
             (1.0f / 3.0f);

  corners[0] = base;
  corners[1] = base + a1;
  corners[2] = base + a2;
  corners[3] = base + 0.5f;
}

struct JacobianResult {
  float3 grad[3]; /* derivada (xyz) de cada canal k=0,1,2 */
  float noise[3]; /* valor escalar de cada canal k=0,1,2 */
};

/* ---- Puerto de JacobianSimplex_ALU (Random.ush), bTiling siempre false,
 * fusionando SimplexSmooth + SimplexDSmooth (sin tipos matriciales) ---- */
static JacobianResult pcg_jacobian_simplex(const float3 &v)
{
  float3 T[4];
  pcg_simplex_corners(v, T);

  float3 fv[4];
  float3 gvec[3][4];
  float grad[3][4];

  for (int c = 0; c < 4; c++) {
    fv[c] = v - T[c];

    float3 scaled = 6.0f * T[c] + 0.5f;
    int3 ip = int3(int(math::floor(scaled.x)),
                   int(math::floor(scaled.y)),
                   int(math::floor(scaled.z)));
    int3 rand = pcg16_hash_i(ip);

    gvec[0][c] = pcg_mgradient_direction(uint32_t(rand.x));
    gvec[1][c] = pcg_mgradient_direction(uint32_t(rand.y));
    gvec[2][c] = pcg_mgradient_direction(uint32_t(rand.z));

    grad[0][c] = math::dot(gvec[0][c], fv[c]);
    grad[1][c] = math::dot(gvec[1][c], fv[c]);
    grad[2][c] = math::dot(gvec[2][c], fv[c]);
  }

  const float scale = 1024.0f / 375.0f;
  float sv[4], ds_x[4], ds_y[4], ds_z[4];

  for (int c = 0; c < 4; c++) {
    float d = math::dot(fv[c], fv[c]);
    float s = std::min(std::max(2.0f * d, 0.0f), 1.0f); /* saturate */

    sv[c] = scale + s * (-3.0f * scale + s * (3.0f * scale - s * scale));

    float dsscale = -12.0f * scale + s * (24.0f * scale - s * 12.0f * scale);
    ds_x[c] = dsscale * fv[c].x;
    ds_y[c] = dsscale * fv[c].y;
    ds_z[c] = dsscale * fv[c].z;
  }

  JacobianResult result;
  for (int k = 0; k < 3; k++) {
    float gx = 0.0f, gy = 0.0f, gz = 0.0f, w = 0.0f;
    for (int c = 0; c < 4; c++) {
      gx += sv[c] * gvec[k][c].x + ds_x[c] * grad[k][c];
      gy += sv[c] * gvec[k][c].y + ds_y[c] * grad[k][c];
      gz += sv[c] * gvec[k][c].z + ds_z[c] * grad[k][c];
      w += sv[c] * grad[k][c];
    }
    result.grad[k] = float3(gx, gy, gz);
    result.noise[k] = w;
  }

  return result;
}

class PCGJacobianSimplexFunction : public mf::MultiFunction {
 public:
  PCGJacobianSimplexFunction()
  {
    static const mf::Signature signature = []() {
      mf::Signature sig;
      mf::SignatureBuilder builder{"PCGJacobianSimplex", sig};
      builder.single_input<float3>("Vector");
      builder.single_output<ColorGeometry4f>("Noise", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<ColorGeometry4f>("Gradient", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<ColorGeometry4f>("Curl", mf::ParamFlag::SupportsUnusedOutput);
	  builder.single_output<float>("Distance", mf::ParamFlag::SupportsUnusedOutput);
      return sig;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &vector = params.readonly_single_input<float3>(0, "Vector");
    MutableSpan<ColorGeometry4f> r_noise =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(1, "Noise");
    MutableSpan<ColorGeometry4f> r_gradient =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(2, "Gradient");
    MutableSpan<ColorGeometry4f> r_curl =
        params.uninitialized_single_output_if_required<ColorGeometry4f>(3, "Curl");
	MutableSpan<float> r_distance =
	    params.uninitialized_single_output_if_required<float>(4, "Distance");

    const bool calc_noise = !r_noise.is_empty();
    const bool calc_gradient = !r_gradient.is_empty();
    const bool calc_curl = !r_curl.is_empty();
	const bool calc_distance = !r_distance.is_empty();

    mask.foreach_index([&](const int64_t i) {
      const JacobianResult j = pcg_jacobian_simplex(vector[i]);

      if (calc_noise) {
        r_noise[i] = ColorGeometry4f(j.noise[0], j.noise[1], j.noise[2], 1.0f);
      }
      if (calc_gradient) {
        r_gradient[i] = ColorGeometry4f(j.grad[0].x, j.grad[0].y, j.grad[0].z, 1.0f);  // A=1.0 ahora
      }
      if (calc_curl) {
        const float cx = j.grad[2].y - j.grad[1].z;
        const float cy = j.grad[0].z - j.grad[2].x;
        const float cz = j.grad[1].x - j.grad[0].y;
        r_curl[i] = ColorGeometry4f(cx, cy, cz, 1.0f);
      }
	  if (calc_distance) {
		r_distance[i] = j.noise[0];
	  }
    });
  }

  void hash_unique(UniqueHashBytes &hash) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
  }
};

static void sh_node_pcg_jacobian_simplex_build_multi_function(NodeMultiFunctionBuilder &builder)
{
  static PCGJacobianSimplexFunction fn;
  builder.set_matching_fn(fn);
}

}  // namespace nodes::node_shader_tex_pcg_jacobian_simplex_cc

void register_node_type_sh_tex_pcg_jacobian_simplex()
{
  namespace file_ns = nodes::node_shader_tex_pcg_jacobian_simplex_cc;

  static bke::bNodeType ntype;

  common_node_type_base(
      &ntype, "ShaderNodeTexPCGJacobianSimplex"_ustr, SH_NODE_TEX_PCG_JACOBIAN_SIMPLEX);
  ntype.ui_name = "PCG Jacobian Simplex Texture";
  ntype.ui_description =
      "Simplex noise and its Jacobian derivative (Noise/Gradient/Curl), using Unreal Engine's "
      "JacobianSimplex_ALU";
  ntype.enum_name_legacy = "TEX_PCG_JACOBIAN_SIMPLEX";
  ntype.nclass = NODE_CLASS_TEXTURE;
  ntype.declare = file_ns::sh_node_tex_pcg_jacobian_simplex_declare;
  ntype.gpu_fn = file_ns::gpu_shader_tex_pcg_jacobian_simplex;
  ntype.build_multi_function = file_ns::sh_node_pcg_jacobian_simplex_build_multi_function;

  bke::node_register_type(ntype);
}

}  // namespace blender