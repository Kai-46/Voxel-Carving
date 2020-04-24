Voxel-Carving
=============

Straight forward implementation of a 3d reconstruction technique called voxel carving (or space carving).

This is a modified of the Voxel-Carving algorithm for use with COLMAP output file.


# Build docker image
docker build -t kai46/voxel-carving .

# Launch a docker container (modify -v for your own purposes)
docker run \
    -ti --rm \
    -u $(id -u) \
    --userns="host" \
    --ipc="host" \
    --hostname="voxel-carving" \
    -v /phoenix:/phoenix \
    kai46/voxel-carving:latest \
    bash
