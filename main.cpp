#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/video/background_segm.hpp>
using namespace std;
using namespace cv;
#define drawCross( img, center, color, d )\
line(img, Point(center.x - d, center.y - d), Point(center.x + d, center.y + d), color, 2, 0);\
line(img, Point(center.x + d, center.y - d), Point(center.x - d, center.y + d), color, 2,  0);
vector<Point> mousev, kalmanv;
cv::KalmanFilter KF;
cv::Mat_<float> measurement(2, 1);
Mat_<float> state(4, 1); // (x, y, Vx, Vy)
int incr = 0; string num2str(int i)
{ stringstream ss;
ss << i;
return ss.str(); }
void initKalman(float x, float y)
{
    // Instantate Kalman Filter with
    // 4 dynamic parameters and 2 measurement parameters,
    // where my measurement is: 2D location of object,
    // and dynamic is: 2D location and 2D velocity.
    KF.init(4, 2, 0);
    measurement = Mat_<float>::zeros(2, 1);
    measurement.at<float>(0, 0) = x;
    measurement.at<float>(0, 0) = y;
    KF.statePre.setTo(0);
    KF.statePre.at<float>(0, 0) = x;
    KF.statePre.at<float>(1, 0) = y;
    KF.statePost.setTo(0);
    KF.statePost.at<float>(0, 0) = x;
    KF.statePost.at<float>(1, 0) = y;
    setIdentity(KF.transitionMatrix);
    setIdentity(KF.measurementMatrix);
    setIdentity(KF.processNoiseCov, Scalar::all(.005));
    //adjust this for faster convergence - but higher noise
    setIdentity(KF.measurementNoiseCov, Scalar::all(1e-1));
    setIdentity(KF.errorCovPost, Scalar::all(.1));
}
Point kalmanPredict()
{
    Mat prediction = KF.predict();
    Point predictPt(prediction.at<float>(0), prediction.at<float>(1));
    KF.statePre.copyTo(KF.statePost);
    KF.errorCovPre.copyTo(KF.errorCovPost);
    return predictPt;
}

