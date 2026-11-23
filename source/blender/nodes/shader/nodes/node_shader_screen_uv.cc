/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_screen_uv_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Vector>("Vector"_ustr)
      .description("Screen space UV coordinates (0-1), Z is always 0");
}

static int node_shader_gpu_screen_uv(GPUMaterial *mat,
                                     bNode *node,
                                     bNodeExecData * /*execdata*/,
                                     GPUNodeStack *in,
                                     GPUNodeStack *out)
{
  /* The previous layer radiance buffer size is only available when transparency is enabled
   * (forward pipeline). Reuse that flag instead of introducing a new one. */
  GPU_material_flag_set(mat, GPU_MATFLAG_TRANSPARENT);

  return GPU_stack_link(
      mat, node, "node_screen_uv", in, out, GPU_kernel_globals(), GPU_shading_data());
}

}  // namespace nodes::node_shader_screen_uv_cc

/* node type definition */
void register_node_type_sh_screen_uv()
{
  namespace file_ns = nodes::node_shader_screen_uv_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeScreenUV"_ustr, SH_NODE_SCREEN_UV);
  ntype.ui_name = "Screen UV";
  ntype.ui_description =
      "Output the screen space UV coordinates used by the Scene Color node when its Vector input "
      "is unconnected.\n"
      "Returns (frag_co.xy / buffer_size, 0.0). Only works in EEVEE with blended materials.";
  ntype.enum_name_legacy = "SCREEN_UV";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_eevee_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_screen_uv;

  bke::node_register_type(ntype);
}

}  // namespace blender