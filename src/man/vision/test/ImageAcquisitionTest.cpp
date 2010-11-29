#include "ImageAcquisitionTest.h"

#include "ColorParams.h"
#include "VisionDef.h"

#include <stdlib.h>
#include <cstdio>

#include <iostream>
#include <fstream>

using namespace std;

typedef unsigned char uchar;

// Our function to test!
extern "C" void _acquire_image(uchar * colorTable, ColorParams* params,
                               uchar* yuvImage, uchar* outImage);

ImageAcquisitionTest::ImageAcquisitionTest() :
    c(0,0,0, 256, 256, 256, 128, 128, 128) // Default old table size
{
    init();
    setup();
}

ImageAcquisitionTest::~ImageAcquisitionTest()
{
    deallocate();
}

void ImageAcquisitionTest::init()
{
    table = yuv = yuvCopy = out = NULL;
}

void ImageAcquisitionTest::setup()
{
    allocate();
    c = ColorParams(54, 24, 100,
                    253, 25, 140,
                    100,10,12);
    c.printParams();

    // Seed the random number generator
    srand ( time(NULL) );

    // Fill the table and images with random values
    for (int i = 0; i < tableMaxSize; ++i) {
        table[i] = static_cast<uchar>(rand()% 255);
    }

    for (int i = 0; i < yuvImgSize; ++i) {
        yuv[i] = static_cast<uchar>(rand() % 255);
    }

    for (int i = 0; i < yuvImgSize; ++i) {
        yuvCopy[i] = yuv[i];
    }

}

/**
 * Allocate memory for the table and image arrays
 */
void ImageAcquisitionTest::allocate()
{
    if (table == NULL){
        table = new uchar[tableMaxSize];
    }

    if (yuv == NULL){
        yuv = new uchar[yuvImgSize];
    }

    // Copy the image for later checking
    if (yuvCopy == NULL){
        yuvCopy = new uchar[yuvImgSize];
    }

    // Allocate the output image.
    if (out == NULL){
        out = new uchar[outImgSize];
    }
}

/**
 * Free the memory from the images and tables
 */
void ImageAcquisitionTest::deallocate()
{
    delete table;
    delete yuv;
    delete yuvCopy;
    delete out;
}

/**
 * @param i Row of pixel in output image
 * @param j Column of pixel in output image
 * @return Averaged value of 4 y pixels around given (i,j) location
 */
int ImageAcquisitionTest::yAvgValue(int i, int j) const
{
    // Get location of pixel in full size (double width) YUV image x4
    // is for 2 bytes per pixel and double YUV image size compared to
    // output image
    uchar* p = yuv + 4 * (i * yuvImgWidth + j);
    return (static_cast<int>(p[0]) +
            static_cast<int>(p[2]) +
            static_cast<int>(p[yuvImgWidth * 2]) +
            static_cast<int>(p[yuvImgWidth * 2 + 2]));
}

/**
 * @param i Row of pixel in output image
 * @param j Column of pixel in output image
 * @return Averaged value of 2 u pixels (at and below) given (i,j) location
 */
int ImageAcquisitionTest::uAvgValue(int i, int j) const
{
    uchar * p = yuv + 4 * (i * yuvImgWidth + j);
    return (static_cast<int>(p[1]) +
            static_cast<int>(p[yuvImgWidth * 2 + 1]));
}

/**
 * @param i Row of pixel in output image
 * @param j Column of pixel in output image
 * @return Averaged value of 2 v pixels (at and below) given (i,j) location
 */
int ImageAcquisitionTest::vAvgValue(int i, int j) const
{
    uchar * p = yuv + 4 * (i * yuvImgWidth + j);
    return (static_cast<int>(p[3]) +
            static_cast<int>(p[yuvImgWidth * 2 + 3]));
}


/**
 * Find the color table y index of a pixel using the ColorParams space values
 *
 * @param i Row of pixel in output image
 * @param j Column of pixel in output image
 * @return Y index of given (i,j) pixel in color table space
 */
int ImageAcquisitionTest::yIndex(int i, int j) const
{
    int y = max( yAvgValue(i,j) - yZero(), 0);
    y = min( (y * ySlope()) >> 16 , yLimit());
    return y;
}

/**
 * See yIndex for details. Does same but for u index.
 */
int ImageAcquisitionTest::uIndex(int i, int j) const
{
    int u = max( uAvgValue(i, j) - uZero(), 0);
    u = min( (u * uSlope()) >> 16 , uLimit());
    return u;
}

/**
 * See yIndex for details. Does same but for v index.
 */
int ImageAcquisitionTest::vIndex(int i, int j) const
{
    int v = max( vAvgValue(i, j) - vZero(), 0);
    v = min( (v * vSlope()) >> 16 , vLimit());
    return v;
}

/**
 * @return Color in table according to given YUV values.
 */
int ImageAcquisitionTest::tableLookup(int y, int u, int v) const
{
    int index = y +
        u * static_cast<int>(c.uvDim & 0xFFFF) +
        v * static_cast<int>( (c.uvDim >> 16) & 0xFFFF);
return table[index];
}

/**
 * @param i Row in output image of pixel
 * @param j Column in output image of given pixel
 * @return Color in output image at given location.
 */
int ImageAcquisitionTest::colorValue(int i, int j) const
{
    // Color image is just past Y Image in array
    return out[outImgWidth * i + j + outImgYSize];
}

void ImageAcquisitionTest::test_color_segmentation()
{
    for (int i=0; i < outImgHeight; ++i){
        for (int j=0; j < outImgWidth; ++j){
            EQ_INT( colorValue(i,j) ,
                    tableLookup( yIndex(i,j),
                                 uIndex(i,j),
                                 vIndex(i,j) ));
        }
    }
    PASSED(COLOR_SEGMENTATION);
}

/**
 * Test the averaging of the pixels by the image acquistion.
 */
void ImageAcquisitionTest::test_avg()
{

    // Run it!
    _acquire_image(table, &c, yuv, out);

    // Make sure that the run didn't affect the initial image
    for (int i = 0; i < 640*480*2; ++i) {
        EQ_INT((int)yuvCopy[i] , (int)yuv[i]);
    }
    PASSED(PRESERVE_IMAGE);

    // Check that the average works properly.
    for (int i = 0; i < outImgHeight; ++i) {
        for (int j=0; j < outImgWidth; ++j){

            int output = (int)out[i*outImgWidth + j];
            EQ_INT( yAvgValue(i, j) >> 2, output);
        }
    }
    PASSED(AVERAGES);
}

int ImageAcquisitionTest::runTests()
{
    test_avg();
    test_color_segmentation();
    return 0;
}

int main(int argc, char * argv[])
{
    ImageAcquisitionTest * iat = new ImageAcquisitionTest();
    return iat->runTests();
}
