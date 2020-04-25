Voxel-Carving
=============

**Shape-from-Sihouette / Visual Hull Intersection**

Straight forward implementation of a 3d reconstruction technique called voxel carving (or space carving).

This is a modified of the Voxel-Carving algorithm for use with COLMAP output file (See [this page](https://colmap.github.io/format.html)).

**Git-clone the repo**

**Build docker image**
```bash
docker build -t kai46/voxel-carving .
```

**Launch a docker container (modify -v for your own purposes)**
```bash
docker run \
    -ti --rm \
    --hostname="voxel-carving" \
    -v /phoenix:/phoenix \
    kai46/voxel-carving:latest \
    bash
```

**Run voxel-carving in the container**
```bash
voxel-carving ./example/ 50 ./carved_shape.ply
```

**See help**
```bash
voxel-carving -h
```

