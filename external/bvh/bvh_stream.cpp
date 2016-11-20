// Copyright (c) 2014 Bauhaus-Universitaet Weimar
// This Software is distributed under the Modified BSD License, see license.txt.
//
// Virtual Reality and Visualization Research Group 
// Faculty of Media, Bauhaus-Universitaet Weimar
// http://www.uni-weimar.de/medien/vr

#include "bvh_stream.h"

namespace vklod {

bvh_stream::
bvh_stream()
: filename_(""),
  num_segments_(0) {


}

bvh_stream::
~bvh_stream() {
    close_stream(false);
}

void bvh_stream::
open_stream(const std::string& bvh_filename,
            const bvh_stream_type type) {

    close_stream(false);
    
    num_segments_ = 0;
    filename_ = bvh_filename;
    type_ = type;

    std::ios::openmode mode = std::ios::binary;

    if (type_ == bvh_stream_type::BVH_STREAM_IN) {
        mode |= std::ios::in;
    }
    if (type_ == bvh_stream_type::BVH_STREAM_OUT) {
        mode |= std::ios::out;
        mode |= std::ios::trunc;
    }

    file_.open(filename_, mode);

    if (!file_.is_open() || !file_.good()) {
        throw std::runtime_error(
            "vklod: bvh_stream::Unable to open stream: " + filename_);
    }
   
}

void bvh_stream::
close_stream(const bool remove_file) {
    
    if (file_.is_open()) {
        if (type_ == bvh_stream_type::BVH_STREAM_OUT) {
            file_.flush();
        }
        file_.close();
        if (file_.fail()) {
            throw std::runtime_error(
                "vklod: bvh_stream::Unable to close stream: " + filename_);
        }

        if (type_ == bvh_stream_type::BVH_STREAM_OUT) {
            if (remove_file) {
                if (std::remove(filename_.c_str())) {
                    throw std::runtime_error(
                        "vklod: bvh_stream::Unable to delete file: " + filename_);
                }

            }
        }
    }

}

void bvh_stream::
read_bvh(const std::string& filename, bvh& bvh) {
 
    open_stream(filename, bvh_stream_type::BVH_STREAM_IN);

    if (type_ != BVH_STREAM_IN) {
        throw std::runtime_error(
            "vklod: bvh_stream::Failed to read bvh from: " + filename_);
    }
    if (!file_.is_open()) {
         throw std::runtime_error(
            "vklod: bvh_stream::Failed to read bvh from: " + filename_);
    }
   
    //scan stream
    file_.seekg(0, std::ios::end);
    size_t filesize = (size_t)file_.tellg();
    file_.seekg(0, std::ios::beg);

    num_segments_ = 0;

    bvh_tree_seg tree;
    bvh_tree_extension_seg tree_ext;
    std::vector<bvh_node_seg> nodes;
    std::vector<bvh_node_extension_seg> nodes_ext;
    uint32_t tree_id = 0;
    uint32_t tree_ext_id = 0;
    uint32_t node_id = 0;
    uint32_t node_ext_id = 0;


    //go through entire stream and fetch the segments
    while (true) {
        bvh_sig sig;
        sig.deserialize(file_);
        if (sig.signature_[0] != 'B' ||
            sig.signature_[1] != 'V' ||
            sig.signature_[2] != 'H' ||
            sig.signature_[3] != 'X') {
             throw std::runtime_error(
                 "vklod: bvh_stream::Invalid magic encountered: " + filename_);
        }
            
        size_t anchor = (size_t)file_.tellg();

        switch (sig.signature_[4]) {

            case 'F': { //"BVHXFILE"
                bvh_file_seg seg;
                seg.deserialize(file_);
                break;
            }
            case 'T': { 
                switch (sig.signature_[5]) {
                    case 'R': { //"BVHXTREE"
                        tree.deserialize(file_);
                        ++tree_id;
                        break;
                    }
                    case 'E': { //"BVHXTEXT"
                        tree_ext.deserialize(file_);
                        ++tree_ext_id;
                        break;                     
                    }
                    default: {
                        throw std::runtime_error(
                            "vklod: bvh_stream::Stream corrupt -- Invalid segment encountered");
                        break;
                    }
                }
                break;
            }
            case 'N': { 
                switch (sig.signature_[5]) {
                    case 'O': { //"BVHXNODE"
                        bvh_node_seg node;
                        node.deserialize(file_);
                        nodes.push_back(node);
                        if (node_id != node.node_id_) {
                            throw std::runtime_error(
                                "vklod: bvh_stream::Stream corrupt -- Invalid node order");
                        }
                        ++node_id;
                        break;
                    }
                    case 'E': { //"BVHXNEXT"
                        bvh_node_extension_seg node_ext;
                        node_ext.deserialize(file_);
                        nodes_ext.push_back(node_ext);
                        if (node_ext_id != node_ext.node_id_) {
                            throw std::runtime_error(
                                "vklod: bvh_stream::Stream corrupt -- Invalid node extension order");
                        }
                        ++node_ext_id;
                        break;
                    }
                    default: {
                        throw std::runtime_error(
                            "vklod: bvh_stream::Stream corrupt -- Invalid segment encountered");
                        break;
                    }
                }
                break;
            }
            default: {
                throw std::runtime_error(
                    "vklod: bvh_stream::file corrupt -- Invalid segment encountered");
                break;
            }
        }

        if (anchor + sig.allocated_size_ < filesize) {
            file_.seekg(anchor + sig.allocated_size_, std::ios::beg);
        }
        else {
            break;
        }

    }

    close_stream(false);

    if (tree_id != 1) {
       throw std::runtime_error(
           "vklod: bvh_stream::Stream corrupt -- Invalid number of bvh segments");
    }   

    if (tree_ext_id > 1) {
       throw std::runtime_error(
           "vklod: bvh_stream::Stream corrupt -- Invalid number of bvh extensions");
    }    

    //Note: this is the rendering library version of the file reader!

    bvh.set_depth(tree.depth_);
    bvh.set_num_nodes(tree.num_nodes_);
    bvh.set_fan_factor(tree.fan_factor_);
    bvh.set_primitives_per_node(tree.max_surfels_per_node_);
    bvh.set_size_of_primitive(tree.serialized_surfel_size_);
    bvh.set_primitive((bvh::primitive_type)tree.primitive_);
    vec3r_t translation(tree.translation_.x_,
                        tree.translation_.y_,
                        tree.translation_.z_);
    bvh.set_translation(translation);

    if (bvh.get_num_nodes() != node_id) {
       throw std::runtime_error(
           "vklod: bvh_stream::Stream corrupt -- Ivalid number of node segments");
    }

    for (const auto& node : nodes) {
       vec3r_t centroid(node.centroid_.x_,
                        node.centroid_.y_,
                        node.centroid_.z_);
       bvh.set_centroid(node.node_id_, centroid);
       bvh.set_avg_primitive_extent(node.node_id_, node.avg_surfel_radius_);
       bvh.set_visibility(node.node_id_, (bvh::node_visibility)node.visibility_);
       vec3r_t box_min(node.bounding_box_.min_.x_,
                       node.bounding_box_.min_.y_,
                       node.bounding_box_.min_.z_);
       vec3r_t box_max(node.bounding_box_.max_.x_,
                       node.bounding_box_.max_.y_,
                       node.bounding_box_.max_.z_);
       bvh.set_bounding_box(node.node_id_, vklod::bounding_box_t(box_min, box_max));
   
    }

}

} // namespace vklod

