/*
 * Processor.cpp
 *
 *  Created on: Jun 13, 2010
 *      Author: ethan
 */

#include "Processor.h"


#include <sys/stat.h>

using namespace cv;

Processor::Processor() :
    stard(20 /*max_size*/,
          8  /*response_threshold*/,
          15 /*line_threshold_projected*/,
          8  /*line_threshold_binarized*/,
          5  /*suppress_nonmax_size*/),
    fastd(20   /*threshold*/,
          true /*nonmax_suppression*/),
    surfd(100. /*hessian_threshold*/,
          1    /*octaves*/,
          2    /*octave_layers*/)
//			surfd(  30, //int _maxCorners,
//					0.02, //double _qualityLevel,
//                    3, //double _minDistance,
//                    3, //int _blockSize=3,
//                    false, //bool _useHarrisDetector=false,
//                    0.04  //double _k=0.04
//					)

{

}

Processor::~Processor() {
    // TODO Auto-generated destructor stub
}

void Processor::detectAndDrawFeatures(int input_idx, image_pool* pool, int feature_type) {
    FeatureDetector* fd = 0;

    switch (feature_type) {
    case DETECT_SURF:
        fd = &surfd;
        break;
    case DETECT_FAST:
        fd = &fastd;
        break;
    case DETECT_STAR:
        fd = &stard;
        break;
    }

    Mat greyimage;
    pool->getGrey(input_idx, greyimage);

    Mat* img = pool->getImage(input_idx);

    if (!img || greyimage.empty() || fd == 0) { return; } // no image at input_idx!

    keypoints.clear();

    fd->detect(greyimage, keypoints);

    for (vector<KeyPoint>::const_iterator it = keypoints.begin(); it != keypoints.end(); ++it) {
        circle(*img, it->pt, 3, cvScalar(255, 0, 255, 0));
    }


}

static double computeReprojectionErrors(
    const vector<vector<Point3f> >& objectPoints, const vector<vector<Point2f> >& imagePoints, const vector<Mat>& rvecs,
    const vector<Mat>& tvecs, const Mat& cameraMatrix,
    const Mat& distCoeffs, vector<float>& perViewErrors) {
    vector<Point2f> imagePoints2;
    int i, totalPoints = 0;
    double totalErr = 0, err;
    perViewErrors.resize(objectPoints.size());

    for (i = 0; i < (int) objectPoints.size(); i++) {
        projectPoints(Mat(objectPoints[i]), rvecs[i], tvecs[i], cameraMatrix, distCoeffs, imagePoints2);
        err = norm(Mat(imagePoints[i]), Mat(imagePoints2), CV_L1);
        int n = (int) objectPoints[i].size();
        perViewErrors[i] = err / n;
        totalErr += err;
        totalPoints += n;
    }

    return totalErr / totalPoints;
}

static void calcChessboardCorners(Size boardSize, float squareSize, vector<Point3f>& corners) {
    corners.resize(0);

    for (int i = 0; i < boardSize.height; i++)
        for (int j = 0; j < boardSize.width; j++)
            corners.push_back(Point3f(float(j * squareSize), float(i
                                      * squareSize), 0));
}

/**from opencv/samples/cpp/calibration.cpp
 *
 */
static bool runCalibration(vector<vector<Point2f> > imagePoints,
                           Size imageSize, Size boardSize, float squareSize, float aspectRatio,
                           int flags, Mat& cameraMatrix, Mat& distCoeffs, vector<Mat>& rvecs,
                           vector<Mat>& tvecs, vector<float>& reprojErrs, double& totalAvgErr) {
    cameraMatrix = Mat::eye(3, 3, CV_64F);
    if (flags & CV_CALIB_FIX_ASPECT_RATIO) {
        cameraMatrix.at<double> (0, 0) = aspectRatio;
    }

    distCoeffs = Mat::zeros(5, 1, CV_64F);

    vector<vector<Point3f> > objectPoints(1);
    calcChessboardCorners(boardSize, squareSize, objectPoints[0]);
    for (size_t i = 1; i < imagePoints.size(); i++) {
        objectPoints.push_back(objectPoints[0]);
    }

    calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix,
                    distCoeffs, rvecs, tvecs, flags);

    bool ok = checkRange(cameraMatrix, CV_CHECK_QUIET) && checkRange(
                  distCoeffs, CV_CHECK_QUIET);

    totalAvgErr = computeReprojectionErrors(objectPoints, imagePoints, rvecs,
                                            tvecs, cameraMatrix, distCoeffs, reprojErrs);

    return ok;
}

bool Processor::detectAndDrawChessboard(int idx, image_pool* pool) {

    Mat grey;
    pool->getGrey(idx, grey);
    if (grey.empty()) {
        return false;
    }
    vector<Point2f> corners;

    IplImage iplgrey = grey;
    if (!cvCheckChessboard(&iplgrey, Size(6, 8))) {
        return false;
    }
    bool patternfound = findChessboardCorners(grey, Size(6, 8), corners);

    Mat* img = pool->getImage(idx);

    if (corners.size() < 1) {
        return false;
    }

    cornerSubPix(grey, corners, Size(11, 11), Size(-1, -1), TermCriteria(
                     CV_TERMCRIT_EPS + CV_TERMCRIT_ITER, 30, 0.1));

    if (patternfound) {
        imagepoints.push_back(corners);
    }

    drawChessboardCorners(*img, Size(6, 8), Mat(corners), patternfound);

    imgsize = grey.size();

    return patternfound;

}

