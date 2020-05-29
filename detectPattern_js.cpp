/* 
    This file contains definitions for interfaces to the functions which are
    exposed to WASM. After setting the image buffer, the detect function 
    runs a quad detector and finds the quad that can potentially have the 
    led blinking within it. Next step is to send the status of the LED to WASM.
*/

#include <iostream>
#include <stdlib.h>
#include <float.h>
#include <string.h>

#include "opencv2/opencv.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "emscripten.h" 

using namespace std;
using namespace cv;

#define BLINK_PATTERN (uint32_t) 0xC93
// maximum size of string for each detection. 
#define STR_DET_LEN 350

extern "C"{
EMSCRIPTEN_KEEPALIVE bool check_led_status(Mat);
EMSCRIPTEN_KEEPALIVE bool check_if_pattern_exists(uint32_t);
EMSCRIPTEN_KEEPALIVE uint8_t* detect();
EMSCRIPTEN_KEEPALIVE int destroy();
EMSCRIPTEN_KEEPALIVE uint8_t * create_buffer(int byte_size);
}

// The ROI coordinates within our quad (LED's region)
double x1_,y1_,x2_,y2_ = 0;
// size oand stride f the image to process
int g_width;
int g_height;

// pointer to the image grayscale pixes
uint8_t *g_img_buf = NULL;

// Two set of lookup tables to compare the input number
uint32_t compare_0_codes[6] = {0x24F,0x49E,0x279,0x4F2,0x3C9,0x792};
uint32_t compare_1_codes[6] = {0xC93,0x93C,0x927,0x9E4,0xF24,0xE49};

// json format string for the 4 points
const char fmt_det_point[] = "{\"corners\": [{\"x\":%d,\"y\":%d},{\"x\":%d,\"y\":%d},{\"x\":%d,\"y\":%d},{\"x\":%d,\"y\":%d}] }";

/**
 * @brief Detects the quad and returns the position of the quad in the image
 * 
 * @return the pointer to the quad detection points buffer
 */

extern "C"{
    EMSCRIPTEN_KEEPALIVE
    uint8_t* detect()
    {
        // Convert the image buffer to a CV matrix to support CV functions
        Mat gray(Size(g_width, g_height), CV_8UC1, g_img_buf, Mat::AUTO_STEP);
        Mat cam_frame = gray;
        // Perform Gaussian blur with kernel size 5 to reduce image noise
        GaussianBlur(gray, gray, Size(5, 5), 0);
        // Thresholding to detect bright spots
        threshold(gray,gray, 80, 255, THRESH_BINARY);

        // Quad Detection code 
        vector<vector<Point> > contours;
        // Detect contours . RETR_LIST returns all polygons
        findContours(gray, contours, RETR_LIST, CHAIN_APPROX_NONE );
        // No contours detected. 
        if(contours.size()==0)
        {
            return (uint8_t *)0;
        }

        vector<vector<Point> > contours_poly( contours.size() );
        vector<Rect> boundRect( contours.size());
        vector<Rect> boundRect_cp( contours.size());

        // Holds the detection points of the quad 
        // source : https://github.com/conix-center/AR.js/tree/master/three.js/vendor/js-apriltag/emscripten

        int str_det_len = contours.size() * STR_DET_LEN;
        int *buffer =(int *) malloc(str_det_len + sizeof(int32_t));
        char *str_det = ((char * ) buffer) + sizeof(int32_t);
        char *str_tmp_det = (char * )malloc(STR_DET_LEN);
        int llen = str_det_len - 1;
        strcpy(str_det, "[ ");
        llen -= 2; //"[ "

        int largestContourIdx = -1;
        float largestContourArea = 0.0;
        /* Contour detection can pick up many contours hence pick the 
        largest contour */
        for( size_t i = 0; i < contours.size(); i++ )
        {
            // Group all the polynomials detected
            double peri = arcLength(contours[i],true);
            approxPolyDP( contours[i], contours_poly[i],  0.05*peri, true );
            // If it is a 4 sided figure
            if(contours_poly[i].size()==4)
            {
                float ctArea= cv::contourArea(contours_poly[i]);
                /* Setting watermarks to reduce false detections */
                    
                if(ctArea >=g_width*g_height*0.5 || ctArea <=g_width*g_height*0.01)
                    continue;
                if(ctArea>largestContourArea)
                {
                    largestContourArea = ctArea ;
                    
                }
                
                boundRect_cp[i] = boundingRect( contours_poly[i] );
                double aspect_ratio = (double)boundRect_cp[i].width/(double)boundRect_cp[i].height;
                // Aspect ratio ensures that the 4 sided figure is a rectangle
                if(aspect_ratio<0.95 || aspect_ratio >1.05)
                {
                    if(largestContourArea == ctArea)
                        largestContourIdx = i;
                    boundRect[i] = boundRect_cp[i];
                }

            }
            
        }
        // Check if quad exist, or else return empty buffer
        int idx = largestContourIdx;
        if(idx>=0)
        {
            int c;

            c = snprintf(str_tmp_det, STR_DET_LEN, fmt_det_point,boundRect[idx].x, 
                        boundRect[idx].y, boundRect[idx].x+ boundRect[idx].width, 
                        boundRect[idx].y, boundRect[idx].x+ boundRect[idx].width, 
                        boundRect[idx].y+boundRect[idx].height, boundRect[idx].x, 
                        boundRect[idx].y+boundRect[idx].height);

            // coordinates of the led is within the quad
            x1_ = boundRect[idx].x + boundRect[idx].width*0.3;
            y1_ = boundRect[idx].y + boundRect[idx].height*0.3;
            x2_ = boundRect[idx].x + boundRect[idx].width*0.8;
            y2_ = boundRect[idx].y + boundRect[idx].height*0.8;
            
            strncat(str_det, str_tmp_det, llen);
            llen -= c;
            free(str_tmp_det);
            strncat(str_det, " ]", llen);
            str_det[str_det_len - 1] = '\0'; // make sure it is null-terminated

            buffer[0] = strlen(str_det);
            buffer[0] = (buffer[0] << 1) | 0;
            //return (uint8_t *)buffer;

        }
        else
        {
            return (uint8_t *)0;
        }
        
        
        /* On quad detection , the below code checks if the led within the
            quad is ON/OFF.   */
        if(x1_>0 && y1_>0 && x2_ > 0 && y2_ > 0)
        {
            Rect myRoi(x1_,y1_,abs(x2_-x1_),abs(y2_-y1_));

            if(myRoi.x >= 0 && myRoi.y >= 0 && myRoi.width + myRoi.x < 
            cam_frame.cols && myRoi.height + myRoi.y < cam_frame.rows
            && myRoi.height>0 && myRoi.width>0)
            {
                Mat cropped_img = cam_frame(myRoi);
                // Threshold value. Pixels less than 185 intensity are discarded
                double thresh = 185;
                double maxValue = 255;
                
                Mat dst;
                
                // Binary thresholing to detect bright spots in the image
                threshold(cropped_img,dst, thresh, maxValue, THRESH_BINARY);
                // Update the led status of current frame
                bool res = check_led_status(dst);
                buffer[0] = buffer[0] | res;
                return (uint8_t *)buffer;
            }
        }
        return (uint8_t *)buffer;
    }
}

