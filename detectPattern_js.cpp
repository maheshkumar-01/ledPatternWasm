/* Copyright (C) 2013-2016, The Regents of The University of Michigan.
All rights reserved.
This software was developed in the APRIL Robotics Lab under the
direction of Edwin Olson, ebolson@umich.edu. This software may be
available under alternative licensing terms; contact the address above.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the Regents of The University of Michigan.
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
//#include "opencv_demo.hpp"




using namespace std;
using namespace cv;
using namespace std::chrono;

#define BLINK_PATTERN (uint32_t) 0xC93
// maximum size of string for each detection
#define STR_DET_LEN 350

// Random border colour generator for contour detection
RNG rng(12345);
extern "C"{
EMSCRIPTEN_KEEPALIVE bool update_led_status(Mat);
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

extern "C"{
EMSCRIPTEN_KEEPALIVE
uint8_t* detect()
{
        
        Mat cam_frame(Size(g_width, g_height), CV_8UC1, g_img_buf, Mat::AUTO_STEP);
        printf("\n image dim %d %d",g_width,g_height);
        Mat gray = cam_frame;
        //cv::imwrite( "cam_frame.jpg", cam_frame );
        GaussianBlur(gray, gray, Size(5, 5), 0);
        threshold(gray,gray, 80, 255, THRESH_BINARY);
        //Convert the frame to greyscale
        //cvtColor(cam_frame, gray, COLOR_BGR2GRAY);

        // Quad Detection code 
        vector<vector<Point> > contours;
        
        findContours(gray, contours, RETR_LIST, CHAIN_APPROX_NONE );
        //printf("\n Detect called in native");
        if(contours.size()==0)
        {
            return (uint8_t *)0;
        }
        vector<vector<Point> > contours_poly( contours.size() );
        vector<Rect> boundRect( contours.size());
        vector<Rect> boundRect_cp( contours.size());

        int str_det_len = contours.size() * STR_DET_LEN;
        int *buffer =(int *) malloc(str_det_len + sizeof(int32_t));
        char *str_det = ((char * ) buffer) + sizeof(int32_t);
        char *str_tmp_det = (char * )malloc(STR_DET_LEN);
        int llen = str_det_len - 1;
        strcpy(str_det, "[ ");
        llen -= 2; //"[ "

        
        Mat drawing = Mat::zeros(gray.size(), CV_8UC3 );
        int smallestContourIdx = -1;
        //float smallestContourArea = FLT_MAX;
        float largestContourArea = 0.0;
        /* Contour detection can pick up frame boundary as a contour, so pick
            the smallest bounding box */
        for( size_t i = 0; i < contours.size(); i++ )
        {
            double peri = arcLength(contours[i],true);
            approxPolyDP( contours[i], contours_poly[i],  0.05*peri, true );
            if(contours_poly[i].size()==4)
            {
                float ctArea= cv::contourArea(contours_poly[i]);
                if(ctArea >=g_width*g_height*0.5 || ctArea <=g_width*g_height*0.01)
                    continue;
                if(ctArea>largestContourArea)
                {
                    printf("\n Contour area  %f ",ctArea);
                    largestContourArea = ctArea ;
                    
                }
                
                boundRect_cp[i] = boundingRect( contours_poly[i] );
                double aspect_ratio = (double)boundRect_cp[i].width/(double)boundRect_cp[i].height;
                if(aspect_ratio<0.95 || aspect_ratio >1.05)
                {
                    if(largestContourArea == ctArea)
                        smallestContourIdx = i;
                    boundRect[i] = boundRect_cp[i];
                }

            }
            
        }
        /* Now that the bounding box is updated, draw rectangle for the smallest
            contour , only for visualisation !! */
        for( size_t i = 0; i< contours.size(); i++ )
        {
            Scalar color = Scalar( rng.uniform(0, 256), rng.uniform(0,256), 
                                   rng.uniform(0,256) );
            //drawContours( drawing, contours_poly, (int)i, color );
            if(i==smallestContourIdx)
                rectangle( drawing, boundRect[i].tl(), boundRect[i].br(), color, 2 );
        }
        // Check if quad exist, or else, discard the frame and process next
        int idx = smallestContourIdx;
        if(idx>=0)
        {
            int c;
            c = snprintf(str_tmp_det, STR_DET_LEN, fmt_det_point,boundRect[idx].x, boundRect[idx].y, 
                        boundRect[idx].x+ boundRect[idx].width, boundRect[idx].y, boundRect[idx].x+ boundRect[idx].width, 
                       boundRect[idx].y+boundRect[idx].height, boundRect[idx].x, boundRect[idx].y+boundRect[idx].height);

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
            return (uint8_t *)buffer;

        }
        else
        {
            return (uint8_t *)0;
        }
        
        
        /* On quad detection , set sampling flag to 1 so that the interrupt
            handler can begin processing */
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
                
                Mat dst,frame_grey;
                // Color to greyscale
                cvtColor(cropped_img, frame_grey, COLOR_BGR2GRAY);
                // Binary thresholing to detect bright spots in the image
                threshold(frame_grey,dst, thresh, maxValue, THRESH_BINARY);
                // Update the led status of current frame
                bool res = update_led_status(dst);
                return (uint8_t *)res;

            }
        }
    return (uint8_t *)0;
}
}
extern "C"{
EMSCRIPTEN_KEEPALIVE
bool update_led_status(Mat thresh_img )
{    
    vector<vector<Point> > contours;
    findContours(thresh_img, contours, RETR_TREE, CHAIN_APPROX_SIMPLE );
    
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

// EMSCRIPTEN_KEEPALIVE
// int main()
// {
//     // Create black empty images
//     //Mat image = Mat::zeros( 400, 400, CV_8UC3 );
//    Mat image;
//   // Draw a line 
//   //line( image, Point( 15, 20 ), Point( 70, 50), Scalar( 110, 220, 0 ),  2, 8 );

//     printf(" Hello from native code! %d\n",image.empty());
//     return 0;
// }

