#!/usr/bin/env sh
$CAFFE_ROOT/build/tools/compute_image_mean ./caffe/dataset/train_lmdb ./caffe/dataset/rl_gazebo_mean.binaryproto

echo "Done."
