/*
 *  Copyright (c) Facebook, Inc. and its affiliates.
 */

#include "h265_sps_parser.h"

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include "h265_common.h"
#include "h265_profile_tier_level_parser.h"
#include "h265_vui_parameters_parser.h"

namespace {
typedef std::shared_ptr<h265nal::H265SpsParser::SpsState> SharedPtrSps;
}  // namespace

namespace h265nal {

// General note: this is based off the 2016/12 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse SPS state from the supplied buffer.
std::shared_ptr<H265SpsParser::SpsState> H265SpsParser::ParseSps(
    const uint8_t* data, size_t length) noexcept {
  std::vector<uint8_t> unpacked_buffer = UnescapeRbsp(data, length);
  rtc::BitBuffer bit_buffer(unpacked_buffer.data(), unpacked_buffer.size());
  return ParseSps(&bit_buffer);
}

std::shared_ptr<H265SpsParser::SpsState> H265SpsParser::ParseSps(
    rtc::BitBuffer* bit_buffer) noexcept {
  uint32_t bits_tmp;
  uint32_t golomb_tmp;

  // H265 SPS Nal Unit (seq_parameter_set_rbsp()) parser.
  // Section 7.3.2.2 ("Sequence parameter set data syntax") of the H.265
  // standard for a complete description.
  auto sps = std::make_shared<SpsState>();

  // sps_video_parameter_set_id  u(4)
  if (!bit_buffer->ReadBits(&(sps->sps_video_parameter_set_id), 4)) {
    return nullptr;
  }

  // sps_max_sub_layers_minus1  u(3)
  if (!bit_buffer->ReadBits(&(sps->sps_max_sub_layers_minus1), 3)) {
    return nullptr;
  }

  // sps_temporal_id_nesting_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->sps_temporal_id_nesting_flag), 1)) {
    return nullptr;
  }

  // profile_tier_level(1, sps_max_sub_layers_minus1)
  sps->profile_tier_level = H265ProfileTierLevelParser::ParseProfileTierLevel(
      bit_buffer, true, sps->sps_max_sub_layers_minus1);