void Processor::drawText(int i, image_pool* pool, const char* ctext) {
    // Use "y" to show that the baseLine is about
    string text = ctext;
    int fontFace = FONT_HERSHEY_COMPLEX_SMALL;
    double fontScale = .8;
    int thickness = .5;

    Mat img = *pool->getImage(i);

    int baseline = 0;
    Size textSize = getTextSize(text, fontFace,
                                fontScale, thickness, &baseline);
    baseline += thickness;

    // center the text
    Point textOrg((img.cols - textSize.width) / 2,
                  (img.rows - textSize.height * 2));

    // draw the box
    rectangle(img, textOrg + Point(0, baseline),
              textOrg + Point(textSize.width, -textSize.height),
              Scalar(0, 0, 255), CV_FILLED);
    // ... and the baseline first
    line(img, textOrg + Point(0, thickness),
         textOrg + Point(textSize.width, thickness),
         Scalar(0, 0, 255));

    // then put the text itself
    putText(img, text, textOrg, fontFace, fontScale,
            Scalar::all(255), thickness, 8);
}
void saveCameraParams(const string& filename, Size imageSize, Size boardSize,
                      float squareSize, float aspectRatio, int flags,
                      const Mat& cameraMatrix, const Mat& distCoeffs,
                      const vector<Mat>& rvecs, const vector<Mat>& tvecs,
                      const vector<float>& reprojErrs,
                      const vector<vector<Point2f> >& imagePoints, double totalAvgErr) {
    FileStorage fs(filename, FileStorage::WRITE);

    time_t t;
    time(&t);
    struct tm* t2 = localtime(&t);
    char buf[1024];
    strftime(buf, sizeof(buf) - 1, "%c", t2);

    fs << "calibration_time" << buf;

    if (!rvecs.empty() || !reprojErrs.empty()) {
        fs << "nframes" << (int) std::max(rvecs.size(), reprojErrs.size());
    }
    fs << "image_width" << imageSize.width;
    fs << "image_height" << imageSize.height;
    fs << "board_width" << boardSize.width;
    fs << "board_height" << boardSize.height;
    fs << "squareSize" << squareSize;

    if (flags & CV_CALIB_FIX_ASPECT_RATIO) {
        fs << "aspectRatio" << aspectRatio;
    }

    if (flags != 0) {
        sprintf(buf, "flags: %s%s%s%s",
                flags & CV_CALIB_USE_INTRINSIC_GUESS ? "+use_intrinsic_guess"
                : "",
                flags & CV_CALIB_FIX_ASPECT_RATIO ? "+fix_aspectRatio" : "",
                flags & CV_CALIB_FIX_PRINCIPAL_POINT ? "+fix_principal_point"
                : "",
                flags & CV_CALIB_ZERO_TANGENT_DIST ? "+zero_tangent_dist" : "");
        cvWriteComment(*fs, buf, 0);
    }

    fs << "flags" << flags;

    fs << "camera_matrix" << cameraMatrix;
    fs << "distortion_coefficients" << distCoeffs;

    fs << "avg_reprojection_error" << totalAvgErr;
    if (!reprojErrs.empty()) {
        fs << "per_view_reprojection_errors" << Mat(reprojErrs);
    }

    if (!rvecs.empty() && !tvecs.empty()) {
        Mat bigmat(rvecs.size(), 6, CV_32F);
        for (size_t i = 0; i < rvecs.size(); i++) {
            Mat r = bigmat(Range(i, i + 1), Range(0, 3));
            Mat t = bigmat(Range(i, i + 1), Range(3, 6));
            rvecs[i].copyTo(r);
            tvecs[i].copyTo(t);
        }
        cvWriteComment(
            *fs,
            "a set of 6-tuples (rotation vector + translation vector) for each view",
            0);
        fs << "extrinsic_parameters" << bigmat;
    }

    if (!imagePoints.empty()) {
        Mat imagePtMat(imagePoints.size(), imagePoints[0].size(), CV_32FC2);
        for (size_t i = 0; i < imagePoints.size(); i++) {
            Mat r = imagePtMat.row(i).reshape(2, imagePtMat.cols);
            Mat(imagePoints[i]).copyTo(r);
        }
        fs << "image_points" << imagePtMat;
    }
}
void Processor::resetChess() {

    imagepoints.clear();
}

void Processor::calibrate(const char* filename) {

    vector<Mat> rvecs, tvecs;
    vector<float> reprojErrs;
    double totalAvgErr = 0;
    int flags = 0;
    bool writeExtrinsics = true;
    bool writePoints = true;

    bool ok = runCalibration(imagepoints, imgsize, Size(6, 8), 1.f, 1.f,
                             flags, K, distortion, rvecs, tvecs, reprojErrs, totalAvgErr);


    if (ok) {

        saveCameraParams(filename, imgsize, Size(6, 8), 1.f,
                         1.f, flags, K, distortion, writeExtrinsics ? rvecs
                         : vector<Mat> (), writeExtrinsics ? tvecs
                         : vector<Mat> (), writeExtrinsics ? reprojErrs
                         : vector<float> (), writePoints ? imagepoints : vector <
                         vector<Point2f> > (), totalAvgErr);
    }

}

