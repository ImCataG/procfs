# procfs
[![forthebadge](https://forthebadge.com/images/badges/made-with-c-plus-plus.svg)](https://forthebadge.com)

This package uses `ps` to retrieve information about all running processes, then uses `FUSE` to create a tree-like pseudo-filesystem. Every directory also contains a `stats` file with statistics about the process.

# build
You may need to install `libfuse-dev`.
`make`

# run
`procfs folderToMountIn`