  // sps_seq_parameter_set_id  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(&(sps->sps_seq_parameter_set_id))) {
    return nullptr;
  }

  // chroma_format_idc  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(&(sps->chroma_format_idc))) {
    return nullptr;
  }
  if (sps->chroma_format_idc == 3) {
    // separate_colour_plane_flag  u(1)
    if (!bit_buffer->ReadBits(&(sps->separate_colour_plane_flag), 1)) {
      return nullptr;
    }
  }

  // pic_width_in_luma_samples  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(&(sps->pic_width_in_luma_samples))) {
    return nullptr;
  }

  // pic_height_in_luma_samples  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(&(sps->pic_height_in_luma_samples))) {
    return nullptr;
  }

  // conformance_window_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->conformance_window_flag), 1)) {
    return nullptr;
  }

  if (sps->conformance_window_flag) {
    // conf_win_left_offset  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(&(sps->conf_win_left_offset))) {
      return nullptr;
    }
    // conf_win_right_offset  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(&(sps->conf_win_right_offset))) {
      return nullptr;
    }
    // conf_win_top_offset  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(&(sps->conf_win_top_offset))) {
      return nullptr;
    }
    // conf_win_bottom_offset  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(&(sps->conf_win_bottom_offset))) {
      return nullptr;
    }
  }

  // bit_depth_luma_minus8  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(&(sps->bit_depth_luma_minus8))) {
    return nullptr;
  }
  // bit_depth_chroma_minus8  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(&(sps->bit_depth_chroma_minus8))) {
    return nullptr;
  }
  // log2_max_pic_order_cnt_lsb_minus4  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(
          &(sps->log2_max_pic_order_cnt_lsb_minus4))) {
    return nullptr;
  }
  // sps_sub_layer_ordering_info_present_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->sps_sub_layer_ordering_info_present_flag),
                            1)) {
    return nullptr;
  }

  for (uint32_t i = (sps->sps_sub_layer_ordering_info_present_flag
                         ? 0
                         : sps->sps_max_sub_layers_minus1);
       i <= sps->sps_max_sub_layers_minus1; i++) {
    // sps_max_dec_pic_buffering_minus1[i]  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(&golomb_tmp)) {
      return nullptr;
    }
    sps->sps_max_dec_pic_buffering_minus1.push_back(golomb_tmp);
    // sps_max_num_reorder_pics[i]  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(&golomb_tmp)) {
      return nullptr;
    }
    sps->sps_max_num_reorder_pics.push_back(golomb_tmp);
    // sps_max_latency_increase_plus1[i]  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(&golomb_tmp)) {
      return nullptr;
    }
    sps->sps_max_latency_increase_plus1.push_back(golomb_tmp);
  }

  // log2_min_luma_coding_block_size_minus3  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(
          &(sps->log2_min_luma_coding_block_size_minus3))) {
    return nullptr;
  }

  // log2_diff_max_min_luma_coding_block_size  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(
          &(sps->log2_diff_max_min_luma_coding_block_size))) {
    return nullptr;
  }

  // log2_min_luma_transform_block_size_minus2  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(
          &(sps->log2_min_luma_transform_block_size_minus2))) {
    return nullptr;
  }

  // log2_diff_max_min_luma_transform_block_size  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(
          &(sps->log2_diff_max_min_luma_transform_block_size))) {
    return nullptr;
  }

  // max_transform_hierarchy_depth_inter  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(
          &(sps->max_transform_hierarchy_depth_inter))) {
    return nullptr;
  }

  // max_transform_hierarchy_depth_intra  ue(v)
  if (!bit_buffer->ReadExponentialGolomb(
          &(sps->max_transform_hierarchy_depth_intra))) {
    return nullptr;
  }

  // scaling_list_enabled_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->scaling_list_enabled_flag), 1)) {
    return nullptr;
  }

  if (sps->scaling_list_enabled_flag) {
    // sps_scaling_list_data_present_flag  u(1)
    if (!bit_buffer->ReadBits(&(sps->sps_scaling_list_data_present_flag), 1)) {
      return nullptr;
    }
    if (sps->sps_scaling_list_data_present_flag) {
      // scaling_list_data()
      // TODO(chemag): add support for scaling_list_data()
#ifdef FPRINT_ERRORS
      fprintf(stderr, "error: unimplemented scaling_list_data() in sps\n");
#endif  // FPRINT_ERRORS
      return nullptr;
    }
  }

  // amp_enabled_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->amp_enabled_flag), 1)) {
    return nullptr;
  }

  // sample_adaptive_offset_enabled_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->sample_adaptive_offset_enabled_flag), 1)) {
    return nullptr;
  }

  // pcm_enabled_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->pcm_enabled_flag), 1)) {
    return nullptr;
  }

  if (sps->pcm_enabled_flag) {
    // pcm_sample_bit_depth_luma_minus1  u(4)
    if (!bit_buffer->ReadBits(&(sps->pcm_sample_bit_depth_luma_minus1), 4)) {
      return nullptr;
    }

    // pcm_sample_bit_depth_chroma_minus1  u(4)
    if (!bit_buffer->ReadBits(&(sps->pcm_sample_bit_depth_chroma_minus1), 4)) {
      return nullptr;
    }

    // log2_min_pcm_luma_coding_block_size_minus3  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(
            &(sps->log2_min_pcm_luma_coding_block_size_minus3))) {
      return nullptr;
    }

    // log2_diff_max_min_pcm_luma_coding_block_size  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(
            &(sps->log2_diff_max_min_pcm_luma_coding_block_size))) {
      return nullptr;
    }

    // pcm_loop_filter_disabled_flag  u(1)
    if (!bit_buffer->ReadBits(&(sps->pcm_loop_filter_disabled_flag), 1)) {
      return nullptr;
    }
  }

  // num_short_term_ref_pic_sets
  if (!bit_buffer->ReadExponentialGolomb(&(sps->num_short_term_ref_pic_sets))) {
    return nullptr;
  }
  if (sps->num_short_term_ref_pic_sets >
      h265limits::NUM_SHORT_TERM_REF_PIC_SETS_MAX) {
#ifdef FPRINT_ERRORS
    fprintf(stderr,
            "error: sps->num_short_term_ref_pic_sets == %" PRIu32
            " > h265limits::NUM_SHORT_TERM_REF_PIC_SETS_MAX\n",
            sps->num_short_term_ref_pic_sets);
#endif  // FPRINT_ERRORS
    return nullptr;
  }

  for (uint32_t i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
    // st_ref_pic_set(i)
    sps->st_ref_pic_set.push_back(H265StRefPicSetParser::ParseStRefPicSet(
        bit_buffer, i, sps->num_short_term_ref_pic_sets));
  }

  // long_term_ref_pics_present_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->long_term_ref_pics_present_flag), 1)) {
    return nullptr;
  }

  if (sps->long_term_ref_pics_present_flag) {
    // num_long_term_ref_pics_sps  ue(v)
    if (!bit_buffer->ReadExponentialGolomb(
            &(sps->num_long_term_ref_pics_sps))) {
      return nullptr;
    }

    for (uint32_t i = 0; i < sps->num_long_term_ref_pics_sps; i++) {
      // lt_ref_pic_poc_lsb_sps[i] u(v)  log2_max_pic_order_cnt_lsb_minus4 + 4
      if (!bit_buffer->ReadBits(&bits_tmp,
                                sps->log2_max_pic_order_cnt_lsb_minus4 + 4)) {
        return nullptr;
      }
      sps->lt_ref_pic_poc_lsb_sps.push_back(bits_tmp);

      // used_by_curr_pic_lt_sps_flag[i]  u(1)
      if (!bit_buffer->ReadBits(&bits_tmp, 1)) {
        return nullptr;
      }
      sps->used_by_curr_pic_lt_sps_flag.push_back(bits_tmp);
    }
  }

  // sps_temporal_mvp_enabled_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->sps_temporal_mvp_enabled_flag), 1)) {
    return nullptr;
  }

  // strong_intra_smoothing_enabled_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->strong_intra_smoothing_enabled_flag), 1)) {
    return nullptr;
  }

  // vui_parameters_present_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->vui_parameters_present_flag), 1)) {
    return nullptr;
  }

  if (sps->vui_parameters_present_flag) {
    // vui_parameters()
    sps->vui_parameters =
        H265VuiParametersParser::ParseVuiParameters(bit_buffer);
  }

  // sps_extension_present_flag  u(1)
  if (!bit_buffer->ReadBits(&(sps->sps_extension_present_flag), 1)) {
    return nullptr;
  }

  if (sps->sps_extension_present_flag) {
    // sps_range_extension_flag  u(1)
    if (!bit_buffer->ReadBits(&(sps->sps_range_extension_flag), 1)) {
      return nullptr;
    }

    // sps_multilayer_extension_flag  u(1)
    if (!bit_buffer->ReadBits(&(sps->sps_multilayer_extension_flag), 1)) {
      return nullptr;
    }

    // sps_3d_extension_flag  u(1)
    if (!bit_buffer->ReadBits(&(sps->sps_3d_extension_flag), 1)) {
      return nullptr;
    }

    // sps_scc_extension_flag  u(1)
    if (!bit_buffer->ReadBits(&(sps->sps_scc_extension_flag), 1)) {
      return nullptr;
    }

    // sps_extension_4bits  u(4)
    if (!bit_buffer->ReadBits(&(sps->sps_extension_4bits), 4)) {
      return nullptr;
    }
  }

  if (sps->sps_range_extension_flag) {
    // sps_range_extension()
    // TODO(chemag): add support for sps_range_extension()
#ifdef FPRINT_ERRORS
    fprintf(stderr, "error: unimplemented sps_range_extension() in sps\n");
#endif  // FPRINT_ERRORS
    return nullptr;
  }
  if (sps->sps_multilayer_extension_flag) {
    // sps_multilayer_extension() // specified in Annex F
    // TODO(chemag): add support for sps_multilayer_extension()
#ifdef FPRINT_ERRORS
    fprintf(stderr, "error: unimplemented sps_multilayer_extension() in sps\n");
#endif  // FPRINT_ERRORS
    return nullptr;
  }

  if (sps->sps_3d_extension_flag) {
    // sps_3d_extension() // specified in Annex I
    // TODO(chemag): add support for sps_3d_extension()
#ifdef FPRINT_ERRORS
    fprintf(stderr, "error: unimplemented sps_3d_extension() in sps\n");
#endif  // FPRINT_ERRORS
    return nullptr;
  }

  if (sps->sps_scc_extension_flag) {
    // sps_scc_extension()
    // TODO(chemag): add support for sps_scc_extension()
#ifdef FPRINT_ERRORS
    fprintf(stderr, "error: unimplemented sps_scc_extension() in sps\n");
#endif  // FPRINT_ERRORS
    return nullptr;
  }

  if (sps->sps_extension_4bits) {
    while (more_rbsp_data(bit_buffer)) {
      // sps_extension_data_flag  u(1)
      if (!bit_buffer->ReadBits(&(sps->sps_extension_data_flag), 1)) {
        return nullptr;
      }
    }
  }

  rbsp_trailing_bits(bit_buffer);

  return sps;
}