int Processor::getNumberDetectedChessboards() {
    return imagepoints.size();
}

/* =================================================================== */

const int thresh = 50;
const int N = 3;//11;


// helper function:
// finds a cosine of angle between vectors
// from pt0->pt1 and from pt0->pt2
float angle(Point pt1, Point pt2, Point pt0) {
    float dx1 = pt1.x - pt0.x;
    float dy1 = pt1.y - pt0.y;
    float dx2 = pt2.x - pt0.x;
    float dy2 = pt2.y - pt0.y;
    return (dx1 * dx2 + dy1 * dy2) / sqrt((dx1 * dx1 + dy1 * dy1) * (dx2 * dx2 + dy2 * dy2) + 1e-9);
}

// returns sequence of squares detected on the image.
// the sequence is stored in the specified memory storage
void findSquares(const Mat& image, vector<vector<Point> >& squares) {
    squares.clear();

    Mat pyr, timg, gray0(image.size(), CV_8U), gray;

    // down-scale and upscale the image to filter out the noise
    pyrDown(image, pyr, Size(image.cols / 2, image.rows / 2));
    pyrUp(pyr, timg, image.size());
    vector<vector<Point> > contours;

    // find squares in every color plane of the image
    for (int c = 0; c < 3; ++c) {
        int ch[] = {c, 0};
        mixChannels(&timg, 1, &gray0, 1, ch, 1);

        // try several threshold levels
        for (int l = 0; l < N; ++l) {
            // hack: use Canny instead of zero threshold level.
            // Canny helps to catch squares with gradient shading
            if (l == 0) {
                // apply Canny. Take the upper threshold from slider
                // and set the lower to 0 (which forces edges merging)
                Canny(gray0, gray, 0, thresh, 5);
                // dilate canny output to remove potential
                // holes between edge segments
                dilate(gray, gray, Mat(), Point(-1, -1));
            } else {
                // apply threshold if l!=0:
                //     tgray(x,y) = gray(x,y) < (l+1)*255/N ? 255 : 0
                gray = gray0 >= (l + 1) * 255 / N;
            }

            // find contours and store them all as a list
            findContours(gray, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);

            vector<Point> approx;

            // test each contour
            for (size_t i = 0; i < contours.size(); ++i) {
                // approximate contour with accuracy proportional
                // to the contour perimeter
                approxPolyDP(Mat(contours[i]), approx, arcLength(Mat(contours[i]), true) * 0.02, true);

                // square contours should have 4 vertices after approximation
                // relatively large area (to filter out noisy contours)
                // and be convex.
                // Note: absolute value of an area is used because
                // area may be positive or negative - in accordance with the contour orientation
                if (approx.size() == 4 &&
                        fabs(contourArea(Mat(approx))) > 500 &&
                        isContourConvex(Mat(approx))) {
                    float maxCosine = 0;

                    for (int j = 2; j < 5; ++j) {
                        // find the maximum cosine of the angle between joint edges
                        float cosine = fabs(angle(approx[j%4], approx[j-2], approx[j-1]));
                        maxCosine = MAX(maxCosine, cosine);
                    }

                    // if cosines of all angles are small
                    // (all angles are ~90 degree) then write quandrange
                    // vertices to resultant sequence
                    if (maxCosine < 0.3) {
                        squares.push_back(approx);
                    }
                }
            }
        }
    }
}

// the function draws all the squares in the image
void drawSquares(Mat& image, const vector<vector<Point> >& squares) {
    for (size_t i = 0; i < squares.size(); ++i) {
        const Point* p = &squares[i][0];
        int n = (int)squares[i].size();
        polylines(image, &p, &n, 1, true, Scalar(0, 255, 0), 3, CV_AA);
    }

}


void Processor::detectAndDrawContours(int input_idx, image_pool* pool) {


    Mat* img;
    img = pool->getImage(input_idx);
    if (!img) { return; } // no image at input_idx!

//	if(0)
//	{
//		Mat greyimage;
//		pool->getGrey(input_idx, greyimage);
//		threshold(greyimage, greyimage, 128, 255, THRESH_BINARY);
//		vector<vector<Point> > contours;
//		vector<Vec4i> hierarchy;
//		findContours( greyimage, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE );
//		// iterate through all the top-level contours, draw each connected component with its own random color
//		int idx = 0;
//		for( ; idx >= 0; idx = hierarchy[idx][0] )	{
//			Scalar color( rand()&255, rand()&255, rand()&255 );
//			drawContours( *img, contours, idx, color, 2, 8, hierarchy, 0);
//		}
//	}
//	else
    {
        vector<vector<Point> > squares;
        findSquares(*img, squares);
        drawSquares(*img, squares);
    }


}