/**
 * @brief Checks if the led within the quad is on/off by thresholding
 * 
 * @return bool value indicating status
 */

extern "C"{
    EMSCRIPTEN_KEEPALIVE
    bool check_led_status(Mat thresh_img )
    {    
        vector<vector<Point> > contours;
        findContours(thresh_img, contours, RETR_TREE, CHAIN_APPROX_SIMPLE );
        /* RETR_TREE is used for detecting shapes within quad, while also
            the frame is detected as a contour */
        if(contours.size()==2)
        {
            return 1;
        }
        else
        {
            return 0;
        }   

    } 
}

/* Contains helper functions to expose native code to WASM land
* credits: @nampereira : 
https://github.com/conix-center/AR.js/tree/master/three.js/vendor/js-apriltag/emscripten
*/

/**
 * @brief Creates/changes size of the image buffer where we receive the images to process
 *
 * @param width Width of the image
 * @param height Height of the image
 * @param stride How many pixels per row (=width typically)
 * 
 * @return the pointer to the image buffer 
 *
 * @warning caller of detect is responsible for putting *grayscale* image pixels in this buffer
 */
extern "C"{
EMSCRIPTEN_KEEPALIVE
uint8_t * set_img_buffer(int width, int height, int stride) {
  if (g_img_buf != NULL) {
    if (g_width == width && g_height == height) return g_img_buf;
    free(g_img_buf);
    g_width = width;
    g_height = height;
    g_img_buf = (uint8_t *) malloc(width * height);
  } else {
    g_width = width;
    g_height = height;
    g_img_buf = (uint8_t *) malloc(width * height);
  }
  return g_img_buf;
}
}
/**
 * @brief Releases resources
 *
 * @return 0=success
 */
extern "C"{
EMSCRIPTEN_KEEPALIVE
int destroy() {
  if (g_img_buf != NULL) free(g_img_buf);
  return 0;
}
}
/* Compares the given number to the combination of pattern that blink pattern 
    can be encoded. If one such pattern is found, the pattern is printed */

extern "C"{
EMSCRIPTEN_KEEPALIVE
bool check_if_pattern_exists(uint32_t num)
{
    for(int i=0;i<6;i++)
    {
        if(compare_0_codes[i]==num)
        {
            return 1;
        }
        else if(compare_1_codes[i]==num)
        {
            return 1;
        }
    }
    return 0;
    
}
}
/**
 * @brief Allocates memory for 'bytes_size' bytes
 *
 * @param bytes_size How many bytes to allocate
 * 
 * @return pointer allocated
 */
extern "C"{
EMSCRIPTEN_KEEPALIVE
uint8_t * create_buffer(int byte_size) {
  return (uint8_t *)malloc(byte_size);
}
}