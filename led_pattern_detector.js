/* This is a wrapper class that calls detectPattern_wasm to load the WASM module and wraps the c++ implementation calls. 
    source : https://github.com/conix-center/AR.js/tree/master/three.js/vendor/js-apriltag/emscripten
*/

class LedPatternDetector {

onWasmInit(Module) {
    // save a reference to the module here
    this._Module = Module;
    // Creates/changes size of the image buffer where we receive the images to process
    this._set_img_buffer = Module.cwrap('set_img_buffer', 'number', ['number', 'number', 'number']);

    //returns pointer to buffer starting with an int32 indicating the size of 
    //the remaining buffer (a string of chars with the json describing the detections)
    this._detect = Module.cwrap('detect', 'number', []);

    //int destroy(); Releases resources allocated by the wasm module
    this._destroy = Module.cwrap('destroy', 'number', []);

    // convenience function
    this._destroy_buffer = function(buf_ptr) {
        this._Module._free(buf_ptr);
    }
    this.onWasmLoadedCallback();
}

constructor(onWasmLoadedCallback) {
    this.onWasmLoadedCallback = onWasmLoadedCallback;
    let _this = this;
    LedPatternWasm().then(function(Module) {
        console.log("LedPATTERN WASM module loaded.");
        _this.onWasmInit(Module);
    });
}

// **public** detect method
detect(grayscaleImg, imgWidth, imgHeight) {
   //console.log("img buffer set"); 
    // set_img_buffer allocates the buffer for image and returns it; 
    // just returns the previously allocated buffer if size has not changed
    let imgBuffer = this._set_img_buffer(imgWidth, imgHeight, imgWidth); 
    this._Module.HEAPU8.set(grayscaleImg, imgBuffer); // copy grayscale image data
    let detectionsBuffer = this._detect();
    console.log("Detect Called"); 
    if (detectionsBuffer == 0) { // returned NULL
        this._destroy_buffer(detectionsBuffer);
        return [];
    }
    let detectionsBufferSize = this._Module.getValue(detectionsBuffer, "i32");
    let led_status = 1;
    
    led_status = led_status & detectionsBufferSize;
    console.log("led status %d",led_status); 
    detectionsBufferSize = detectionsBufferSize >> 1;
    console.log("buffer size %d",detectionsBufferSize); 
    if (detectionsBufferSize == 0) { // returned zero detections
        this._destroy_buffer(detectionsBuffer);
        return [];
    }
    const resultView = new Uint8Array(this._Module.HEAP8.buffer, detectionsBuffer + 4, detectionsBufferSize);
    let detectionsJson = '';
    // detectionsBufferSize is always 1 for now
    for (let i = 0; i < detectionsBufferSize; i++) {
        detectionsJson += String.fromCharCode(resultView[i]);
    }
    this._destroy_buffer(detectionsBuffer);
    let detections = JSON.parse(detectionsJson);
    
    console.log("Led Status is %d",led_status); 

    return [detections,led_status];
}

}