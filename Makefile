# Include wasm library before including opencv libs. typically in opencv/platforms/js/build_wasm/lib
LDFLAGS = -llibopencv_video -llibopencv_core -llibopencv_objdetect -llibopencv_features2d -llibopencv_imgproc -llibopencv_calib3d
#Include OpenCV include path OpenCV-3.x.x/include/
INC =

detectPattern.js: detectPattern_js.cpp
	emcc -Os $(LDFLAGS) $(INC) -s MODULARIZE=1 -s USE_ZLIB=1 -s 'EXPORT_NAME="LedPatternWasm"' -s WASM=1 -IdetectPattern -s ALLOW_MEMORY_GROWTH=1 -s EXTRA_EXPORTED_RUNTIME_METHODS='["cwrap", "getValue", "setValue"]' -o $@ $^

clean:
	rm detectPattern.js
	rm *.wasm
	