#ifdef FDUMP_DEFINE
void H265SpsParser::SpsState::fdump(FILE* outfp, int indent_level) const {
  fprintf(outfp, "sps {");
  indent_level = indent_level_incr(indent_level);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_video_parameter_set_id: %i", sps_video_parameter_set_id);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_max_sub_layers_minus1: %i", sps_max_sub_layers_minus1);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_temporal_id_nesting_flag: %i",
          sps_temporal_id_nesting_flag);

  fdump_indent_level(outfp, indent_level);
  profile_tier_level->fdump(outfp, indent_level);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_seq_parameter_set_id: %i", sps_seq_parameter_set_id);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "chroma_format_idc: %i", chroma_format_idc);

  if (chroma_format_idc == 3) {
    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "separate_colour_plane_flag: %i",
            separate_colour_plane_flag);
  }

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "pic_width_in_luma_samples: %i", pic_width_in_luma_samples);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "pic_height_in_luma_samples: %i", pic_height_in_luma_samples);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "conformance_window_flag: %i", conformance_window_flag);

  if (conformance_window_flag) {
    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "conf_win_left_offset: %i", conf_win_left_offset);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "conf_win_right_offset: %i", conf_win_right_offset);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "conf_win_top_offset: %i", conf_win_top_offset);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "conf_win_bottom_offset: %i", conf_win_bottom_offset);
  }

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "bit_depth_luma_minus8: %i", bit_depth_luma_minus8);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "bit_depth_chroma_minus8: %i", bit_depth_chroma_minus8);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "log2_max_pic_order_cnt_lsb_minus4: %i",
          log2_max_pic_order_cnt_lsb_minus4);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_sub_layer_ordering_info_present_flag: %i",
          sps_sub_layer_ordering_info_present_flag);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_max_dec_pic_buffering_minus1 {");
  for (const uint32_t& v : sps_max_dec_pic_buffering_minus1) {
    fprintf(outfp, " %i", v);
  }
  fprintf(outfp, " }");

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_max_num_reorder_pics {");
  for (const uint32_t& v : sps_max_num_reorder_pics) {
    fprintf(outfp, " %i", v);
  }
  fprintf(outfp, " }");

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_max_latency_increase_plus1 {");
  for (const uint32_t& v : sps_max_latency_increase_plus1) {
    fprintf(outfp, " %i", v);
  }
  fprintf(outfp, " }");

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "log2_min_luma_coding_block_size_minus3: %i",
          log2_min_luma_coding_block_size_minus3);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "log2_diff_max_min_luma_coding_block_size: %i",
          log2_diff_max_min_luma_coding_block_size);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "log2_min_luma_transform_block_size_minus2: %i",
          log2_min_luma_transform_block_size_minus2);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "log2_diff_max_min_luma_transform_block_size: %i",
          log2_diff_max_min_luma_transform_block_size);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "max_transform_hierarchy_depth_inter: %i",
          max_transform_hierarchy_depth_inter);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "max_transform_hierarchy_depth_intra: %i",
          max_transform_hierarchy_depth_intra);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "scaling_list_enabled_flag: %i", scaling_list_enabled_flag);

  if (scaling_list_enabled_flag) {
    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "sps_scaling_list_data_present_flag: %i",
            sps_scaling_list_data_present_flag);

    if (sps_scaling_list_data_present_flag) {
      // scaling_list_data()
      // TODO(chemag): add support for scaling_list_data()
      fprintf(stderr, "error: unimplemented scaling_list_data() in sps\n");
    }
  }

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "amp_enabled_flag: %i", amp_enabled_flag);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sample_adaptive_offset_enabled_flag: %i",
          sample_adaptive_offset_enabled_flag);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "pcm_enabled_flag: %i", pcm_enabled_flag);

  if (pcm_enabled_flag) {
    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "pcm_sample_bit_depth_luma_minus1: %i",
            pcm_sample_bit_depth_luma_minus1);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "pcm_sample_bit_depth_chroma_minus1: %i",
            pcm_sample_bit_depth_chroma_minus1);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "log2_min_pcm_luma_coding_block_size_minus3: %i",
            log2_min_pcm_luma_coding_block_size_minus3);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "log2_diff_max_min_pcm_luma_coding_block_size: %i",
            log2_diff_max_min_pcm_luma_coding_block_size);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "pcm_loop_filter_disabled_flag: %i",
            pcm_loop_filter_disabled_flag);
  }

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "num_short_term_ref_pic_sets: %i",
          num_short_term_ref_pic_sets);

  for (uint32_t i = 0; i < num_short_term_ref_pic_sets; i++) {
    fdump_indent_level(outfp, indent_level);
    st_ref_pic_set[i]->fdump(outfp, indent_level);
  }

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "long_term_ref_pics_present_flag: %i",
          long_term_ref_pics_present_flag);

  if (long_term_ref_pics_present_flag) {
    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "num_long_term_ref_pics_sps: %i",
            num_long_term_ref_pics_sps);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "lt_ref_pic_poc_lsb_sps {");
    for (const uint32_t& v : lt_ref_pic_poc_lsb_sps) {
      fprintf(outfp, " %i", v);
    }
    fprintf(outfp, " }");

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "used_by_curr_pic_lt_sps_flag {");
    for (const uint32_t& v : used_by_curr_pic_lt_sps_flag) {
      fprintf(outfp, " %i", v);
    }
    fprintf(outfp, " }");
  }

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_temporal_mvp_enabled_flag: %i",
          sps_temporal_mvp_enabled_flag);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "strong_intra_smoothing_enabled_flag: %i",
          strong_intra_smoothing_enabled_flag);

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "vui_parameters_present_flag: %i",
          vui_parameters_present_flag);

  fdump_indent_level(outfp, indent_level);
  if (vui_parameters_present_flag) {
    vui_parameters->fdump(outfp, indent_level);
  }

  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "sps_extension_present_flag: %i", sps_extension_present_flag);

  if (sps_extension_present_flag) {
    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "sps_range_extension_flag: %i", sps_range_extension_flag);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "sps_multilayer_extension_flag: %i",
            sps_multilayer_extension_flag);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "sps_3d_extension_flag: %i", sps_3d_extension_flag);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "sps_scc_extension_flag: %i", sps_scc_extension_flag);

    fdump_indent_level(outfp, indent_level);
    fprintf(outfp, "sps_extension_4bits: %i", sps_extension_4bits);
  }

  if (sps_range_extension_flag) {
    // sps_range_extension()
    // TODO(chemag): add support for sps_range_extension()
    fprintf(stderr, "error: unimplemented sps_range_extension() in sps\n");
  }

  if (sps_multilayer_extension_flag) {
    // sps_multilayer_extension() // specified in Annex F
    // TODO(chemag): add support for sps_multilayer_extension()
    fprintf(stderr,
            "error: unimplemented sps_multilayer_extension_flag() in "
            "sps\n");
  }

  if (sps_3d_extension_flag) {
    // sps_3d_extension() // specified in Annex I
    // TODO(chemag): add support for sps_3d_extension()
    fprintf(stderr, "error: unimplemented sps_3d_extension_flag() in sps\n");
  }

  if (sps_scc_extension_flag) {
    // sps_scc_extension()
    // TODO(chemag): add support for sps_scc_extension()
    fprintf(stderr, "error: unimplemented sps_scc_extension_flag() in sps\n");
  }

  indent_level = indent_level_decr(indent_level);
  fdump_indent_level(outfp, indent_level);
  fprintf(outfp, "}");
}
#endif  // FDUMP_DEFINE

