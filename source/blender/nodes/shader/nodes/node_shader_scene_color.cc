/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"

namespace blender {

namespace nodes::node_shader_scene_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Vector"_ustr)
      .description("Normalized (0-1) coordinates to sample from. Defaults to screen space UVs")
      .hide_value();
  b.add_output<decl::Color>("Color"_ustr)
      .description("Color of the scene behind the surface");
}

static int node_shader_gpu_scene_color(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack *in,
                                       GPUNodeStack *out)
{
  /* EEVEE binds the previous layer radiance only for materials flagged as transparent
   * (forward pipeline). Reuse that flag instead of introducing a new one. */
  GPU_material_flag_set(mat, GPU_MATFLAG_TRANSPARENT);

  const char *function_name = in[0].link ? "node_scene_color" : "node_scene_color_screen_uv";
  return GPU_stack_link(
      mat, node, function_name, in, out, GPU_kernel_globals(), GPU_shading_data());
}

}  // namespace nodes::node_shader_scene_color_cc

/* node type definition */
void register_node_type_sh_scene_color()
{
  namespace file_ns = nodes::node_shader_scene_color_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeSceneColor"_ustr, SH_NODE_SCENE_COLOR);
  ntype.ui_name = "Scene Color";
  ntype.ui_description =
      "Sample the color of the scene rendered behind the surface, for refraction and "
      "distortion effects.\nNote: only supported in EEVEE, with Blended materials";
  ntype.enum_name_legacy = "SCENE_COLOR";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.add_ui_poll = object_eevee_shader_nodes_poll;
  ntype.gpu_fn = file_ns::node_shader_gpu_scene_color;

  bke::node_register_type(ntype);
}

}  // namespace blender