Point kalmanCorrect(float x, float y)
{
    measurement(0) = x;
    measurement(1) = y;
    Mat estimated = KF.correct(measurement);
    Point statePt(estimated.at<float>(0), estimated.at<float>(1));
    return statePt;
}
int main()
{
    Mat frame, thresh_frame;
    vector<Mat> channels;
    VideoCapture capture("/home/lenovo/CLionProjects/person/test0.avi");
    if (!capture.isOpened())
        cout << "hello";
    vector<Vec4i> hierarchy;
    vector<vector<Point> > contours; // cv::Mat frame;
    cv::Mat back,img;
    cv::Mat fore;
    cv::Ptr<BackgroundSubtractorMOG2> bg = createBackgroundSubtractorMOG2();
    //cv::BackgroundSubtractorMOG2 bg;
    // bg->setNMixtures(3);
    // bg->apply(img, mask);
    //bg.nmixtures = 3;//nmixtures
    // bg.bShadowDetection = false;
    int incr = 0;
    int track = 0;
    bool update_bg_model = true;
    //capture.open("1.avi");
    if (!capture.isOpened())
        cerr << "Problem opening video source" << endl;
    mousev.clear();
    kalmanv.clear();
    initKalman(0, 0);
    while ((char)waitKey(1) != 'q' && capture.grab()) {
        Point s, p;
        capture.retrieve(frame);
        //bg(img, fgmask, update_bg_model ? -1 : 0);
        // bg->operator ()(frame, fore);
        bg->apply(frame, fore, update_bg_model ? -1 : 0);
        bg->getBackgroundImage(back);
        erode(fore, fore, Mat());
        erode(fore, fore, Mat());
        dilate(fore, fore, Mat());
        dilate(fore, fore, Mat());
        dilate(fore, fore, Mat());
        dilate(fore, fore, Mat());
        dilate(fore, fore, Mat());
        dilate(fore, fore, Mat());
        dilate(fore, fore, Mat());
        cv::normalize(fore, fore, 0, 1., cv::NORM_MINMAX);
        cv::threshold(fore, fore, .5, 1., CV_THRESH_BINARY);
        split(frame, channels);
        add(channels[0], channels[1], channels[1]);
        subtract(channels[2], channels[1], channels[2]);
        threshold(channels[2], thresh_frame, 50, 255, CV_THRESH_BINARY);
        medianBlur(thresh_frame, thresh_frame, 5); //
        // imshow("Red", channels[1]);
        findContours(fore, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
        vector<vector<Point> > contours_poly(contours.size());
        vector<Rect> boundRect(contours.size());
        Mat drawing = Mat::zeros(thresh_frame.size(), CV_8UC1);
        for (size_t i = 0; i < contours.size(); i++) {
            // cout << contourArea(contours[i]) << endl;
            if (contourArea(contours[i]) > 500)
                drawContours(drawing, contours, i, Scalar::all(255), CV_FILLED, 8, vector<Vec4i>(), 0, Point());
        }
        thresh_frame = drawing;
        findContours(fore, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
        drawing = Mat::zeros(thresh_frame.size(), CV_8UC1);
        for (size_t i = 0; i < contours.size(); i++) {
            // cout << contourArea(contours[i]) << endl;
            if (contourArea(contours[i]) > 3000)
                drawContours(drawing, contours, i, Scalar::all(255), CV_FILLED, 8, vector<Vec4i>(), 0, Point());
        }
        thresh_frame = drawing;
        // Get the moments
        vector<Moments> mu(contours.size());
        for (size_t i = 0; i < contours.size(); i++) {
            mu[i] = moments(contours[i], false);
        } //  Get the mass centers:
        vector<Point2f> mc(contours.size());
        for (size_t i = 0; i < contours.size(); i++) {
            mc[i] = Point2f(mu[i].m10 / mu[i].m00, mu[i].m01 / mu[i].m00);
        }
        for (size_t i = 0; i < contours.size(); i++) {
            approxPolyDP(Mat(contours[i]), contours_poly[i], 3, true);
            boundRect[i] = boundingRect(Mat(contours_poly[i]));
        }
        p = kalmanPredict();
        // cout << "kalman prediction: " << p.x << " " << p.y << endl;
        mousev.push_back(p);
        string text;
        for (size_t i = 0; i < contours.size(); i++) {
            if (contourArea(contours[i]) > 1000) {
                rectangle(frame, boundRect[i].tl(), boundRect[i].br(), Scalar(0, 255, 0), 2, 8, 0);
                Point pt = Point(boundRect[i].x, boundRect[i].y + boundRect[i].height + 15);
                Point pt1 = Point(boundRect[i].x, boundRect[i].y - 10);
                Point center = Point(boundRect[i].x + (boundRect[i].width / 2),
                                     boundRect[i].y + (boundRect[i].height / 2));
                cv::circle(frame, center, 8, Scalar(0, 0, 255), -1, 1, 0);
                Scalar color = CV_RGB(255, 0, 0);
                text = num2str(boundRect[i].width) + "*" + num2str(boundRect[i].height);
                putText(frame, "car", pt, CV_FONT_HERSHEY_DUPLEX, 1.0f, color);
                putText(frame, text, pt1, CV_FONT_HERSHEY_DUPLEX, 1.0f, color);
                s = kalmanCorrect(center.x, center.y);
                drawCross(frame, s, Scalar(255, 255, 255), 5);
                if (s.y <= 130 && p.y > 130 && s.x > 15) {
                    incr++;
                    cout << "Counter " << incr << endl;
                }
                //cout << "Counter " << contours.size() << endl;
                for (int i = mousev.size() - 20; i < mousev.size() - 1; i++) {
                    line(frame, mousev[i], mousev[i + 1], Scalar(0, 255, 0), 1);
                }
            }

            imshow("Video", frame);
            //	imshow("Red", channels[2]);
            // 	imshow("Binary", thresh_frame);
        }
    }
    return 0;
}