uint32_t H265SpsParser::SpsState::getPicSizeInCtbsY() {
  // PicSizeInCtbsY support (page 77)
  // Equation (7-10)
  uint32_t MinCbLog2SizeY = log2_min_luma_coding_block_size_minus3 + 3;
  // Equation (7-11)
  uint32_t CtbLog2SizeY =
      MinCbLog2SizeY + log2_diff_max_min_luma_coding_block_size;
  // Equation (7-12)
  // uint32_t MinCbSizeY = 1 << MinCbLog2SizeY;
  // Equation (7-13)
  uint32_t CtbSizeY = 1 << CtbLog2SizeY;
  // Equation (7-14)
  // uint32_t PicWidthInMinCbsY = pic_width_in_luma_samples / MinCbSizeY;
  // Equation (7-15)
  uint32_t PicWidthInCtbsY =
      static_cast<uint32_t>(std::ceil(pic_width_in_luma_samples / CtbSizeY));
  // Equation (7-16)
  // uint32_t PicHeightInMinCbsY = pic_height_in_luma_samples / MinCbSizeY;
  // Equation (7-17)
  uint32_t PicHeightInCtbsY =
      static_cast<uint32_t>(std::ceil(pic_height_in_luma_samples / CtbSizeY));
  // Equation (7-18)
  // uint32_t PicSizeInMinCbsY = PicWidthInMinCbsY * PicHeightInMinCbsY;
  // Equation (7-19)
  uint32_t PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;
  // PicSizeInSamplesY = pic_width_in_luma_samples *
  //     pic_height_in_luma_samples (7-20)
  // PicWidthInSamplesC = pic_width_in_luma_samples / SubWidthC (7-21)
  // PicHeightInSamplesC = pic_height_in_luma_samples / SubHeightC (7-22)
  return PicSizeInCtbsY;
}

}  // namespace h265nal
