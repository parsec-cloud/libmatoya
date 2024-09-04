`libmatoya` is a cross-platform application development library.

This branch exists to try to figure out what's going on with OpenGL.

Install `gcc` and `make`, and run `make` to build the library and the `1-draw` test.
The test will automatically run when built.

You can alternatively run `make DEBUG=1` to compile the library and test with debug information.

This branch is based on `stable`, and effectively just turns `make` into `make && cd test && make 1-draw`,
modifying the `1-draw` test to use the OpenGL renderer instead of the default Vulkan.

If the test works, you will see a very distorted Final Fantasy image on a black background (due to bad image decoding).
If the test does not work, the image will be missing.
