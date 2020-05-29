# ledPatternWasm
The detect_pat.html grabs frames from the camera and sends it to the native 
C++ code through WASM.
The detect() function defined in the detectPattern_js.cpp currently detects a 
quad in the frame and returns the quad positions back to the caller along with
the status of the led in the first bit of the buffer.

PENDING WORK : 

Currently the algorithm was tested with a gif video running on a smartphone.
The smartphone's display saturates the image. The algorithm running on the 
native C++ has to be tested with a better test setup.

The blinking pattern is currently hardcoded with the value 0xC93 in the native
code i.e a 12 bit pattern. After accumulating the 12 bits in the web application
front, check_if_pattern_exists(uint32_t num) can be called with the accumulated
number to check if the pattern exists.

The blinking pattern video is currently configured to change its state every
30 ms. Hence the fps has to be atleast 30 to detect the pattern and 60 for odd/
even sampling to compensate for the camera saturation.

Also, thresholding values are currently hardcoded which seems to work well.
Adaptive thresholding needs to be researched. 

Thanks to @nampereira's contribution to the CONIX repository, 
https://github.com/conix-center/AR.js/blob/master/three.js/vendor/js-apriltag/
from which the webassembly code was taken from and modified accordingly.
