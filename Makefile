#Add prebuilt opencv wasm support libraries (typically found in opencv/platforms/js/build_wasm/lib
LDFLAGS = -LPath_to_opencv_wasm_lib -llibopencv_video -llibopencv_core -llibopencv_objdetect -llibopencv_features2d -llibopencv_imgproc -llibopencv_calib3d
#Add opencv include path.
INC = -IPath_to_OpenCV-3.4.4/include/

detectPattern.js: detectPattern_js.cpp
	emcc -Os $(LDFLAGS) $(INC) -s MODULARIZE=1 -s USE_ZLIB=1 -s 'EXPORT_NAME="LedPatternWasm"' -s WASM=1 -IdetectPattern -s ALLOW_MEMORY_GROWTH=1 -s EXTRA_EXPORTED_RUNTIME_METHODS='["cwrap", "getValue", "setValue"]' -o $@ $^
clean:
	rm detectPattern.js
	rm *.wasm
