/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup shdnodes
 */

#include "node_shader_util.hh"
#include "node_util.hh"

#include "BKE_image.hh"
#include "BKE_node_runtime.hh"
#include "BKE_texture.h"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace blender::nodes::node_shader_input_image_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Image>("Image"_ustr).custom_draw([](CustomSocketDrawParams &params) {
    params.layout.alignment_set(ui::LayoutAlign::Expand);
    ui::template_id(&params.layout,
                    &params.C,
                    &params.node_ptr,
                    "image",
                    "IMAGE_OT_new",
                    "IMAGE_OT_open",
                    nullptr);
  });
}

static int node_shader_gpu_input_image(GPUMaterial *mat,
                                       bNode *node,
                                       bNodeExecData * /*execdata*/,
                                       GPUNodeStack * /*in*/,
                                       GPUNodeStack *out)
{
  Image *ima = id_cast<Image *>(node->id);

  if (!ima) {
    return 1;
  }

  /* Use default sampler state for the image output. */
  GPUSamplerState sampler_state = GPUSamplerState::default_sampler();
  sampler_state.filtering = GPU_SAMPLER_FILTERING_LINEAR | GPU_SAMPLER_FILTERING_MIPMAP;

  /* Use the image user of the original node: the GPU image keeps a pointer to it and the
   * dependency refresh updates the original, not the evaluated copy. */
  bNode *node_original = node->runtime->original ? node->runtime->original : node;
  NodeTexImage *tex = static_cast<NodeTexImage *>(node_original->storage);
  ImageUser *iuser = &tex->iuser;

  /* Create the image link and output it. */
  GPUNodeLink *gpu_image = GPU_image(mat, ima, iuser, sampler_state);
  out[0].link = gpu_image;

  return 1;
}

static void node_shader_init_input_image(bNodeTree * /*ntree*/, bNode *node)
{
  NodeTexImage *tex = MEM_new<NodeTexImage>(__func__);
  BKE_texture_mapping_default(&tex->base.tex_mapping, TEXMAP_TYPE_POINT);
  BKE_texture_colormapping_default(&tex->base.color_mapping);
  BKE_imageuser_default(&tex->iuser);

  node->storage = tex;
}

}  // namespace nodes::node_shader_input_image_cc

namespace blender {

void register_node_type_sh_input_image()
{
  namespace file_ns = nodes::node_shader_input_image_cc;

  static bke::bNodeType ntype;

  sh_node_type_base(&ntype, "ShaderNodeInputImage"_ustr, SH_NODE_INPUT_IMAGE);
  ntype.ui_name = "Image";
  ntype.ui_description = "Input an image data-block";
  ntype.enum_name_legacy = "INPUT_IMAGE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_shader_init_input_image;
  bke::node_type_storage(
      ntype, "NodeTexImage", node_free_standard_storage, node_copy_standard_storage);
  ntype.gpu_fn = file_ns::node_shader_gpu_input_image;
  ntype.default_width = bke::NodeWidth::_240;

  bke::node_register_type(ntype);
}

}  // namespace blender