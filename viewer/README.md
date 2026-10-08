# Viewer

Reserved for roadmap phase 6: the three.js viewer rebuilt on the engine's WebAssembly build, rendering the simulator's
own water volumes (column fills, then a raymarched volume from a voxel map of the spaces) with a timeline scrubber,
deck plans, scenario editing and ensemble bands.

Until then the working viewer is the oracle's: open `oracle/js/out/titanic.html` in a browser (three.js and the fonts
load from CDNs), or rebuild it with `node oracle/js/tools/build.js`. It runs the JavaScript core directly and reads
only snapshots of it, which is the boundary the WebAssembly build will keep.

![The oracle viewer four minutes after the collision](../docs/media/viewer-titanic-1912.png)
